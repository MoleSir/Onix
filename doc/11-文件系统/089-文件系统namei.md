# 文件系统 namei

主要完成以下函数：

```c++
// 获取 pathname 对应的父目录 inode
static inode_t *named(char *pathname, char **next);

// 获取 pathname 对应的 inode
static inode_t *namei(char *pathname);
```

## `named` 实现

这个函数的功能是：传入一个目录，获得这个目录对应文件（或目录）的父目录 inode，并且返回该目录下一级的名称

比如 `pathname` 等于 "/home/d1/hh.c"，返回 "home/d1" 这个目录的 inode，`next` 保存 "hh.c"；

````c
inode_t *named(char *pathname, char **next)
{
    inode_t* inode = NULL;
    task_t* task = running_task();
    char* left = pathname;

    // 第一个字符是分隔符，说明从根目录开始
    if (IS_SEPARATOR(left[0]))
    {
        // 说明，inode 就是进程的根目录
        inode = task->iroot;
        // 跳过分隔符
        left++;
    }

    // left 为空，表示当前目录
    else if (left[0])
        inode = task->ipwd;
    else
        return NULL;

    // 如果是根目录就已经把 '/' 去掉了
    inode->count++;

    // *next 表示 pathname 去掉根目录后的字符串
    *next = left;

    // 没有子目录，直接返回根目录，或当前目录
    if (!*left)
        return inode;

    // 存在子目录，找到路径中最右侧的分隔符
    char* right = strrsep(left);
    if (!right || right < left)
        return inode;
    
    // 跳过分隔符，得到最后一级名称
    right++;

    *next = left;
    dentry_t* entry = NULL;
    buffer_t* buf = NULL;
    while (true)
    {
        // inode 是路径的最高目录，left 是去掉最高目录的路径，调用 find_entry
        // 获得 left 的 inode 与 buf，如果 left 还不是最后一级，把之后的路径放入 next 返回
        buf = find_entry(&inode, left, next, &entry);

        // 没找到，失败
        if (!buf)
            goto failure;
        
        // 找到了，获取 left 对应的 inode
        dev_t dev = inode->dev;
        iput(inode);
        inode = iget(dev, entry->nr);

        // 如果不是目录或权限不允许，失败
        if (!ISDIR(inode->desc->mode) || !permission(inode, P_EXEC))
            goto failure;
        
        // 如果此时，最后一级名称等于 left 的后级，说明找到了，成功
        if (right == *next)
            goto success;

        // 不成功、也不失败，继续找下一级
        left = *next;
    }

success:
    brelse(buf);
    return inode;

failure:
    brelse(buf);
    iput(inode);
    return NULL;
}
````


## `namei` 实现

获得传入路径的 inode：

````c
inode_t *namei(char *pathname)
{
    char* next = NULL;
    // 先获得 pathname 次高级目录，比如 "/home/hello.c"，这里先得到 "/home" 的 inode
    inode_t* dir = named(pathname, &next);
    if (!dir)
        return NULL;
    if (!(*next))
        return dir;

    // 再到此高级目录中找
    char* name = next;
    dentry_t* entry = NULL;
    // 调用 find_entry 寻找
    buffer_t* buf = find_entry(&dir, name, &next, &entry);
    if (!buf)
    {
        iput(dir);
        return NULL;
    }

    inode_t* inode = iget(dir->dev, entry->nr);

    iput(dir);
    brelse(buf);
    return inode;
}
````