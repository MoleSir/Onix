#include <onix/fs.h>
#include <onix/assert.h>
#include <onix/task.h>
#include <onix/device.h>
#include <onix/types.h>

#define FILE_NR 128

file_t file_table[FILE_NR];

file_t* get_file()
{
    for (size_t i = 0; i < FILE_NR; ++i)
    {
        file_t* file = file_table + i;
        if (!(file->count))
        {
            file->count++;
            return file;
        }
    }
    panic("Exceed max open files!!!");
}

void put_file(file_t* file)
{
    assert(file->count > 0);
    file->count--;
    if (!(file->count))
        iput(file->inode);
}

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

fd_t sys_create(char* filename, int mode)
{
    return sys_open(filename, O_CREAT | O_TRUNC, mode);
}

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

int sys_lseek(fd_t fd, off_t offset, int whence)
{
    assert(fd < TASK_FILE_NR);
    // 得到文件
    task_t* task = running_task();
    file_t* file = task->files[fd];

    assert(file);
    assert(file->inode);

    // 根据 whence 跳转类型
    switch (whence)
    {
    case SEEK_SET:
        assert(offset >= 0);
        file->offset = offset;
        break;
    case SEEK_CUR:
        assert(file->offset + offset > 0);
        file->offset += offset;
        break;
    case SEEK_END:
        // 超过总大小也没关系，inode_write 会拓容
        assert(file->inode->desc->size + offset >= 0);
        file->offset = file->inode->desc->size + offset;
        break;
    default:
        panic("whence not defined!!!");
        break;
    }
    return file->offset;
}

void file_init()
{
    for (size_t i = 0; i < FILE_NR; ++i)
    {
        file_t* file = file_table + i;
        file->count = 0;
        file->mode = 0;
        file->flags = 0;
        file->offset = 0;
        file->inode = NULL;
    }
}