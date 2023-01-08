# 系统调用 link,unlink

完成以下系统调用：

```c++
// 创建文件硬链接
int link(char *oldname, char *newname);

// 删除文件
int unlink(char *filename);
```

## 创建文件硬链接

将 oldname 对应的那个文件，增加一个硬链接：newname，即 newname 与 oldname 找到的 都指向同一个 inoed

````c
int sys_link(char* oldname, char* newname)
{
    int ret = EOF;
    buffer_t* buf = NULL;
    inode_t* dir = NULL;
    inode_t* inode = NULL;
    char* name = NULL;
    char* next = NULL;
    dentry_t* entry = NULL;

    // 找到 oldname 对应的 inode
    inode = namei(oldname);
    if (!inode)
        goto rollback;

    // 只能对文件硬链接，不能对目录
    if (ISDIR(inode->desc->mode))
        goto rollback;
    
    // 得到要创建的新目录的父目录，next 保存 oldname 的最后一级名称
    dir = named(newname, &next);
    if (!dir)
        goto rollback;

    // 名称不存在
    if (!(*next))
        goto rollback;

    // 不同设备不允许硬链接
    if (dir->dev != inode->dev)
        goto rollback;
    
    // 没有写权限
    if (!permission(dir, P_WRITE))
        goto rollback;

    // 此时 next 为最后生成的文件名称，判断其是不是已经存在
    name = next;
    buf = find_entry(&dir, name, &next, &entry);
    if (buf)
        goto rollback;

    // 添加一个新目录项
    buf = add_entry(dir, name, &entry);
    // 新目录项与 olename 文件指向同一个 inode
    entry->nr = inode->nr;
    buf->dirty = true;

    // 文件新增一个引用
    inode->desc->nlinks++;
    inode->ctime = time();
    inode->buf->dirty = true;
    ret = 0;

rollback:
    brelse(buf);
    iput(inode);
    iput(dir);
    return ret;
}
````

过程注释详细；


## 删除文件

删除 filename 对应的 dentry，可能会把指向的文件也删了，也可能不会，要根据硬链接数判断：

````c
int sys_unlink(char* filename)
{
    int ret = EOF;
    char* next = NULL;
    char* name = NULL;
    inode_t* inode = NULL;
    buffer_t* buf = NULL;
    dentry_t* entry = NULL;

    // 找到 filename 目录的 inode
    inode_t* dir = named(filename, &next);
    if (!dir)
        goto rollback;

    // 文件名称不存在
    if (!(*next))
        goto rollback;

    // 权限
    if (!permission(dir, P_WRITE))
        goto rollback;

    // 找到 filename 对应的 inode
    name = next;
    buf = find_entry(&dir, name, &next, &entry);
    if (!buf)
        goto rollback;

    // 根据 entry 中的 nr 找到 inode
    inode = iget(dir->dev, entry->nr);
    if (ISDIR(inode->desc->mode))
        goto rollback;
    
    task_t* task = running_task();
    if ((inode->desc->mode & ISVTX) && task->uid != inode->desc->uid)
        goto rollback;

    if (!inode->desc->nlinks)
        LOGK("deleting non exists file (%04x:%d)\n", inode->dev, inode->nr);

    // 目录项指向空 nr
    entry->nr = 0;
    buf->dirty = true;

    // 文件链接数量 -1
    inode->desc->nlinks--;
    inode->buf->dirty = true;

    if (inode->desc->nlinks == 0)
    {
        // 文件块可以不要了！
        inode_truncate(inode);
        // 释放这个 inode （位图）
        ifree(inode->dev, inode->nr);
    }

rollback:
    brelse(buf);
    iput(inode);
    iput(dir);
    return ret;
}
````

过程注释详细；
