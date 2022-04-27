#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/mutex.h>

#include "chardev.h"
#include "cryptocard_user.h"


static int cryptdev_open(struct inode *inode, struct file *file);
static int cryptdev_release(struct inode *inode, struct file *file);
static ssize_t cryptdev_read(struct file *filp,
                             char *buffer,
                             size_t length,
                             loff_t * offset);
static ssize_t cryptdev_write(struct file *filp,
                              const char __user *buffer,
                              size_t length,
                              loff_t * offset);
static long cryptdev_ioctl(struct file *filp, 
                         unsigned int cmd, 
                         unsigned long arg);
static int cryptdev_mmap(struct file *filp, struct vm_area_struct *vma);

static char *cryptnode_devnode(struct device *dev, umode_t *mode);


atomic_t  device_opened;
static int major;
static struct class *cryptdev_class;
static struct device *cryptdev_device;
struct driver_pvt* drv_pvt_exposed;

static const struct file_operations cryptdev_fops = {
	.owner          = THIS_MODULE,
	.open           = cryptdev_open,
	.release        = cryptdev_release,
	.read           = cryptdev_read, /* for enctyption */
	.write          = cryptdev_write, /* for decryption */
	.unlocked_ioctl = cryptdev_ioctl, /* for config modifications */
    .mmap           = cryptdev_mmap,
};


static char *cryptnode_devnode(struct device *dev, umode_t *mode)
{
        if (mode && dev->devt == MKDEV(major, 0))
                *mode = 0666;
        return NULL;
}

int create_char_dev(struct driver_pvt* drv_pvt)
{
    int err;
    major = register_chrdev(0, DEVNAME, &cryptdev_fops);
    err = major;
    if (err < 0)
    {
        printk(KERN_ALERT "Registering char device failed with %d\n", major);
        goto error_regdev;
    }    
    cryptdev_class = class_create(THIS_MODULE, DEVNAME);
    err = PTR_ERR(cryptdev_class);
    if (IS_ERR(cryptdev_class))
            goto error_class;

    cryptdev_class->devnode = cryptnode_devnode;

    cryptdev_device = device_create(cryptdev_class, NULL,
                                    MKDEV(major, 0),
                                    NULL, DEVNAME);
    err = PTR_ERR(cryptdev_device);
    if (IS_ERR(cryptdev_device))
            goto error_device;

    printk(KERN_INFO "I was assigned major number %d. To talk to\n", major);
    atomic_set(&device_opened, 0);

    drv_pvt_exposed = drv_pvt;

	return 0;

error_device:
    class_destroy(cryptdev_class);
error_class:
    unregister_chrdev(major, DEVNAME);
error_regdev:
    return err;
}

void cleanup_char_dev(void)
{
    device_destroy(cryptdev_class, MKDEV(major, 0));
    class_destroy(cryptdev_class);
    unregister_chrdev(major, DEVNAME);
    printk(KERN_INFO "Deregistered char dev\n");
}

/* Concurrency safe */
static int cryptdev_open(struct inode *inode, struct file *file)
{
    unsigned int minor;
    struct crypt_device_private *dev_pvt;
    atomic_inc(&device_opened);
    // try_module_get(THIS_MODULE);
    
    minor = iminor(inode);
    dev_pvt = kmalloc(sizeof(struct crypt_device_private), GFP_KERNEL);

    if(!dev_pvt)
        return -ENOMEM;

    // dev_pvt->chnum = minor;
    dev_pvt->drv_pvt = drv_pvt_exposed;
    dev_pvt->config = drv_pvt_exposed->config;
    
    file->private_data = (void *)dev_pvt;
    // printk(KERN_INFO "Device opened successfully\n");

    return 0;
}

/* Concurrency safe */
static int cryptdev_release(struct inode *inode, struct file *file)
{
    struct crypt_device_private *dev_pvt;
    atomic_dec(&device_opened);
    // module_put(THIS_MODULE);

    dev_pvt = (struct crypt_device_private *) file->private_data;
    kfree(dev_pvt);
    // printk(KERN_INFO "Device closed successfully\n");

    return 0;
}


