#include <linux/types.h>
#include <linux/io.h>
#include <linux/pci.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <linux/sched.h>

#include "core.h"

#include "cryptocard_regs.h"

const size_t MIMO_LEN_LIM  =   1023UL*1024;
const size_t DMA_LEN_LIM   =   32UL*1024;


int mimo_op(const char __user *buff,
            size_t len,
            devop_t mode,
            struct driver_pvt *drv_pvt)
{
    u8 dec_mode, int_mode;
    u32 key, len_reg, st_reg;
    void *base = drv_pvt->hwmem;
    void* dev_buf_addr = base + 0xa8;
    size_t chunk_size;
    
    key = drv_pvt->config.key;
    
    // printk(KERN_INFO "Setting Key: 0x%x\n", key);
    myiowrite32(key, base, REG_KEY);


    st_reg = myioread32(base, REG_MIMO_SR);
    // printk(KERN_INFO "MIMO SR: 0x%x\n", st_reg);
    dec_mode = (mode == DECR);
    int_mode = drv_pvt->config.use_intr;
    st_reg = MIMO_ST_MASK & ((dec_mode << MIMO_MODE_OFF) | (int_mode << MIMO_INT_OFF));

    // printk(KERN_INFO "Setting MIMO SR: 0x%x, %u\n", st_reg, dec_mode);
    myiowrite32(st_reg, base, REG_MIMO_SR);
    

    while(len > 0)
    {

        chunk_size = (len > MIMO_LEN_LIM) ? MIMO_LEN_LIM : len;
        
        // printk(KERN_INFO "Setting len: %lu out of rem %lu\n", chunk_size, len);
        myiowrite32((u32)chunk_size, base, REG_MIMO_LEN);

        // printk(KERN_INFO "Copying to device\n");
        if(buff)
            copy_from_user(dev_buf_addr, buff, chunk_size);
        // memcpy(dev_buf_addr, buff, chunk_size);
        
        
        drv_pvt->int_rec = 0;
        // printk(KERN_INFO "Setting Address reg to 0x%x\n", 0xa8);
        myiowrite64(0xa8, base, REG_MIMO_DATA_ADDR);

        if(int_mode)
        {
            // printk(KERN_INFO "MIMO mode: interrupt\n");

            /* interrupt handler will set this to 1 */
            while(wait_event_interruptible(drv_pvt->wq, drv_pvt->int_rec == 1) != 0)
            {
                printk("Some signal recieved while waiting for iterrupt\n");
                return -EAGAIN;
            }
        }
        else
        {
            st_reg = myioread32(base, REG_MIMO_SR);
            int i = 0;
            while(st_reg & 1)
            {
                // printk("Waiting iter: %d, MIMO SR: 0x%x\n", ++i, st_reg);
                usleep_range(100, 101);
                // schedule();
                st_reg = myioread32(base, REG_MIMO_SR);
            }
        }

        // memcpy(buff, dev_buf_addr, chunk_size);
        if(buff)
            copy_to_user(buff, dev_buf_addr, chunk_size);
        if(buff)
            buff += chunk_size;
        len -= chunk_size;
    }
    
    return 0;
}


int dma_op(const char __user *buff,
            size_t len,
            devop_t mode,
            struct driver_pvt *drv_pvt)
{
    u8 dec_mode, int_mode;
    u32 key;
    u64 len_reg, st_reg;
    void *base = drv_pvt->hwmem;
    char *kbuff;
    size_t chunk_size, dma_buffer_len;
    struct device *dev = &(drv_pvt->pdev->dev);
    dma_addr_t dma_handle;

    dma_buffer_len = DMA_LEN_LIM;
    kbuff = kmalloc(dma_buffer_len, GFP_KERNEL);

    if(!kbuff)
        return -ENOMEM;

    int err = 0;

    dma_handle = dma_map_single(dev, kbuff, dma_buffer_len, DMA_BIDIRECTIONAL);
    if(err = dma_mapping_error(dev, dma_handle))
    {
        printk(KERN_WARNING "DMA: Mapping error\n");
        goto error;
    }

    key = drv_pvt->config.key;
    dec_mode = (mode == DECR);
    int_mode = drv_pvt->config.use_intr;
    
    
    // printk(KERN_INFO "Setting Key: 0x%x\n", key);
    myiowrite32(key, base, REG_KEY);

    // printk(KERN_INFO "Setting Address reg to 0x%llx\n", dma_handle);
    myiowrite64(dma_handle, base, REG_DMA_ADDR);

    while(len > 0)
    {
        chunk_size = (len > DMA_LEN_LIM) ? DMA_LEN_LIM : len;
        if(copy_from_user(kbuff, buff, chunk_size)) 
        {
            err = -EFAULT;
            printk(KERN_ERR "DMA: Error while copying from user\n");
            goto error;
        }
        
        st_reg = DMA_CR_MASK & ((1UL << DMA_ST_OFF) | (dec_mode << DMA_MODE_OFF) | (int_mode << DMA_INT_OFF));

        // printk(KERN_INFO "Setting len: %lu\n", chunk_size);
        myiowrite64(chunk_size, base, REG_DMA_LEN);

        /*  */
        dma_sync_single_for_device(dev, dma_handle, dma_buffer_len, DMA_TO_DEVICE);
        
        drv_pvt->int_rec = 0;
        // printk(KERN_INFO "Setting DMA CR: 0x%llx, DEC: %u\n", st_reg, dec_mode);
        myiowrite64(st_reg, base, REG_DMA_CR);

        if(int_mode)
        {
            // printk(KERN_INFO "DMA mode: interrupt\n");
            /* interrupt handler will set this to 1 */
            while(wait_event_interruptible(drv_pvt->wq, drv_pvt->int_rec == 1) != 0)
            {
                printk("Some signal recieved while waiting for iterrupt\n");
                return -EAGAIN;
            }
        }
        else
        {
            // st_reg = myioread64(base, REG_DMA_CR);
            int i = 0;
            while(st_reg & 1)
            {
                // printk("Waiting iter: %d DMA SR: 0x%llx\n", ++i, st_reg);
                usleep_range(100, 101);
                // schedule();
                st_reg = myioread64(base, REG_DMA_CR);
            }
        }
    
        /*  */
        dma_sync_single_for_cpu(dev, dma_handle, dma_buffer_len, DMA_FROM_DEVICE);

        if(copy_to_user(buff, kbuff, chunk_size))
        {
            err = -EFAULT;
            printk(KERN_ERR "DMA: Error while copying back to user\n");
            goto error;
        }

        buff += chunk_size;
        len -= chunk_size;
    }

error:
    kfree(kbuff);
    return 0;
}

irqreturn_t irq_handler(int irq, void *cookie)
{
    struct driver_pvt *drv_pvt;
    struct pci_dev *pdev;
    u8 *base;
    u32 isr;

    pdev = (struct pci_dev *)cookie;
    drv_pvt = pci_get_drvdata(pdev);

    base = drv_pvt->hwmem;

    isr = myioread32(base, REG_INT_SR);
    myiowrite32(isr, base, REG_INT_ACKR);

    // printk("Handled IRQ #%d, IS_REG: 0x%x\n", irq, isr);

    drv_pvt->int_rec = 1;
    wake_up_interruptible_sync(&drv_pvt->wq);

    return IRQ_HANDLED;
}
