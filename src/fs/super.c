#include <onix/fs.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/device.h>
#include <onix/buffer.h>
#include <onix/stat.h>
#include <onix/task.h>
#include <onix/time.h>
#include <string.h>
#include <stdlib.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define SUPER_NR 16

// 超级块表，一个超级块描述一个文件系统，所以系统支持 SUPER_NR 个文件系统挂载
static super_block_t super_table[SUPER_NR];
// 根文件系统超级块
static super_block_t* root;

// 从超级块表查找一个空闲块
static super_block_t* get_free_super()
{
    for (size_t i = 0; i < SUPER_NR; ++i)
    {
        super_block_t* sb = super_table + i;
        if (sb->dev == EOF)
            return sb;
    }
    panic("no more super block!!!");
}

// 获取设备 dev 的超级快
super_block_t* get_super(dev_t dev)
{
    for (size_t i = 0; i < SUPER_NR; ++i)
    {
        super_block_t* sb = super_table + i;
        if (sb->dev == dev)
            return sb;
    }
    return NULL;
}

// 
void put_super(super_block_t* sb)
{
    if (!sb)
        return;
    assert(sb->count > 0);
    sb->count --;
    if (sb->count)
        return;

    sb->dev = EOF;
    iput(sb->imount);
    iput(sb->iroot);

    for (int i = 0; i < sb->desc->imap_block; ++i)
        brelse(sb->imaps[i]);

    for (int i = 0; i < sb->desc->zmap_block; ++i)
        brelse(sb->zmaps[i]);

    brelse(sb->buf);
}

// 读设备 dev 的超级块
super_block_t* read_super(dev_t dev)
{
    // 检查 dev 的超级块是否已经被添加过
    super_block_t* sb = get_super(dev);
    if (sb)
    {
        sb->count++;
        return sb;
    }

    LOGK("Reading super block of device %d\n", dev);

    // 获得一个空的块
    sb = get_free_super();

    // 设备的第一块保存超级块信息（0 是 boot）
    buffer_t* buf = bread(dev, 1);

    // 设置超级块信息
    sb->buf = buf;
    sb->desc = (super_desc_t*)(buf->data);
    sb->dev = dev;
    sb->count = 1;

    assert(sb->desc->magic == MINIX1_MAGIC);

    memset(sb->imaps, 0, sizeof(sb->imaps));
    memset(sb->zmaps, 0, sizeof(sb->zmaps));

    // 超级块中的信息指明了位图的位置，但具体的内容还是需要再读取
    // 读取 inode 位图，逻辑块索引从 2 开始，0 是引导、1 是超级块
    int idx = 2;
    // 读取 imap_block 个块
    for (int i = 0; i < sb->desc->imap_block; ++i)
    {
        // 每一块需要一个 buffer_t
        assert(i < IMAP_NR);
        if ((sb->imaps[i] = bread(dev, idx)))
            idx++;
        else    
            break;
    }

    // 读块位图，读取 zmap_block 个块
    for (int i = 0; i < sb->desc->zmap_block; ++i)
    {
        assert(i < IMAP_NR);
        if ((sb->zmaps[i] = bread(dev, idx)))
            idx++;
        else    
            break;
    }

    return sb;
}

// 挂载根文件系统
void mount_root()
{
    LOGK("Mount root file system...\n");

    // 读取磁盘的超级块信息
    device_t* device = device_find(DEV_IDE_PART, 0);
    assert(device);
    // 传入磁盘设备号读取
    root = read_super(device->dev);

    // 初始化根目录 inode
    // 根目录的 inode
    root->iroot = iget(device->dev, 1);
    // 根据目录挂载 inode，就是根目录本身
    root->imount = iget(device->dev, 1);
}

void super_init()
{
    for (size_t i = 0; i < SUPER_NR; ++i)
    {
        super_block_t* sb = super_table + i;
        sb->dev = EOF;
        sb->desc = NULL;
        sb->buf = NULL;
        sb->iroot = NULL;
        sb->imount = NULL;
        list_init(&(sb->inode_list));
    }

    mount_root();
}

