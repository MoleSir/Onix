# 截断 inode

实现：

````c
void inode_truncate(inode_t *inode)
````

用释放 inode 指向那个文件的所有的文件块，清除内容，大小变为 0，但文件没有消失；


````c++
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
````

使用了递归的方法，很巧妙！