# 文件系统 inode

在 084-根超级块中，可以读取到磁盘的超级块信息与两张位图到内存中使用；在 085-文件系统位图操作中，可以根据位图申请和释放一个文件块或某个 inode 所在的逻辑块；

但只是申请，并没有把 inode 从内存中读取出来，这次就要完成这个功能，主要完成以下几个函数：

```c++
inode_t *iget(dev_t dev, idx_t nr); // 获得设备 dev 的第 nr 号 inode
void iput(inode_t *inode);          // 释放某个 inode

// 获取 inode 第 block 块的索引值
// 如果不存在 且 create 为 true，则创建
idx_t bmap(inode_t *inode, idx_t block, bool create);
```

## inode 结构体

与超级块类似地，除了定义描述 inode 在磁盘中的布局，为了可以更好使用，还需要一些其他信息，所以增加一个结构体描述读入内存中的 inode：

````c
typedef struct inode_t
{
    inode_desc_t* desc;     // inode 描述符
    struct buffer_t* buf;   // inode 描述符对应的 buffer
    dev_t dev;              // 设备号
    idx_t nr;               // i 节点号
    u32 count;              // 引用计数
    time_t atime;           // 访问时间
    time_t ctime;           // 创建时间
    list_node_t node;       // 链表节点
    dev_t mount;            // 安装设备
} inode_t;
````

包含了：inode 磁盘信息、inode 对应的 buffe、此 inode 属于哪个文件系统、inode 位于文件系统的哪个 inode 节点、引用计数等信息；

并且在内存中创建一个 `inode_t` 结构体数组，保存所有读入到内存中的 inode 信息，定义在 inode.c 中：

````c
#define INODE_NR 64
static inode_t inode_table[INODE_NR];
````

并且在超级块结构体中添加一个字：`inode_list`，一条由 `inode_t` 结构体组成的链表：

````c
typedef struct super_block_t
{
    super_desc_t* desc;             // 超级块描述符
    struct buffer_t* buf;           // 超级快描述符 buffer
    struct buffer_t* imaps[IMAP_NR];// inode 位图缓冲
    struct buffer_t* zmaps[ZMAP_NR];// 块位图缓冲
    dev_t dev;                      // 设备号
    list_t inode_list;              // 该文件系统中使用中的 inode 链表
    inode_t* iroot;                 // 根目录的 inode
    inode_t* imount;                // 安装到的 inode
} super_block_t;
````

两个数据结构体都保存这读入内存中的 inode，这里有点区别：一个超级块代表一个文件系统，所以在超级块中的 `inode_t` 链表表示的是这个文件系统读入到内存中的所有 inode；

而全局的 inode 数组是表示整个系统中（可能存在多个文件系统）读入的 inode。

## 申请、释放 inode_t 结构体

有了数组后，就需要两个函数分别获得、释放一个空闲的 `inode_t` 结构体：

````c
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
````

## 计算 nr 号 inode 对应的磁盘号

````c
static inline idx_t cal_inode_block(super_block_t* sb, idx_t nr)
{
    // inode 的 nr 从 1 开始编址
    // 磁盘结构：主引导 + 超级块 + inode 位图 + 文件块位图 + (nr-1) / 一块的 inode 数量
    return 2 + sb->desc->imap_block + sb->desc->zmap_block + (nr - 1) / BLOCK_INODES;
}
````

## 获得已经存在的 inode

从某个文件系统中获得已经读入内存的 inode，就去超级块中的链表寻找即可：

````c
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
````


## iget 实现

传入 dev 与 nr，表示要获得 dev 设备文件系统的 nr 号 inode 的信息：

````c
inode_t* iget(dev_t dev, idx_t nr)
{
    // 尝试在已经存在的 inode 寻找
    inode_t* inode = find_exist_inode(dev, nr);
    if (inode)
    {
        // 找到了，引用计数增加，更新访问时间为当前
        inode->count++;
        inode->atime = time();

        return inode;
    }

    // 没有在已读取的 inode 找到，就创建一个
    super_block_t* sb = get_super(dev);
    assert(sb);
    // 得到一个空闲的 inode 结构体
    inode = get_free_inode_struct();
    // 配置 inode 的属性
    inode->dev = dev;
    inode->nr = nr;
    inode->count = 1;

    // 加入本设备的 inode 链表
    list_push(&(sb->inode_list), &(inode->node));

    // 找到该 inode 在磁盘中的块
    idx_t block = cal_inode_block(sb, inode->nr);
    // 读取整个块
    buffer_t* buf = bread(inode->dev, block);
    // 配置 inode 的缓冲区
    inode->buf = buf;

    // 一个块保存多个 inode，找此 inode 的位置
    // 将缓冲视为一个 inode 描述符数组，获得对应指针，求 nr 在一个块中的索引
    inode->desc = (inode_desc_t*)buf->data + ((inode->nr - 1) % BLOCK_INODES); 
    // 更新时间
    inode->ctime = inode->desc->mtime;
    inode->atime = time();

    return inode;
}
````

1. 先使用 `find_exist_inode` 查找这个 inode 是不是已经读入了，是就可以直接返回；
2. 还没读入，先向 `inode_table` 申请一个空闲结构体使用；
3. 设置一些基本参数：这个 inode 是什么设备的，inode 号是多少等；
4. 链入该文件系统的 inode 链表；
5. 根据 nr 号计算 inode 在磁盘中所在的逻辑块号；
6. 使用 `bread` 读取 dev 设备的 block 块到内存，并且将缓冲区设置给 inode，这样可以通过 `inode->buf->data` 找到 inode 所在逻辑块的内容；
7. 一个块有多个 inode 信息，计算需要的 inode 在块中的偏移，最后得到 inode 在磁盘中的信息；
8. 设置 inode 的时间信息，最后返回这个 `inode_t` 结构体指针；


## iput 实现

将一个 `inode_t` 结构体释放：

````c
void iput(inode_t* inode)
{
    if (!inode)
        return;
    
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
````

1. 引用计数减一；
2. 如果已经没有进程使用这个文件，就需要释放内存空间；
3. 先释放这个 inode 所在逻辑块的缓冲；
4. 从超级块链表中移除；
5. 把 inode 所占据的 `inode_table` 数组空间释放。

