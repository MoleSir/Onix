#include <onix/types.h>
#include <onix/fs.h>
#include <onix/task.h>
#include <onix/stat.h>
#include <stdio.h>
#include <onix/device.h>
#include <string.h>
#include <onix/syscall.h>
#include <ds/fifo.h>
#include <onix/assert.h>
#include <onix/debug.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

int pipe_read(inode_t* inode, char* buf, int count)
{
    fifo_t* fifo = (fifo_t*)(inode->desc);
    int nr = 0;
    
    // 读取，直到读了 count 个
    while (nr < count)
    {
        // 如果管道为空，读不了
        if (fifo_empty(fifo))
        {
            assert(inode->rxwaiter == NULL);
            // 设置此管道的等待进程为当前进程，进入阻塞
            inode->rxwaiter = running_task();
            task_block(inode->rxwaiter, NULL, TASK_BLOCKED);
        }

        // 从队列中取一个字符
        buf[nr++] = fifo_get(fifo);
        // 如果管道存在发送等待进程（说明管道满了，别人写不下，只能等着），接触阻塞
        if (inode->txwaiter)
        {
            task_unblock(inode->txwaiter);
            inode->txwaiter = NULL;
        } 
    }
    return nr;
}

int pipe_write(inode_t* inode, char* buf, int count)
{
    fifo_t* fifo = (fifo_t*)(inode->desc);
    int nw = 0;

    while (nw < count)
    {
        // 如果管道满，写不了，阻塞
        if (fifo_full(fifo))
        {
            assert(inode->txwaiter == NULL);
            inode->txwaiter = running_task();
            task_block(inode->txwaiter, NULL, TASK_BLOCKED);
        }
        
        // 放入一个字符
        fifo_put(fifo, buf[nw++]);
        // 如果管道的写进程存在，说明原来管道空了，有进程等待读，唤醒
        if (inode->rxwaiter)
        {
            task_unblock(inode->rxwaiter);
            inode->rxwaiter = NULL;
        }
    }

    return nw;
}

// 系统调用
int sys_pipe(fd_t pipefd[2])
{
    inode_t* inode = get_pipe_inode();
    
    task_t* task = running_task();
    file_t* files[2];

    // 从进程中拿到一个空文件描述符
    pipefd[0] = task_get_fd(task);
    // 从全局文件数组中拿一个空闲的，将指针赋值给这个进程
    files[0] = task->files[pipefd[0]] = get_file();

    // 同上
    pipefd[1] = task_get_fd(task);
    files[1] = task->files[pipefd[1]] = get_file();

    // 0 文件的 inode 指向管道，并且为只读
    files[0]->inode = inode;
    files[0]->flags = O_RDONLY;

    // 1 文件的 inode 指向管道，并且为只写
    files[1]->inode = inode;
    files[1]->flags = O_WRONLY;

    return 0;
}