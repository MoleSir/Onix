#include <onix/buffer.h>
#include <ds/list.h>
#include <onix/task.h>
#include <string.h>
#include <onix/debug.h>
#include <onix/memory.h>
#include <onix/device.h>
#include <onix/assert.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

// 哈希数量，应该是一个素数
#define HASH_COUNT 31

static buffer_t* buffer_start = (buffer_t*)KERNEL_BUFFER_MEM;
static u32 buffer_count = 0;

// 记录当前 buffer_t 结构体的位置
static buffer_t* buffer_ptr = (buffer_t*)KERNEL_BUFFER_MEM;

// 记录当前数据缓冲区位置
static void* buffer_data = (void*)(KERNEL_BUFFER_MEM + KERNEL_BUFFER_SIZE - BLOCK_SIZE);

// 缓冲链表，被释放的块
static list_t free_list;
// 等待进行链表
static list_t wait_list;
// 缓冲哈希表
static list_t hash_table[HASH_COUNT];

// 哈希函数，参数是：设备号和块号
u32 hash(dev_t dev, idx_t block)
{
    // 最大也只能是 HASH_COUNT
    return (dev ^ block) % HASH_COUNT;
}

static buffer_t* get_from_hash_table(dev_t dev, idx_t block)
{
    // 先找数组
    u32 idx = hash(dev, block);
    list_t* list = hash_table + idx;
    buffer_t* bf = NULL;

    // 再遍历链表
    for (list_node_t* node = list->head.next; node != &(list->tail); node = node->next)
    {
        buffer_t* ptr = element_entry(buffer_t, hnode, node);
        if (ptr->dev == dev && ptr->block == block)
        {
            bf = ptr;
            break;
        }
    }

    // 没找到，返回空指针
    if (!bf)
        return NULL;

    // bf 存在缓冲列表，移除
    if (list_search(&free_list, &(bf->rnode)))
        list_remove(&(bf->rnode));

    return bf;
}

// 将 bf 放入哈希表
static void hash_locate(buffer_t* bf)
{
    u32 idx = hash(bf->dev, bf->block);
    list_t* list = hash_table + idx;
    assert(!list_search(list, &(bf->hnode)));
    list_push(list, &(bf->hnode));
}

// 将 bf 从哈希表移除
static void hash_remove(buffer_t* bf)
{
    u32 idx = hash(bf->dev, bf->block);
    list_t* list = hash_table + idx;
    assert(list_search(list, &(bf->hnode)));
    list_remove(&(bf->hnode));
}

// 在内存中初始化一个 buffer 
static buffer_t* get_new_buffer()
{
    buffer_t* bf = NULL;    
    // 判断内存是否足够
    if ((u32)buffer_ptr + sizeof(buffer_t) < (u32)buffer_data)
    {
        bf = buffer_ptr;
        bf->data = buffer_data;
        bf->dev = EOF;
        bf->block = 0;
        bf->count = 0;
        bf->dirty = false;
        bf->vaild = false;
        lock_init(&(bf->lock));
        buffer_count++;
        buffer_ptr++;
        buffer_data -= BLOCK_SIZE;
        LOGK("buffer count %d\n", buffer_count);
    }

    return bf;
}

// 获得空闲的 buffer
static buffer_t* get_free_buffer()
{
    buffer_t* bf = NULL;
    while (true)
    {
        // 如果内存足够，直接获得缓冲
        bf = get_new_buffer();
        if (bf)
            return bf;
        
        // 否则，从空闲列表获得
        if (!list_empty(&(free_list)))
        {
            // 取最远没有被访问的块
            bf = element_entry(buffer_t, rnode, list_popback(&free_list));
            hash_remove(bf);
            bf->vaild = false;
            return bf;
        }

        // 等待某个缓冲释放
        task_block(running_task(), &(wait_list), TASK_BLOCKED);
    }
}

// 获取设备 dev，第 block 对应的缓冲
buffer_t* getblk(dev_t dev, idx_t block)  
{
    // 先从已经创建好的哈希表中找
    buffer_t* bf = get_from_hash_table(dev, block);
    if (bf)
    {
        bf->count++;
        return bf;
    }

    // 哈希表没有找到 dev 设备的 block 块缓冲，尝试去构建一个
    bf = get_free_buffer();
    assert(bf->count == 0);
    assert(bf->dirty == 0);

    // 赋上相应的值
    bf->count = 1;
    bf->dev = dev;
    bf->block = block;
    // 把创建好的缓冲放入哈希表中
    hash_locate(bf);
    return bf;
}

// 读取 dev 设备的 block 块
buffer_t* bread(dev_t dev, idx_t block)
{
    buffer_t* bf = getblk(dev, block);
    assert(bf != NULL);
    if (bf->vaild)
    {
        return bf;
    }

    lock_acquire(&(bf->lock));

    if (!(bf->vaild))
    {
        device_request(bf->dev, bf->data, BLOCK_SECS, bf->block * BLOCK_SECS, 0, REQ_READ);

        bf->dirty = false;
        bf->vaild = true;
    }

    lock_release(&(bf->lock));
    return bf;
}

// 写缓冲，将内存中的缓冲块数据写入到磁盘的对应位置
void bwrite(buffer_t* bf)
{
    assert(bf);
    if (!bf->dirty)
        return;
    device_request(bf->dev, bf->data, BLOCK_SECS, bf->block * BLOCK_SECS, 0, REQ_WRITE);
    bf->dirty = false;
    bf->vaild = true;
}

// 释放缓冲
void brelse(buffer_t* bf)
{
    if (!bf)
        return;
    if (bf->dirty)
        bwrite(bf);

    bf->count--;
    assert(bf->count >= 0);
    if (bf->count)
        return;

    assert(!bf->rnode.next);
    assert(!bf->rnode.prve);
    list_push(&free_list, &bf->rnode);
    if (!list_empty(&wait_list))
    {
        task_t *task = element_entry(task_t, node, list_popback(&wait_list));
        task_unblock(task);
    }
}

void buffer_init()
{
    LOGK("buffer_t size is %d\n", sizeof(buffer_t));

    // 初始化空闲链表
    list_init(&free_list);
    // 初始化等待进行链表
    list_init(&wait_list);

    // 初始化哈希表
    for (size_t i = 0; i < HASH_COUNT; ++i)
        list_init(&(hash_table[i]));
}