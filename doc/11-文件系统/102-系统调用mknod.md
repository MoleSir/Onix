# 系统调用 mknod

完成以下系统调用：

```c
// 设备抽象为文件
int mknod(char* filename, int mode, int dev);
```

## `mknod` 实现

`mknod` 系统调用用来将 `dev` 对应设备抽象为一个文件 `filename`：

```c
int sys_mknod(char* filename, int mode, int dev)
{
    char* next = NULL;
    char* name = NULL;
    inode_t* dir = NULL;
    inode_t* inode = NULL;
    buffer_t* buf = NULL;
    dentry_t* entry = NULL;
    int ret = EOF;

    // 找到父目录
    dir = named(filename, &next);
    if (!dir)
        goto rollback;

    // 如果文件名为 空
    if (!(*next))
        goto rollback;

    // 权限不足
    if (!permission(dir, P_WRITE))
        goto rollback;

    name = next;
    // 检查 name 是否已经存在
    buf = find_entry(&dir, name, &next, &entry);
    if (buf)
        goto rollback;

    // 新建一个目录项
    buf = add_entry(dir, name, &entry);
    buf->dirty = true;

    // 申请一个 inode（位图）
    entry->nr = ialloc(dir->dev);

    // 获得对应的 inode
    inode = new_inode(dir->dev, entry->nr);
    inode->desc->mode = mode;

    // 如果是字符设备或块设备
    if (ISBLK(mode) || ISCHR(mode)) 
        inode->desc->zone[0] = dev;
    
    ret = 0;

rollback:
    brelse(buf);
    iput(inode);
    iput(dir);
    return ret;
}
```

实现比较简单，看注释；


## `dev_init` 设备初始化

把系统中的几个设备：键盘、显示屏、磁盘等都抽象为文件，保存在 "/dev" 下：

````c
void dev_init()
{
    // 创建目录 "dev"
    mkdir("/dev", 0755);

    device_t* device = NULL;

    // 初始化控制台，为字符文件："/dev/console"，只写
    device = device_find(DEV_CONSOLE, 0);
    mknod("/dev/console", IFCHR | 0200, device->dev);

    // 初始化键盘，为字符文件："/dev/keyboard"，只读
    device = device_find(DEV_KEYBOARD, 0);
    mknod("dev/keyboard", IFCHR | 0400, device->dev);

    char name[32];

    // 初始化磁盘设备，可读可写
    for (size_t i = 0; true; ++i)
    {
        device_t* device = device_find(DEV_IDE_DISK, i);
        if (!device)
            break;

        sprintf(name, "/dev/%s", device->name);
        mknod(name, IFBLK | 0600, device->dev);
    }

    // 初始化块设备，可读可写
    for (size_t i = 0; true; ++i)
    {
        device_t* device = device_find(DEV_IDE_PART, i);
        if (!device)
            break;
        sprintf(name, "/dev/%s", device->name);
        mknod(name, IFBLK | 0600, device->dev);
    }
}
````

实现就是调用 `mknod` 系统调用；