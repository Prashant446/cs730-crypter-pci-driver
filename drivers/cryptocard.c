#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/pci.h>
#include<linux/wait.h>
#include<linux/device.h>
#include<linux/mutex.h>

#include "cryptocard.h"
#include "chardev.h"
#include "core.h"
#include "cryptocard_regs.h"


static int on_device_load(struct pci_dev *dev, const struct pci_device_id *id);
static void on_device_unload(struct pci_dev *dev);
// todo: add others if required

// helpers
static int set_interrupts(struct pci_dev *pdev);

// test funcs
static void print_dev_identification(struct pci_dev *pdev);
// static void liveness_check(struct pci_dev *pdev);
// 

const struct cryptdev_config default_config = {
    .use_dma = 0,
    .use_intr= 0,
    .key = 0,
};

static const struct pci_device_id id_table[2] = {{PCI_DEVICE(0x1234, 0xdeba)}};
MODULE_DEVICE_TABLE(pci, id_table);

static struct pci_driver driver = {
    .name = DRIVER,
    .id_table = id_table,
    .probe = on_device_load,
    .remove = on_device_unload,
};


/* driver test functions */
static void print_dev_identification(struct pci_dev *pdev)
{
    struct driver_pvt *drv_pvt = pci_get_drvdata(pdev);
    u8* base = drv_pvt->hwmem;
    printk(KERN_INFO "Device identification: %x", myioread32(base, REG_IDENT));
}

// static void liveness_check(struct pci_dev *pdev)
// {
//     u8* base = get_base_addr(pdev);
//     u32 val = 0x44644646U;
//     char *res;
//     myiowrite32(val, base, REG_LIVE);
//     res = myioread32(base, REG_LIVE) == (~val) ? "Passes" : "Failed";
//     printk(KERN_INFO "Device liveness check: %s\n", res);
// }

/* driver test functions */


static int on_device_load(struct pci_dev *pdev, const struct pci_device_id *id)
{
    int bar, err;
    u16 vendor, device;
    struct driver_pvt *drv_pvt;

    /* Let's read data from the PCI device configuration registers */
    pci_read_config_word(pdev, PCI_VENDOR_ID, &vendor);
    pci_read_config_word(pdev, PCI_DEVICE_ID, &device);

    printk(KERN_INFO "Device vid: 0x%X pid: 0x%X\n", vendor, device);
    printk(KERN_INFO "loaded device: %s\n", pci_name(pdev));

    err = pci_enable_device(pdev);
    if(err < 0)
    {
        INFO("");
        printk(KERN_ERR "Can't enable device %x:%x\n", pdev->vendor, pdev->device);
        goto error_enable;
    }

    pci_set_master(pdev);

    if(pci_try_set_mwi(pdev))
    {
        INFO("");
        printk(KERN_ERR "Can't set mwi for device %x:%x\n", pdev->vendor, pdev->device);
    }
    
    /* Request IO BAR */
    bar = pci_select_bars(pdev, IORESOURCE_MEM);

    /* Enable device memory */
    err = pci_enable_device_mem(pdev);

    if(err < 0)
     goto error_enable_mem;

    /* Request memory region for the BAR */
    err = pci_request_region(pdev, bar, DRIVER);

    if (err < 0)
        goto error_req_region;

     /* Allocate memory for the driver private data */
    drv_pvt = kzalloc(sizeof(struct driver_pvt), GFP_KERNEL);


    /* Get start and stop memory offsets */
    drv_pvt->mmio_start = pci_resource_start(pdev, 0);
    drv_pvt->mmio_len = pci_resource_len(pdev, 0);
    drv_pvt->mmio_flags = pci_resource_flags(pdev, 0);

    // printk("mmio: %lx - %lx (len: %ld)\n", mmio_start, mmio_start + mmio_len, mmio_len);

    if(!drv_pvt)
    {
        err = ENOMEM;
        goto error_kmalloc;
    }
    
     /* Remap BAR to the local pointer */
    drv_pvt->hwmem = ioremap(drv_pvt->mmio_start, drv_pvt->mmio_len);
    if (!drv_pvt->hwmem) 
    {
       err = EIO;
       goto error_ioremap;
    }
    
    if(dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64)))
    {
        drv_pvt->dma_enabled = 0;
        printk(KERN_WARNING "DMA: not availaible");
    }
    else
    {
        drv_pvt->dma_enabled = 1;
        printk(KERN_INFO "DMA: availaible");
    }
    
    /* initialise wait queue */
    init_waitqueue_head(&drv_pvt->wq);

    drv_pvt->config = default_config;
    drv_pvt->pdev = pdev;

    /* initialise mutex */
    mutex_init(&drv_pvt->dev_mutex);

    /* Set driver private data */
    /* Now we can access mapped "hwmem" from the any driver's function */
    pci_set_drvdata(pdev, drv_pvt);

    
    print_dev_identification(pdev);
    // liveness_check(pdev);

    err = set_interrupts(pdev);

    if(err < 0)
        goto error_int;

    create_char_dev(drv_pvt);

    printk(KERN_INFO "Complete setting up device: %s\n", pci_name(pdev));
    return 0;