/* to be used for encrypt */
static ssize_t cryptdev_read(struct file *filp,
                           char __user *buffer,
                           size_t length,
                           loff_t * offset)
{   
    int err;
    struct crypt_device_private *dev_pvt;
    struct mutex *dev_mutex;

    dev_pvt = (struct crypt_device_private *) filp->private_data;
    dev_mutex = &dev_pvt->drv_pvt->dev_mutex;
    err = 0;
    
    err = mutex_lock_killable(dev_mutex);

    if(err)
    {
        printk("Kill signal recieved while waiting for lock\n");
        return err;
    }

    /* Critical Section start */
    dev_pvt->drv_pvt->config = dev_pvt->config;
    if(dev_pvt->config.use_dma == YES)
        err = dma_op(buffer, length, ENCR, dev_pvt->drv_pvt);
    else
        err = mimo_op(buffer, length, ENCR, dev_pvt->drv_pvt);
    /* Critical Section end */

    if(err)
        printk(KERN_ERR "Error while encrytping\n");

    mutex_unlock(dev_mutex);
    return err;
}

static ssize_t cryptdev_write(struct file *filp,
                           const  char __user *buffer,
                           size_t length,
                           loff_t * offset)
{
           
    int err;
    struct crypt_device_private *dev_pvt;
    struct mutex *dev_mutex;

    dev_pvt = (struct crypt_device_private *) filp->private_data;
    dev_mutex = &dev_pvt->drv_pvt->dev_mutex;
    
    err = 0;

    err = mutex_lock_killable(dev_mutex);
    if(err)
    {
        printk("Kill signal recieved while waiting for lock\n");
        return err;
    }

    /* Critical Section start */
    dev_pvt->drv_pvt->config = dev_pvt->config;
    if(dev_pvt->config.use_dma == YES)
        err = dma_op(buffer, length, DECR, dev_pvt->drv_pvt);
    else
        err = mimo_op(buffer, length, DECR, dev_pvt->drv_pvt);
    /* Critical Section end */

    if(err)
        printk(KERN_ERR "Error while decrypting\n");

    mutex_unlock(dev_mutex);
    
    return err;
}

static int cryptdev_mmap(struct file *filp, struct vm_area_struct *vma)
{
    struct driver_pvt *drv_pvt = ((struct crypt_device_private *)filp->private_data)->drv_pvt;
    int err;
    unsigned long phy = drv_pvt->mmio_start;
    unsigned long vsize = vma->vm_end - vma->vm_start;
    unsigned long psize = drv_pvt->mmio_len;

    vma->vm_flags |= VM_IO | VM_PFNMAP;

    // printk(KERN_INFO "phy base: 0x%x\n", drv_pvt->mmio_start);

    if(vsize > psize)
    {
        printk(KERN_INFO "Tried to map more device memory than availaible\n");
        return -EINVAL;
    }

    if(io_remap_pfn_range(vma, 
                          vma->vm_start, 
                          phy >> PAGE_SHIFT,
                          vsize,
                          vma->vm_page_prot)
    )
    {
        printk(KERN_INFO "IO remap: FAILED\n");
        return -ERESTART;
    }
    printk(KERN_INFO "IO remap: SUCCESS\n");
    return 0;
}

static long cryptdev_ioctl(struct file *filp, 
                         unsigned int cmd, 
                         unsigned long arg)
{
    struct crypt_device_private *dev_pvt;
    // printk(KERN_INFO "In cryptdev_ioctl\n");

    dev_pvt = (struct crypt_device_private *) filp->private_data;

    switch (cmd)
    {
    case IOCTL_SET_KEY:
        // printk(KERN_INFO "Setting key\n");
        dev_pvt->config.key = arg;
        break;
    case IOCTL_SET_USE_DMA:
        // printk(KERN_INFO "Setting use_dma=%lu\n", arg);
        dev_pvt->config.use_dma = arg;
        break;
    case IOCTL_SET_USE_INT:
        // printk(KERN_INFO "Setting use_intr=%lu\n", arg);
        dev_pvt->config.use_intr = arg;
        break;
    default:
        printk(KERN_ALERT "Invalid cmd to ioctl: 0x%x\n", cmd);
        return -EINVAL;
    }
    return 0;
}
