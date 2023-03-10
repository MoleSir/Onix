#include <ds/bitmap.h>
#include <onix/assert.h>
#include <string.h>
#include <onix/buffer.h>
#include <onix/fs.h>
#include <onix/task.h>
#include <onix/stat.h>
#include <onix/arena.h>
#include <ds/fifo.h>
#include <onix/memory.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define INODE_NR 64

#define MIN(x, y) (x < y ? x : y)

// 保存 inode_t 结构体
// 系统中可能存在多个文件系统，每个文件系统的 super 会记录自己的 inode，但所有的 inode 都会保存在这个数组
static inode_t inode_table[INODE_NR];

// 申请一个 inode 结构体空间
static inode_t* get_free_inode_struct()
{
    for (size_t i = 0; i < INODE_NR; ++i)
    {
        inode_t* inode = inode_table + i;
        // 找到第一个 设备为 EOF 的结构体
        if (inode->dev == EOF)
            return inode;
    }
    panic("no more inode!!!");
}

// 释放一个 inode 结构体空间
static void put_free_inode_struct(inode_t* inode)
{
    assert(inode != inode_table);
    assert(inode->count == 0);
    // 将 inode 的设备设置为 EOF
    inode->dev = EOF;
}

// 计算 nr 号的 inode 在哪个块（磁盘逻辑块号）
static inline idx_t cal_inode_block(super_block_t* sb, idx_t nr)
{
    // | 主引导 | 超级块 | inode 位图 | 文件块位图 | 保存 inode 的块 | 保存文件的块 |
    // inode 的 nr 从 1 开始编址
    // 磁盘结构：主引导 + 超级块 + inode 位图 + 文件块位图 + (nr-1) / 一块的 inode 数量
    return 2 + sb->desc->imap_block + sb->desc->zmap_block + (nr - 1) / BLOCK_INODES;
}

// 获取管道
inode_t* get_pipe_inode()
{
    inode_t* inode = get_free_inode_struct();

    // 不是实际存在于设备中，但 EOF = -1 表示无效，所以这里使用 -2
    inode->dev = -2;
    // 申请内存，表示缓冲队列，用 desc 保持缓冲队列结构体地址
    inode->desc = (inode_desc_t*)kmalloc(sizeof(fifo_t));
    // 管道缓冲区，用 buf 保持缓冲队列缓冲区地址
    inode->buf = (void*)alloc_kpage(1);
    // 两个文件
    inode->count = 2;
    // 管道标志
    inode->pipe = true;
    // 初始化输入输出设备
    fifo_init((fifo_t*)(inode->desc), (char*)(inode->buf), PAGE_SIZE);
    
    return inode;
}

// 释放管道
void put_pipe_inode(inode_t* inode)
{
    if (!inode)
        return;
    inode->count--;
    if (inode->count)
        return;
    inode->pipe = false;

    // 释放 fifo
    kfree(inode->desc);
    // 释放缓冲区
    free_kpage((u32)(inode->buf), 1);
    // 释放 inode
    put_free_inode_struct(inode);
}

// 从 dev 号磁盘的已被读取的 inode 中查找编号为 nr 的 inode
static inode_t* find_exist_inode(dev_t dev, idx_t nr)
{
    // 得到磁盘超级块中的链表
    super_block_t* sb = get_super(dev);
    assert(sb);
    list_t* list = &(sb->inode_list);

    // 依次遍历链表，找到其中的某个 nr 为指定编号的 inode 返回
    for (list_node_t* node = list->head.next; node != &(list->tail); node = node->next)
    {
        inode_t* inode = element_entry(inode_t, node, node);
        if (inode->nr == nr)
            return inode;
    }
    return NULL;
}

// 获取根目录的 inode
inode_t* get_root_inode()
{
    return inode_table;
}

extern time_t time();

static inode_t* fit_inode(inode_t* inode)
{
    if (!(inode->mount))
        return inode;
    
    super_block_t* sb = get_super(inode->mount);
    assert(sb);
    iput(inode);
    inode = sb->iroot;
    inode->count++;
    return inode;
}