error_int:
    iounmap(drv_pvt->hwmem);
error_ioremap:
error_kmalloc:
    /* ! this hack is to make sure 2 devices never collide on same addr region */
    pci_disable_device(pdev);
    pci_release_region(pdev, bar);
    goto error_enable_mem;
error_req_region:
    pci_disable_device(pdev);
error_enable_mem:
    pci_clear_mwi(pdev);
    pci_clear_master(pdev);
error_enable:
    INFO("");
    printk(KERN_ERR "Error code: %d\n", err);
    return err;
}


/* Reqest IRQ and setup handler */
static int set_interrupts(struct pci_dev *pdev)
{
    int err, irq;
    struct driver_pvt *drv_pvt;
    // printk(KERN_INFO "Setting interrupthandlers\n");
    drv_pvt = pci_get_drvdata(pdev);
    err = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_ALL_TYPES);

    if(err < 0)
        goto error_alloc_irq_vec;
    
    irq = pci_irq_vector(pdev, 0);
    if(irq < 0)
    {
        err = irq;
        goto error_get_irq;
    }
    // printk("irq: %d\n", irq);
    
    // free_irq(irq, pdev);
    err = request_threaded_irq(irq, irq_handler, NULL, IRQF_SHARED, pci_name(pdev), pdev);
    if(err < 0)
        goto error_req_threaded_irq;

    return 0;
    
error_alloc_irq_vec:
    printk(KERN_ALERT "Can't allocate irq_vectors\n");
    goto error;
error_get_irq:
    printk(KERN_ALERT "Can't get irq_vector\n");
error_req_threaded_irq:
    printk(KERN_ALERT "Can't assign interrupt handler\n");
    pci_free_irq_vectors(pdev);
error:
    return err;
}


static void on_device_unload(struct pci_dev *pdev)
{
    int irq;
    struct driver_pvt *drv_pvt = pci_get_drvdata(pdev);

    cleanup_char_dev();

    if (drv_pvt) {
        if (drv_pvt->hwmem) 
        {
            iounmap(drv_pvt->hwmem);
        }
        irq = pci_irq_vector(pdev, 0);
        if(irq > 0)
            free_irq(irq, pdev);
        pci_free_irq_vectors(pdev);

        kfree(drv_pvt);
    }


    /* Free memory region */
    pci_clear_mwi(pdev);

    pci_clear_master(pdev);
    /* And disable device */
    pci_disable_device(pdev);
    // call after disabling to prevent two devices form colliding
    pci_release_region(pdev, pci_select_bars(pdev, IORESOURCE_MEM));

    INFO("Device unloaded");
    printk(KERN_INFO "unloaded device: %x:%x\n", pdev->vendor, pdev->device);
}


static int __init cryptocardpci_driver_init(void)
{
    int err;
    printk(KERN_INFO "Loading Driver\n");
    err = pci_register_driver(&driver);
	return err;
}

static void __exit cryptocardpci_driver_exit(void)
{
    pci_unregister_driver(&driver);
	printk(KERN_INFO "Unloading Driver\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("praskr@cse.iitk.ac.in");
MODULE_DESCRIPTION("Driver for cryptdevice");

module_init(cryptocardpci_driver_init);
module_exit(cryptocardpci_driver_exit);
