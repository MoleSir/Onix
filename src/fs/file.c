#include <onix/fs.h>
#include <onix/assert.h>
#include <onix/task.h>
#include <onix/device.h>
#include <onix/stat.h>
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
    int len = 0;
    // 获得文件结构体
    task_t* task = running_task();
    file_t* file = task->files[fd];
    assert(file);
    assert(count > 0);

    // 如果只写，错误返回
    if ((file->flags & O_ACCMODE) == O_WRONLY)
        return EOF;

    inode_t* inode = file->inode;

    // 管道文件
    if (inode->pipe)
    {
        len = pipe_read(inode, buf, count);
        return len;
    }
    // 字符文件
    else if (ISCHR(inode->desc->mode))
    {
        assert(inode->desc->zone[0]);
        // 读设备
        len = device_read(inode->desc->zone[0], buf, count, 0, 0);
        return len;
    }
    // 块文件
    else if (ISBLK(inode->desc->mode))
    {
        assert(inode->desc->zone[0]);
        device_t* device = device_get(inode->desc->zone[0]);
        assert(file->offset % BLOCK_SIZE == 0);
        assert(count % BLOCK_SIZE == 0);
        // 读设备的扇区
        len = device_read(inode->desc->zone[0], buf, count / BLOCK_SIZE, file->offset / BLOCK_SIZE, 0);
        return len;
    }
    // 其他文件
    else
    {
        // 直接读 inode
        len = inode_read(inode, buf, count, file->offset);
    }

    if (len != EOF)
        file->offset += len;

    return len;
}

u32 sys_write(fd_t fd, char* buf, u32 count)
{
    // 获得文件结构体
    task_t* task = running_task();
    file_t* file = task->files[fd];
    assert(file);
    assert(count > 0);

    // 如果只读，错误返回
    if ((file->flags & O_ACCMODE) == O_RDONLY)
        return EOF;

    int len = 0;
    inode_t* inode = file->inode;

    // 管道文件
    if (inode->pipe)
    {
        int len = pipe_write(inode, buf, count);
        return len;
    }
    // 字符文件
    else if (ISCHR(inode->desc->mode))
    {
        assert(inode->desc->zone[0]);
        // 读设备
        device_t* device = device_get(inode->desc->zone[0]);
        len = device_write(inode->desc->zone[0], buf, count, 0, 0);
        return len;
    }
    // 块文件
    else if (ISBLK(inode->desc->mode))
    {
        assert(inode->desc->zone[0]);
        device_t* device = device_get(inode->desc->zone[0]);
        assert(file->offset % BLOCK_SIZE == 0);
        assert(count % BLOCK_SIZE == 0);
        // 读设备的扇区
        len = device_write(inode->desc->zone[0], buf, count / BLOCK_SIZE, file->offset / BLOCK_SIZE, 0);
        return len;
    }
    // 其他文件
    else
    {
        // 直接读 inode
        len = inode_write(inode, buf, count, file->offset);
    }

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

// 读取目录
int sys_readdir(fd_t fd, dirent_t* dir, u32 count)
{
    return sys_read(fd, (char*)dir, sizeof(dirent_t));
}

static int dupfd(fd_t fd, fd_t arg)
{
    task_t* task = running_task();
    if (fd >= TASK_FILE_NR || !(task->files[fd]))
        return EOF;

    // 从 task->files 数组中找到从下标 arg 开始最小的一个空文件描述符指针 
    for (; arg < TASK_FILE_NR; arg++)
    {
        if (!(task->files[arg]))
            break;
    }

    if (arg >= TASK_FILE_NR)
        return EOF;
    
    // 空的文件描述符指针指向旧的文件描述符
    task->files[arg] = task->files[fd];
    // 文件描述符引用 ++
    task->files[arg]->count++;
    return arg;
}

fd_t sys_dup(fd_t oldfd)
{
    // 第二参数写 0，表示将 oldfd 对应的文件，映射到下标最小的空闲文件描述符表位置
    return dupfd(oldfd, 0);
}

fd_t sys_dup2(fd_t oldfd, fd_t newfd)
{
    // 先把 newfd 关闭，保证 newfd 文件描述符为 NULL
    close(newfd);
    // 执行 dupfd 时，最后一定选到 newfd，即将 oldfd 与 newfd 指向同一个文件
    return dupfd(oldfd, newfd);
}

void file_init()
{
    for (size_t i = 3; i < FILE_NR; ++i)
    {
        file_t* file = file_table + i;
        file->count = 0;
        file->mode = 0;
        file->flags = 0;
        file->offset = 0;
        file->inode = NULL;
    }
}