// 获得设备号为 dev 磁盘的 nr 号 indoe
// 调用者保证 dev 磁盘的 nr 号 inode 是有效的（位图为 1）
inode_t* iget(dev_t dev, idx_t nr)
{
    // 尝试在已经存在的 inode 寻找，这里的存在是指读入内存的存在
    // 这个函数不可以向磁盘申请一个新的 inode（位图）
    inode_t* inode = find_exist_inode(dev, nr);
    if (inode)
    {
        // 找到了，引用计数增加，更新访问时间为当前
        inode->count++;
        inode->atime = time();

        return fit_inode(inode);
    }

    // 没有在已读取的 inode 找到，就从磁盘读入一个
    super_block_t* sb = get_super(dev);
    assert(sb);
    // 得到一个空闲的 inode 结构体
    inode = get_free_inode_struct();
    // 配置 inode 的属性
    inode->dev = dev;
    inode->nr = nr;
    inode->count++;

    // 加入本设备的 inode 链表
    list_push(&(sb->inode_list), &(inode->node));

    // 找到该 inode 在磁盘中的块
    idx_t block = cal_inode_block(sb, inode->nr);
    // 读取整个块
    buffer_t* buf = bread(inode->dev, block);
    // 配置 inode 的缓冲区
    inode->buf = buf;

    // 一个块保存多个 inode，找此 inode 的位置
    // 将缓冲视为一个 inode 描述符数组，获得对应指针，求 nr 在一个块中的索引，（nr - 1）!!!
    inode->desc = (inode_desc_t*)(buf->data) + ((inode->nr - 1) % BLOCK_INODES); 
    // 更新时间
    inode->ctime = inode->desc->mtime;
    inode->atime = time();

    return inode;
}

// 创建新的 inode 
inode_t* new_inode(dev_t dev, idx_t nr)
{
    task_t* task = running_task();
    inode_t* inode = iget(dev, nr);

    inode->buf->dirty = true;

    inode->desc->mode = 0777 & (~(task->umask));
    inode->desc->uid = task->uid;
    inode->desc->size = 0;
    inode->desc->mtime = inode->atime = time();
    inode->desc->gid = task->gid;
    inode->desc->nlinks = 1;

    return inode;
}

// 释放 inode
void iput(inode_t* inode)
{
    if (!inode)
        return;
    
    if (inode->pipe)
    {
        put_pipe_inode(inode);
        return;
    }
    
    if (inode->buf->dirty)
        bwrite(inode->buf);

    // 应用计数减一
    inode->count--;
    if (inode->count)
        return;

    // 引用计数为 0，释放 inode 缓冲
    brelse(inode->buf);

    // 链表移除
    list_remove(&(inode->node));

    // 释放 inode 内存
    put_free_inode_struct(inode);
}

// 获取 inode 第 block 块索引，如果不存在，并且 create 为 true 则创建
// inode 指向一个文件，返回该文件内容的第 block 号文件块索引（是针对这个文件的内容的索引）
// 返回的是磁盘逻辑块
idx_t bmap(inode_t* inode, idx_t block, bool create)
{
    assert(block >= 0 && block < TOTAL_BLOCK);

    // 数组索引
    u16 index = block;

    // 数组
    u16* array = inode->desc->zone;

    // 缓冲区
    buffer_t* buf = inode->buf;

    // 用于下面的 brelse 传入参数 indoe 的 buf 不应该释放
    buf->count += 1;

    // 当前处理级别
    int level = 0;

    // 当前子块数量
    int divider = 1;

    // 直接块
    if (block < DIRECT_BLOCK)
        goto reckon;

    block -= DIRECT_BLOCK;

    if (block < INDIRECT1_BLOCK)
    {
        index = DIRECT_BLOCK;
        level = 1;
        divider = 1;
        goto reckon;
    }
    
    block -= INDIRECT1_BLOCK;
    assert(block < INDIRECT2_BLOCK);
    index = DIRECT_BLOCK + 1;
    level = 2;
    divider = BLOCK_INDEXES;
    
reckon:
    for (; level >=0; level--)
    {
        // 如果不存在，并且 create，申请一个文件块
        if (!array[index] && create)
        {
            array[index] = balloc(inode->dev);
            buf->dirty = true;
        }
        brelse(buf);

        // 如果 level == 0 或者索引不存在，直接返回
        if (level == 0 || !array[index])
            return array[index];

        // level 不为 0，处理下一级索引
        buf = bread(inode->dev, array[index]);
        index = block / divider;
        block = block % divider;
        divider /= BLOCK_INDEXES;
        array = (u16*)(buf->data);
    }
}