// 将 devname 文件对应的设备挂载到 dirname 目录下
int sys_mount(char *devname, char *dirname, int flags)
{
    LOGK("mount %s to %s\n", devname, dirname);

    inode_t* devinode = NULL;
    inode_t* dirinode = NULL;
    super_block_t* sb = NULL;
    
    // 打开 devname 设备名对应的文件 inode
    devinode = namei(devname);
    if (!devinode)
        goto rollback;
    if (!ISBLK(devinode->desc->mode))
        goto rollback;

    // devname 的设备号，约定保存在文件的 zone[0] 处
    dev_t dev = devinode->desc->zone[0];

    // 获得挂载目录的 inode
    dirinode = namei(dirname);
    if (!dirinode)
        goto rollback;
    // 只能挂载目录下
    if (!ISDIR(dirinode->desc->mode));
    // 已经有挂载目录了
    if (dirinode->count != 1 || dirinode->mount)
        goto rollback;

    sb = read_super(dev);
    // 如果已经被挂载了！
    if (sb->imount)
        goto rollback;

    // 超级快的根为第一个块
    sb->iroot = iget(dev, 1);
    // 超级快的挂载目录为 dirinode
    sb->imount = dirinode;
    // 目录的挂载设备号为 dev
    dirinode->mount = dev;
    iput(devinode);
    return 0;

rollback:
    put_super(sb);
    iput(devinode);
    iput(dirinode);
    return EOF;
}

int sys_umount(char *target)
{
    LOGK("umount %s\n", target);
    inode_t *inode = NULL;
    super_block_t *sb = NULL;
    int ret = EOF;

    inode = namei(target);
    if (!inode)
        goto rollback;

    if (!ISBLK(inode->desc->mode) && inode->nr != 1)
        goto rollback;

    if (inode == root->imount)
        goto rollback;

    dev_t dev = inode->dev;
    if (ISBLK(inode->desc->mode))
    {
        dev = inode->desc->zone[0];
    }

    sb = get_super(dev);
    if (!sb->imount)
        goto rollback;

    if (!sb->imount->mount)
    {
        LOGK("warning super block mount = 0\n");
    }

    if (list_size(&sb->inode_list) > 1)
        goto rollback;

    iput(sb->iroot);
    sb->iroot = NULL;

    sb->imount->mount = 0;
    iput(sb->imount);
    sb->imount = NULL;
    ret = 0;

rollback:
    put_super(sb);
    iput(inode);
    return ret;
}

