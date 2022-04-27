#ifndef __CRYPTOCARD_CORE_H_
#define __CRYPTOCARD_CORE_H_

#include<linux/types.h>
#include<linux/io.h>
#include<linux/pci.h>
#include<linux/device.h>

#include "cryptocard.h"


static inline u64 my_ioread64(volatile void __iomem *addr)
{
    u64 ret; 
    asm volatile("mov" "q" " %1,%0":"=r" (ret) \
        :"m" (*(volatile u64 __force *)addr) :"memory"); 
    return ret;
}

static inline void my_iowrite64(u64 val, volatile void __iomem *addr)
{
    asm volatile(
            "mov" "q" " %0,%1": :"r" (val),
            "m" (*(volatile u64 __force *)addr)
            :"memory");
}

#define myioread8(addr, offset) \
        ioread8(addr + offset)
#define myioread16(addr, offset) \
        ioread16(addr + offset)
#define myioread32(addr, offset) \
        ioread32(addr + offset)
#define myioread64(addr, offset) \
        my_ioread64(addr + offset)

#define myiowrite8(val, addr, offset) \
        iowrite8(val, addr + offset)
#define myiowrite16(val, addr, offset) \
        iowrite16(val, addr + offset)
#define myiowrite32(val, addr, offset) \
        iowrite32(val, addr + offset)
#define myiowrite64(val, addr, offset) \
        my_iowrite64(val, addr + offset)


#define YES 1
#define NO  0
typedef enum {ENCR, DECR} devop_t;

int mimo_op(const char *buff,
            size_t len,
            devop_t mode,
            struct driver_pvt *drv_pvt);

int dma_op(const char *buff,
            size_t len,
            devop_t mode,
            struct driver_pvt *drv_pvt);

irqreturn_t irq_handler(int irq, void *cookie);

#endif
