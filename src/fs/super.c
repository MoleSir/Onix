#include <onix/fs.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/device.h>
#include <onix/buffer.h>
#include <string.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

void super_init()
{
    // 找到磁盘的第0个分区，这个分区的 0 块是 bootloader，第 1 块是超级块
    device_t* device = device_find(DEV_IDE_PART, 0);
    assert(device);

    // 读取超级块与 bootloader
    buffer_t* boot = bread(device->dev, 0);
    buffer_t* super = bread(device->dev, 1);

    // 解释为超级块
    super_desc_t* sb = (super_desc_t*)(super->data);
    assert(sb->magic == MINIX1_MAGIC);

    // inode 位图位于第 2 块
    buffer_t* imap = bread(device->dev, 2);
    
    // 块位图位于 inode 后一块，需要加上 inode 的大小
    buffer_t* zmap = bread(device->dev, 2 + sb->imap_block);

    // 读取第一个 inode 块
    buffer_t* buf1 = bread(device->dev, 2 + sb->imap_block + sb->zmap_block);
    inode_desc_t* inode = (inode_desc_t*)(buf1->data);

    // 读取第一个文件的第 0 块
    buffer_t* buf2 = bread(device->dev, inode->zone[0]);

    // 第一个文件是根目录，解释为 dentry_t
    dentry_t* dir = (dentry_t*)(buf2->data);
    inode_desc_t* helloi = NULL;
    // 依次查找 dir 中的每一个 dentry_t 表项
    while (dir->nr)
    {
        LOGK("inode %04d, name %s\n", dir->nr, dir->name);
        if (!strcmp(dir->name, "hello.txt"))
        {
            // 得到文件的 inode 数组索引，到其中数组查找，索引的 1 代表地址上的 0，因为 0 号索引代表结束
            helloi = (inode_desc_t*)(buf1->data) + (dir->nr - 1);
            strcpy(dir->name, "word.txt");
        }
        dir++;
    }

    // 根据文件 inode 保存的块号，读取文件内容
    buffer_t* buf3 = bread(device->dev, helloi->zone[0]);
    LOGK("content %s", buf3->data);

    // 修改文件，重新把文件数据写回块
    strcpy(buf3->data, "This is modified context!!!\n");
    buf3->dirty = true;
    bwrite(buf3);

    // 文件内容改变，inode 中的数据也需要改，重新写 inode 
    helloi->size = strlen(buf3->data);
    buf1->dirty = true;
    bwrite(buf1);

    // 文件名称被修改，保存文件的文件目录内容被修改
    buf2->dirty = true;
    bwrite(buf2);
}