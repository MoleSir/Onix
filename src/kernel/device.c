#include <onix/device.h>
#include <string.h>
#include <onix/task.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/arena.h>
#include <ds/list.h>
#include <onix/onix.h>

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

        list_init(&(device->request_list));
        device->direct = DIRECT_UP;
    }
}

// 执行块设备请求
static void do_request(request_t *req)
{
    LOGK("dev %d do request idx %d\n", req->dev, req->idx);

    switch (req->type)
    {
    case REQ_READ:
        device_read(req->dev, req->buf, req->count, req->idx, req->flags);
        break;
    case REQ_WRITE:
        device_write(req->dev, req->buf, req->count, req->idx, req->flags);
        break;
    default:
        panic("req type %d unknown!!!");
        break;
    }
}

// 获得下一个请求
static request_t* request_next_req(device_t* device, request_t* req)
{
    list_t* list = &(device->request_list);

    // 调度到最大的磁道，更改方向
    if (device->direct == DIRECT_UP && req->node.next == &(list->tail))
        device->direct = DIRECT_DOWN;
    // 调度到早小的磁道，更改方向
    else if (device->direct == DIRECT_DOWN && req->node.prve == &(list->head))
        device->direct = DIRECT_UP;

    void* next = NULL;
    // 如果磁道方向向上，就取当前的下一个请求
    if (device->direct == DIRECT_UP)
        next = req->node.next;
    // 如果磁道方向向下，就取当前的上一个请求
    else
        next = req->node.prve;
    
    // 已经没有请求，返回空
    if (next == &(list->head) || next == &(list->tail))
        return NULL;

    return element_entry(request_t, node, next);
}

// 块设备请求
void device_request(dev_t dev, void *buf, u8 count, idx_t idx, int flags, u32 type)
{
    device_t *device = device_get(dev);
    assert(device->type = DEV_BLOCK); // 是块设备
    idx_t offset = idx + device_ioctl(device->dev, DEV_CMD_SECTOR_START, 0, 0);

    // 如果是块，就找到其所在磁盘进行操作
    if (device->parent)
        device = device_get(device->parent);

    // 创建一个请求结构体，赋上参数
    request_t *req = kmalloc(sizeof(request_t));
    req->dev = device->dev;
    req->buf = buf;
    req->count = count;
    req->idx = offset;
    req->flags = flags;
    req->type = type;
    req->task = NULL;

    // 判断当前设备的请求列表是否为空
    bool empty = list_empty(&device->request_list);

    // 将请求插入链表
    list_insert_sort(
        &(device->request_list), &(req->node),
        element_node_offset(request_t, node, idx)
    );

    // 如果列表不为空，则阻塞，因为已经有请求在处理了，等待处理完成；
    if (!empty)
    {
        req->task = running_task();
        task_block(req->task, NULL, TASK_BLOCKED);
    }

    // 阻塞解开或链表为空，可用处理这个请求
    do_request(req);

    // 获得下一关请求
    request_t* next_req = request_next_req(device, req);

    // 处理完，释放节点，释放内存空间
    list_remove(&req->node);
    kfree(req);

    // 如果此时链表还有请求，就唤醒最后一个——先来先服务策略
    if (next_req)
    {
        assert(next_req->task->magic == ONIX_MAGIC);
        task_unblock(next_req->task);
    }
}