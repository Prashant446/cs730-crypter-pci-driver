#ifndef __CRYPTOCARD_CHARDEV_H_
#define __CRYPTOCARD_CHARDEV_H_

#include <linux/device.h>
#include <linux/types.h>
#include <linux/fs.h>

#include "core.h"

#define DEVNAME "cryptdevice"


int create_char_dev(struct driver_pvt* drv_pvt);
void cleanup_char_dev(void);


struct crypt_device_private {
	uint8_t chnum;
	struct driver_pvt* drv_pvt;
    struct cryptdev_config config;
};

#endif
