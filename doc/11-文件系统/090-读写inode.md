# 读写 inode

主要完成以下函数：

```c++
// 从 inode 的 offset 处，读 len 个字节到 buf
int inode_read(inode_t *inode, char *buf, u32 len, off_t offset)

// 从 inode 的 offset 处，将 buf 的 len 个字节写入磁盘
int inode_write(inode_t *inode, char *buf, u32 len, off_t offset)
```

## 读文件

将 inode 指向的那个文件，从起始位置开始偏移 offset 个字节的位置拷贝 len 个字节到 buf 中：

````c
int inode_read(inode_t *inode, char *buf, u32 len, off_t offset)
{
    assert(ISFILE(inode->desc->mode) || ISDIR(inode->desc->mode));

    // 判断文件偏移量
    if (offset > inode->desc->size)
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
````

逻辑与功能都比较简单，不多说了。需要注意的就是，文件在磁盘中的各个块是不一定连续的，所以可能需要多次读取，一次最多读一块大小；


## 写文件

将 buf 中的 len 个字节，写入到 inode 对应文件的 offset 位置（写到文件内容里）：

```c
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
```

逻辑与功能都比较简单，不多说了。需要注意的就是，文件在磁盘中的各个块是不一定连续的，所以可能需要多次写入，一次最多写一块大小；