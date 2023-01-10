#include <onix/fs.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/device.h>
#include <onix/buffer.h>
#include <onix/stat.h>
#include <string.h>

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