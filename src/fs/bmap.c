#include <ds/bitmap.h>
#include <onix/assert.h>
#include <string.h>
#include <onix/buffer.h>
#include <onix/fs.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

// 分配一个文件块
idx_t balloc(dev_t dev)
{
    super_block_t* sb = get_super(dev);
    assert(sb);

    buffer_t* buf = NULL;
    idx_t bit = EOF;
    bitmap_t map;

    for (size_t i = 0; i < ZMAP_NR; i++)
    {
        buf = sb->zmaps[i];
        assert(buf);

        // 将整个缓冲区作为位图
        bitmap_make(&map, buf->data, BLOCK_SIZE, i * BLOCK_BITS + sb->desc->firstdatazone - 1);
        
        // 从位图扫描一位，即得到一个空闲的块
        bit = bitmap_scan(&map, 1);
        if (bit != EOF)
        {
            // 扫描成功，标记缓冲区脏，中止查找
            assert(bit < sb->desc->zones);
            buf->dirty = true;
            break;
        }
    }
    bwrite(buf);
    return bit;
}

// 释放一个文件块
void bfree(dev_t dev, idx_t idx)
{
    super_block_t* sb = get_super(dev);
    assert(sb);
    assert(idx < sb->desc->zones);

    buffer_t* buf;
    bitmap_t map;

    for (size_t i = 0; i < ZMAP_NR; i++)
    {
        if (idx > BLOCK_BITS * (i + 1))
            continue;

        buf = sb->zmaps[i];
        assert(buf);

        // 将整个缓冲区作为位图
        bitmap_make(&map, buf->data, BLOCK_SIZE, i * BLOCK_BITS + sb->desc->firstdatazone - 1);
        
        // 将 idx 对应的位图设置为 0
        assert(bitmap_test(&map, idx));
        bitmap_set(&map, idx, 0);

        buf->dirty = true;
        break;
    }
    bwrite(buf);
}

// 分配一个文件系统 inode
idx_t ialloc(dev_t dev)
{
    super_block_t* sb = get_super(dev);
    assert(sb);

    buffer_t* buf = NULL;
    idx_t bit = EOF;
    bitmap_t map;

    for (size_t i = 0; i < IMAP_NR; i++)
    {
        buf = sb->imaps[i];
        assert(buf);

        bitmap_make(&map, buf->data, BLOCK_BITS, i * BLOCK_BITS);
        bit = bitmap_scan(&map, 1);
        if (bit != EOF)
        {
            assert(bit < sb->desc->inodes);
            buf->dirty = true;
            break;
        }
    }
    bwrite(buf);
    return bit;
}

// 释放一个文件系统 inode
void ifree(dev_t dev, idx_t idx)
{
    super_block_t* sb = get_super(dev);
    assert(sb);
    assert(idx < sb->desc->inodes);

    buffer_t* buf;
    bitmap_t map;

    for (size_t i = 0; i < IMAP_NR; i++)
    {
        if (idx > BLOCK_BITS * (i + 1))
            continue;

        buf = sb->imaps[i];
        assert(buf);

        // 将整个缓冲区作为位图
        bitmap_make(&map, buf->data, BLOCK_SIZE, i * BLOCK_BITS);
        
        // 将 idx 对应的位图设置为 0
        assert(bitmap_test(&map, idx));
        bitmap_set(&map, idx, 0);

        buf->dirty = true;
        break;
    }
    bwrite(buf);
}