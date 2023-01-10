#include <onix/fs.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/device.h>
#include <onix/buffer.h>
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

// 读设备 dev 的超级块
super_block_t* read_super(dev_t dev)
{
    // 检查 dev 的超级块是否已经被添加过
    super_block_t* sb = get_super(dev);
    if (sb)
        return sb;

    LOGK("Reading super block of device %d\n", dev);

    // 获得一个空的块
    sb = get_free_super();

    // 设备的第一块保存超级块信息（0 是 boot）
    buffer_t* buf = bread(dev, 1);

    // 设置超级块信息
    sb->buf = buf;
    sb->desc = (super_desc_t*)(buf->data);
    sb->dev = dev;

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