int devmkfs(dev_t dev, u32 icount)
{
    super_block_t* sb = NULL;
    buffer_t* buf = NULL;
    int ret = EOF;

    // 向 dev 设备发送获得扇区数量命令
    int total_block = device_ioctl(dev, DEV_CMD_SECTOR_COUNT, NULL, 0) / BLOCK_SECS; 
    assert(total_block);
    assert(icount < total_block);

    // 如果传入的参数没有指定 inode 数量，设置为总块数的 1/3
    if (!icount)
        icount = total_block / 3;

    // 申请一个空超级块
    sb = get_free_super();
    sb->dev = dev;
    sb->count = 1;

    // 获得超级块的缓冲
    buf = bread(dev, 1);
    sb->buf = buf;
    buf->dirty = true;

    super_desc_t* desc = (super_desc_t*)(buf->data);
    sb->desc = desc;

    // 计算磁盘中的 inode 要占几个块
    int inode_blocks = div_round_up(icount * sizeof(inode_desc_t), BLOCK_SIZE);
    desc->inodes = icount;
    desc->zones = total_block;
    // 计算 inode 位图要占几个块
    desc->imap_block = div_round_up(icount, BLOCK_BITS);

    // 文件块数量 = 总块数 - inode 位图块数 - indeo 信息包含的块数 - 超级块 - 引导块
    int zcount = total_block - desc->imap_block - inode_blocks - 2;
    // 计算文件块位图要几个块
    desc->zmap_block = div_round_up(zcount, BLOCK_BITS);

    // 文件的起始块号 = 引导 + 超级 + inode 位图块 + 文件位图块 + inode 信息块
    desc->firstdatazone = 2 + desc->imap_block + desc->zmap_block + inode_blocks;
    desc->log_zone_size = 0;
    desc->max_size = BLOCK_SIZE * TOTAL_BLOCK;
    desc->magic = MINIX1_MAGIC;
    
    // 清空数组
    memset(sb->imaps, 0, sizeof(sb->imaps));
    memset(sb->zmaps, 0, sizeof(sb->zmaps));

    // inode 位图从 2 号块开始
    int idx = 2;
    for (int i = 0; i < sb->desc->imap_block; ++i)
    {
        // 依次读一块作为位图，全部设置为 0
        if (sb->imaps[i] = bread(dev, idx))
        {
            memset(sb->imaps[i]->data, 0, BLOCK_SIZE);
            // 设置缓冲为脏，需要写回磁盘
            sb->imaps[i]->dirty = true;
            idx++;
        }
        else 
            break;
    }
    // 对文件块位图同理，只不过其开始的磁盘块不确定，所以需要写完 inode 后再随后的块写
    for (int i = 0; i < sb->desc->zmap_block; ++i)
    {
        if (sb->zmaps[i] = bread(dev, idx))
        {
            memset(sb->zmaps[i]->data, 0, BLOCK_SIZE);
            sb->zmaps[i]->dirty = true;
            idx++;
        }
        else
            break;
    }

    // 初始化位图
    // 申请到第一个文件块，作为根目录的内容（两个 dentry_t）
    // balloc 函数会修改文件位图块的信息，会将位图信息写回磁盘
    idx = balloc(dev);

    // 申请两个 indoe，0 号 inode 空余不用，1 号作为根目录 inode
    // ialloc 函数会修 inode 位图块的信息，会将位图信息写回磁盘
    idx = ialloc(dev);
    idx = ialloc(dev);

    // 两张位图的最后若干位都是无效的，因为没有那么多块，而计算位图块数的时候是向上进的
    int counts[] = {
        // icount + 1，加 1 是因为让 0 号 inode 空余不用，那么实际空间就要多用一块位置
        icount + 1,
        zcount,
    };

    // 取两张位图的最后一块，这些的部分 bit 是无效的
    buffer_t* maps[] = {
        sb->imaps[sb->desc->imap_block - 1],
        sb->zmaps[sb->desc->zmap_block - 1],
    };
    for (size_t i = 0; i < 2; ++i)
    {
        int count = counts[i];
        buffer_t* map = maps[i];
        map->dirty = true;

        // 无效位开始的 bit 序号
        int offset = count % (BLOCK_BITS);
        // 无效位开始的字节序号，但这个字节并不是所有的都无效
        int begin = (offset / 8);
        // 获得起始无效字节的地址
        char* ptr = (char*)map->data + begin;

        // 这个字节之后的所有字节都无效，直接设置位全 1，即每个字节都是 0xff
        memset(ptr + 1, 0xFF, BLOCK_SIZE  - begin - 1);
        
        // 0x80 = 0b 1000 0000，无效位从最高位开始设置
        int bits = 0x80;
        char data = 0;
        // % 8 相当于取第 3 bits，offset 是无效位在整个块的 bit 中的起始位置
        // % 8 后就相当于在这个字节中的起始位置；
        // 所以这个字节的 offset % 8 开始以后都需要是 1
        // 需要设置 8 - offset % 8 个 bit 位
        int remain = 8 - offset % 8;

        // 设置 remain 次，从最高位开始，每次 bits 右移 1 位，即设置下一关 bit
        while (remain--)
        {
            data |= bits;
            bits >> 1;
        }
        ptr[0] = data;
    }

    // 创建根目录
    task_t* task = running_task();

    // 取 1 号 inode
    inode_t* iroot = iget(dev, 1);
    sb->iroot = iroot;

    iroot->desc->gid = task->gid;
    iroot->desc->uid = task->uid;
    iroot->desc->mode = (0777 & ~task->umask) | IFDIR;
    // 两个目录，所以文件大小是两个目录项
    iroot->desc->size = sizeof(dentry_t) * 2;
    iroot->desc->mtime = time();
    iroot->desc->nlinks = 2;

    // 得到 根目录的 0 号文件块
    buf = bread(dev, bmap(iroot, 0, true));
    buf->dirty = true;

    // 文件块解释为 dentry_t 
    dentry_t* entry = (dentry_t*)(buf->data);
    memset(entry, 0, BLOCK_SIZE);

    // 第一个 dentry_t，为 "."，指向根目录 inode 序号
    strcpy(entry->name, ".");
    entry->nr = iroot->nr;

    // 第二个 dentry_t，为 ".."，指向根目录 inode 序号
    entry++;
    strcpy(entry->name, "..");
    entry->nr = iroot->nr;

    // 释放 buf，写回磁盘
    brelse(buf);
    ret = 0;

rollback:
    // put super 会将内容写回磁盘
    put_super(sb);
    return ret;    
}


int sys_mkfs(char* devname, int icount)
{
    inode_t* inode = NULL;
    int ret = EOF;

    // 得到 devname 的 inode
    inode = namei(devname);
    if (!inode) 
        goto rollback;
    
    // 要求 inode 为块设备
    if (!ISBLK(inode->desc->mode))
        goto rollback;

    // 取出这个文件对应设备的设备号
    dev_t dev = inode->desc->zone[0];
    assert(dev);

    // 调用 devmkfs
    ret = devmkfs(dev, icount);

rollback:
    iput(inode);
    return ret;
}