// 从 inode 的 offset 处，读 len 个字节到 buf
int inode_read(inode_t *inode, char *buf, u32 len, off_t offset)
{
    assert(ISFILE(inode->desc->mode) || ISDIR(inode->desc->mode));

    // 判断文件偏移量
    if (offset >= inode->desc->size)
        return EOF;

    // 开始读取的位置
    u32 begin = offset;

    // 剩余字节数量
    u32 left = MIN(len, inode->desc->size - offset);
    while (left)
    {
        // 找到对应的文件
        idx_t nr = bmap(inode, offset / BLOCK_SIZE, false);
        assert(nr);

        // 读取文件缓冲
        buffer_t* bf = bread(inode->dev, nr);

        // 文件在逻辑块中的偏移量
        u32 start = offset % BLOCK_SIZE;

        // 本次需要读取的字节数
        u32 chars = MIN(BLOCK_SIZE - start, left);

        // 更新 偏移值 和 剩余字节数量
        offset += chars;
        left -= chars;

        // 文件逻辑块指针
        char* ptr = bf->data + start;

        // 拷贝
        memcpy(buf, ptr, chars);

        // 更新缓冲位置
        buf += chars;

        // 释放文件块
        brelse(bf);
    } 

    // 更新访问时间
    inode->atime = time();

    // 返回读取数量
    return offset - begin;
}

// 从 inode 的 offset 处，将 buf 的 len 个字节写入磁盘，写的范围超过文件大小，会主动拓容
int inode_write(inode_t *inode, char *buf, u32 len, off_t offset)
{
    assert(ISFILE(inode->desc->mode));

    // 开始位置
    u32 begin = offset;

    // 剩余字节数量
    u32 left = len;
    while (left)
    {
        // 找到对应的文件，不存在就创建
        idx_t nr = bmap(inode, offset / BLOCK_SIZE, true);
        assert(nr);

        // 读入文件块
        buffer_t* bf = bread(inode->dev, nr);
        bf->dirty = true;

        // 文件在逻辑块中的偏移量
        u32 start = offset % BLOCK_SIZE;
        // 文件逻辑块指针
        char* ptr = bf->data + start;

        // 本次需要读取的字节数
        u32 chars = MIN(BLOCK_SIZE - start, left);

        // 更新 偏移值 和 剩余字节数量
        offset += chars;
        left -= chars;

        // 如果偏移量大于文件大小，更新
        if (offset > inode->desc->size)
        {
            inode->desc->size = offset;
            inode->buf->dirty = true;
        }

        // 拷贝
        memcpy(ptr, buf, chars);

        // 更新缓冲位置
        buf += chars;

        // 释放文件块
        brelse(bf);
    } 

    // 更新访问时间
    inode->atime = time();

    bwrite(inode->buf);

    // 返回读取数量
    return offset - begin;
}

// 释放块中的 index 号，有三种等级：直接块、间接块、二级间接块
static void inode_bfree(inode_t* inode, u16* array, int index, int level)
{
    if (!array[index])
        return;
    
    // 0 级，直接释放数组中 index 位置保存的那个块
    if (!level)
    {
        bfree(inode->dev, array[index]);
        return;
    }

    // 不是 1 级，array[index] 的值是另一个块，保存下一级的各个块号
    buffer_t* buf = bread(inode->dev, array[index]);
    for (size_t i = 0; i < BLOCK_INDEXES; i++)
        // 递归调用，传入保存块号的地址，要释放第几个，释放等价递减
        inode_bfree(inode, (u16*)(buf->data), i, level - 1);

    brelse(buf);
    // 把保存块索引的块释放
    bfree(inode->dev, array[index]);
}

// 释放 inode 所有的文件块，清除内容
void inode_truncate(inode_t *inode)
{
    if (!ISFILE(inode->desc->mode) && !ISDIR(inode->desc->mode))
        return;

    // 释放直接块
    for (size_t i = 0; i < DIRECT_BLOCK; ++i)
    {
        inode_bfree(inode, inode->desc->zone, i, 0);
        inode->desc->zone[i] = 0;
    }

    // 释放一级间接块
    inode_bfree(inode, inode->desc->zone, DIRECT_BLOCK, 1);
    
    // 释放二级间接块
    inode_bfree(inode, inode->desc->zone, DIRECT_BLOCK + 1, 2);

    inode->desc->zone[DIRECT_BLOCK] = 0;
    inode->desc->zone[DIRECT_BLOCK + 1] = 0;
    inode->desc->size = 0;
    inode->buf->dirty = true;
    inode->desc->mtime = time();

    bwrite(inode->buf);
}

void inode_init()
{
    for (size_t i = 0; i < INODE_NR; ++i)
    {
        inode_t* inode = inode_table + i;
        inode->dev = EOF;
    }
}