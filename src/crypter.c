#include<crypter.h>
#include<cryptocard_user.h>

static const char dev_path[] = "/dev/cryptdevice";
static size_t mapped_size = 0;

/*Function template to create handle for the CryptoCard device.
On success it returns the device handle as an integer*/
DEV_HANDLE create_handle()
{
    int fd = open(dev_path, O_RDWR | O_SYNC);
    if(fd < 0)
        return ERROR;
    return fd;
}

/*Function template to close device handle.
Takes an already opened device handle as an arguments*/
void close_handle(DEV_HANDLE cdev)
{
    close(cdev);
}

/*Function template to encrypt a message using MMIO/DMA/Memory-mapped.
Takes four arguments
  cdev: opened device handle
  addr: data address on which encryption has to be performed
  length: size of data to be encrypt
  isMapped: TRUE if addr is memory-mapped address otherwise FALSE
*/
int encrypt(DEV_HANDLE cdev, ADDR_PTR addr, uint64_t length, uint8_t isMapped)
{
    if(!addr)
        return ERROR;
    if(isMapped == TRUE)
    {
        addr = NULL;
    }
    if(read(cdev, addr, length))
        return ERROR;
    return 0;
}

/*Function template to decrypt a message using MMIO/DMA/Memory-mapped.
Takes four arguments
  cdev: opened device handle
  addr: data address on which decryption has to be performed
  length: size of data to be decrypt
  isMapped: TRUE if addr is memory-mapped address otherwise FALSE
*/
int decrypt(DEV_HANDLE cdev, ADDR_PTR addr, uint64_t length, uint8_t isMapped)
{
    if(!addr)
        return ERROR;
    if(isMapped == TRUE)
    {
        addr = NULL;
    }
    if(write(cdev, addr, length))
        return ERROR;
    return 0;
}

/*Function template to set the key pair.
Takes three arguments
  cdev: opened device handle
  a: value of key component a
  b: value of key component b
Return 0 in case of key is set successfully*/
int set_key(DEV_HANDLE cdev, KEY_COMP a, KEY_COMP b)
{
    uint32_t key = a;
    key = (key << KEYA_OFF) | b;
    if(ioctl(cdev, IOCTL_SET_KEY, key))
        return ERROR;
    return 0;
}

/*Function template to set configuration of the device to operate.
Takes three arguments
  cdev: opened device handle
  type: type of configuration, i.e. set/unset DMA operation, interrupt
  value: SET/UNSET to enable or disable configuration as described in type
Return 0 in case of key is set successfully*/
int set_config(DEV_HANDLE cdev, config_t type, uint8_t value)
{
    uint64_t req;
    if(type == DMA)
        req = IOCTL_SET_USE_DMA;
    else if(type == INTERRUPT)
        req = IOCTL_SET_USE_INT;
    else
        return ERROR;
    
    if(ioctl(cdev, req, value))
        return ERROR;
    return 0;
}

/*Function template to device input/output memory into user space.
Takes three arguments
  cdev: opened device handle
  size: amount of memory-mapped into user-space (not more than 1MB strict check)
Return virtual address of the mapped memory*/
ADDR_PTR map_card(DEV_HANDLE cdev, uint64_t size)
{
    if(mapped_size)
    {
        // some other process is already mapping
        /* we are assuming that only one process can map at a time */
        return NULL;
    }
    size += DEVICE_BUFFER_OFFSET;
    mapped_size = size;
    if(size > DEVICE_IO_REGION_SIZE)
        return NULL;
    void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, cdev, 0);
    return addr ? (addr + DEVICE_BUFFER_OFFSET) : NULL;
}

/*Function template to device input/output memory into user space.
Takes three arguments
  cdev: opened device handle
  addr: memory-mapped address to unmap from user-space*/
void unmap_card(DEV_HANDLE cdev, ADDR_PTR addr)
{
    int err = 0;
    if(!addr)
        return;
    err = munmap(addr - DEVICE_BUFFER_OFFSET, mapped_size);
    if(!err)
        mapped_size = 0;
}
