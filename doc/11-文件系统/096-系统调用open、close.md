# 系统调用 open,close

完成以下系统调用，都是调用之前的函数，加上对文件数组的一点点操作：

```c++
// 打开文件
fd_t open(char *filename, int flags, int mode);
// 创建普通文件
fd_t creat(char *filename, int mode);
// 关闭文件
void close(fd_t fd);
```

注册系统调用的过程就不看了，直接看处理函数；


## 打开文件

````c
fd_t sys_open(char* filename, int flags, int mode)
{
    // 调包侠罢了
    inode_t* inode = inode_open(filename, flags, mode);
    if (!inode)
        return EOF;

    task_t* task = running_task();
    // 申请一个空的文件描述符指针，返回索引
    fd_t fd = task_get_fd(task);
    // 申请一个空的文件描述符结构体，返回指针
    file_t* file = get_file();
    assert(task->files[fd] == NULL);
    task->files[fd] = file;

    // 配置基本信息
    file->inode = inode;
    file->flags = flags;
    file->count = 1;
    file->mode = inode->desc->mode;
    file->offset = 0;

    // 追加打开
    if (flags & O_APPEND)
        file->offset = file->inode->desc->size;

    return fd;
}
````


## 创建文件

````c
fd_t sys_create(char* filename, int mode)
{
    return sys_open(filename, O_CREAT | O_TRUNC, mode);
}
````


## 关闭文件

```c
void sys_close(fd_t fd)
{
    assert(fd < TASK_FILE_NR);
    task_t* task = running_task();
    // 得到文件结构体指针
    file_t* file = task->files[fd];
    if (!file)
        return;

    assert(file->inode);
    // 释放文件结构体
    put_file(file);
    // 释放进程文件结构体指针
    task_put_fd(task, fd);
}
```