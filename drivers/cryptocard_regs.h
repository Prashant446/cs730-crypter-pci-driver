#ifndef __CRYPTOCARD_REGS_H_
#define __CRYPTOCARD_REGS_H_

#define REG_IDENT           0x0
#define REG_LIVE            0x4
#define REG_KEY             0x8
#define REG_KEYA            0xa
#define REG_KEYB            0xb
#define REG_MIMO_LEN        0xc
#define REG_MIMO_SR         0x20
#define REG_INT_SR          0x24
#define REG_INT_RR          0x60
#define REG_INT_ACKR        0x64
#define REG_MIMO_DATA_ADDR  0x80
#define REG_DMA_ADDR        0x90
#define REG_DMA_LEN         0x98
#define REG_DMA_CR          0xa0

// #define KEYA_OFF 0x8
// #define KEYB_OFF 0x0

#define MIMO_ST_MASK    0xfffffffe
#define MIMO_ST_OFF     0x0
#define MIMO_MODE_OFF   0x1
#define MIMO_INT_OFF    0x7

#define DMA_CR_MASK     0x7
#define DMA_ST_OFF      0x0
#define DMA_MODE_OFF    0x1
#define DMA_INT_OFF     0x2


#endif
