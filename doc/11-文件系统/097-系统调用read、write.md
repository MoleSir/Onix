# 系统调用 read,write

完成以下系统调用：

```c++
// 读文件
int read(fd_t fd, char *buf, int len);
// 写文件
int write(fd_t fd, char *buf, int len);
```

其中的 fd 表示向当前进程的 fd 号文件写或读，这里把输入输出设备也抽象为进程的文件，所以 `read` 与 `write` 可以读键盘、写显示屏，只要传入 `stdin` 表示标准输入设备文件号、`stdout` 表示标准输出设备文件号，`printf` 就是这样做的：

```c
int printf(const char* fmt, ...)
{
    va_list args;
    int i;
    va_start(args, fmt);
    i = vsprintf(buf, fmt, args);
    va_end(args);
    write(stdout, buf, i);
    return i;
}
```
跳过注册系统调用过程，看处理函数实现；


## `write` 实现 

````c
u32 sys_write(fd_t fd, char* buf, u32 count)
{
    // 输出显示器
    if (fd == stdout || fd == stderr)
    {
        device_t* device = device_find(DEV_CONSOLE, 0);
        return device_write(device->dev, buf, count, 0, 0);
    }

    // 获得文件结构体
    task_t* task = running_task();
    file_t* file = task->files[fd];
    assert(file);
    assert(count > 0);

    // 如果只读，错误返回
    if ((file->flags & O_ACCMODE) == O_RDONLY)
        return EOF;

    inode_t* inode = file->inode;
    int len = inode_write(inode, buf, count, file->offset);
    if (len != EOF)
        file->offset += len;
    
    return len;
}
````

首先判断了是否为标准输出，如果是就获得显示器设备，然后写入；

之后就是获取到进程的 fd 号文件，调用 `inode_write`；


## `read` 实现

````c
u32 sys_read(fd_t fd, char* buf, u32 count)
{
    // 如果是输入设备
    if (fd == stdin)
    {
        // 获得键盘设备，使用虚拟设备读取
        device_t* device = device_find(DEV_KEYBOARD, 0);
        return device_read(device->dev, buf, count, 0, 0);
    }

    // 获得文件结构体
    task_t* task = running_task();
    file_t* file = task->files[fd];
    assert(file);
    assert(count > 0);

    // 如果只写，错误返回
    if ((file->flags & O_ACCMODE) == O_WRONLY)
        return EOF;

    // 调用 inode_read 读取磁盘文件
    inode_t* inode = file->inode;
    int len = inode_read(inode, buf, count, file->offset);
    if (len != EOF)
        file->offset += len;
    
    return len;
}
````

首先判断了是否为标准输入，如果是就获得键盘设备，然后写入；

之后就是获取到进程的 fd 号文件，调用 `inode_read`；
