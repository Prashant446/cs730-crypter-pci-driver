#ifndef __CRYPTOCARD_MOD_H_
#define __CRYPTOCARD_MOD_H_

#include<linux/pci.h>
#include<linux/device.h>
#include<linux/wait.h>
#include<linux/mutex.h>


#define DRIVER "praskrCryptDriver"

#define INFO(msg)                                                   \
    do                                                              \
    {                                                               \
        printk(KERN_INFO "%s:%d: ", __FILE__, __LINE__);   \
    } while(0)


// structs
struct cryptdev_config {
    uint8_t use_dma;
    uint8_t use_intr;
    uint32_t key;
};

struct driver_pvt {
    u8 __iomem *hwmem;
    unsigned long mmio_start, mmio_len, mmio_flags;
    struct cryptdev_config config;
    /* pointer back to the struct pci_dev containing the data */
    struct pci_dev *pdev;
    u8 dma_enabled;
    /* for process waiting for interrupt */
    wait_queue_head_t wq;
    /* flag to store wether interrupt happened or not */
    u8 int_rec;
    /* mutex for device */
    struct mutex dev_mutex;
};

extern const struct cryptdev_config default_config;

#endif
