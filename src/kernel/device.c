#include <onix/device.h>
#include <string.h>
#include <onix/task.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/arena.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define DEVICE_NR 64

static device_t devices[DEVICE_NR];

// 获取空设备
static device_t* get_null_device()
{
    // 从 1 索引开始，0 号设备一直是 NULL
    for (size_t i = 1; i < DEVICE_NR; ++i)
    {
        device_t* device = devices + i;
        if (device->type == DEV_NULL)
            return device;
    }
    panic("no more divices!!!\n");
}

// 配置一个空设备，返回设备号
dev_t device_install(
    int type, int subtype,
    void* ptr, char* name, dev_t parent, 
    void* ioctl, void* read, void* write)
{
    device_t* device = get_null_device();
    device->ptr = ptr;
    device->parent = parent;
    device->type = type;
    device->subtype = subtype;
    strncpy(device->name, name, NAMELEN);
    device->ioctl = ioctl;
    device->read = read;
    device->write = write;
    return device->dev;
}

// 根据子类型查找设备
device_t* device_find(int subtype, idx_t idx)
{
    idx_t nr = 0;
    // 找到第 idx 个 subtype 类型的设备
    for (size_t i = 0; i < DEVICE_NR; ++i)
    {
        device_t* device = devices + i;
        if (device->subtype == subtype)
        {
            if (nr == idx)
                return device;
            else
                nr++;
        }
    }
    return NULL;
}

// 根据设备号查找
device_t* device_get(dev_t dev)
{
    assert(dev < DEVICE_NR);
    device_t* device = devices + dev;
    assert(device->type != DEV_NULL);
    return device;
}

// 控制设备
int device_ioctl(dev_t dev, int cmd, void* args, int flags)
{
    // 找到对应的设备，使用设备本身的函数指针
    device_t* device = device_get(dev);
    if (device->ioctl)
        return device->ioctl(device->ptr, cmd, args, flags);
    LOGK("ioctl of device %d not implement!!!\n", dev);
    return EOF;
}

// 读设备
int device_read(dev_t dev, void* buf, size_t count, idx_t idx, int flags)
{
    // 找到对应的设备，使用设备本身的函数指针
    device_t* device = device_get(dev);
    if (device->read)
        return device->read(device->ptr, buf, count, idx, flags);
    LOGK("read of device %d not implement!!!\n", dev);
    return EOF;
}

// 写设备
int device_write(dev_t dev, void* buf, size_t count, idx_t idx, int flags)
{
    // 找到对应的设备，使用设备本身的函数指针
    device_t* device = device_get(dev);
    if (device->write)
        return device->write(device->ptr, buf, count, idx, flags);
    LOGK("write of device %d not implement!!!\n", dev);
    return EOF;
}

// 设备初始化
void device_init()
{
    // 初始化全部设备为空
    for (size_t i = 0; i < DEVICE_NR; ++i)
    {
        device_t* device = devices + i;
        strcpy((char*)device->name, "null");
        device->type = DEV_NULL;
        device->subtype = DEV_NULL;
        device->dev = i;
        device->parent = 0;
        device->ioctl = NULL;
        device->read = NULL;
        device->write = NULL;
    }
}