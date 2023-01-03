# 系统调用 exit

````c
noreturn void exit(int status);
````

利用伪装系统调用返回的函数是不可以结束的，不然任务不知道会转到哪里去。想结束就需要使用 `exit` 系统调用，执行的进程会退出，并且释放页面、页表、页目录、虚拟内存位图资源;

系统调用注册过程不说了，处理函数是 `task_exit`：

```c
void task_exit(int status)
{
    task_t* task = running_task();

    // 当前进程非阻塞，并且正在执行
    assert(task->node.next == NULL && task->node.prve == NULL && task->state == TASK_RUNNING);

    // 改变状态
    task->state = TASK_DIED;
    task->status = status;

    // 释放页目录
    free_pde();

    // 释放虚拟位图
    free_kpage((u32)task->vmap->bits, 1);
    kfree(task->vmap);

    // 将子进程的父进程复制为自己的父进程
    for (size_t i = 0; i < NR_TASKS; ++i)
    {
        task_t* child = task_table[i];
        if (!child)
            continue;
        
        if (child->ppid != task->pid)
            continue;

        child->ppid = task->ppid;
    }

    LOGK("task 0x%p exit...\n", task);
    schedule();
}
```

逻辑很简单，看注释即可。其中有释放页面的函数：

````c
void free_pde()
{
    task_t* task = running_task();
    assert(task->uid != KERNEL_USER);

    page_entry_t* pde = get_pde();

    for (size_t didx = 2; didx < 1023; didx++)
    {
        page_entry_t* dentry = pde + didx;
        if (!dentry->present)
            continue;
        
        page_entry_t* pte = (page_entry_t*)(PDE_MASK | (didx << 12));
        for (size_t tidx = 0; tidx < 1024; tidx++)
        {
            page_entry_t* entry = pte + tidx;
            if (!entry->present)
                continue;
            
            // 释放页面
            assert(memory_map[entry->index] > 0);
            put_page(PAGE(entry->index));
        }

        // 释放页表
        put_page(PAGE(dentry->index));
    }

    // 释放页目录
    free_kpage(task->pde, 1);
    LOGK("free pages %d\n", free_pages);
}
````

也比较简答，就是要注意，页面、页表是用户空间申请的物理页，而页目录是内核的物理页，所以回收函数不一样；

执行完 `exit` 后，执行调度，到别的任务，而由于当前任务的状态改变，之后就不会再被调度，而这个任务执行调度前插入栈的信息还是在其内核栈中保存；

> 需要注意的是，创建任务时，还为内核申请了一页内存，也就是放 task_t 结构体的，但是目前没有释放，这页内存需要让这个进程的父进程来帮忙处理；

> 内核内存前 8M，用户内存 8M 到 128M。每个用户进程都有一页内核栈，每个用户进程有自己独立的页表系统，其保存在内核栈中的 `task_t` 结构体，由于分页的存在，所有进程都认为自己独占 8M 到 128M 的空间，自己的内核独占前 8M，但实际上是共享的。还需要注意，最开始系统中的页表让逻辑、物理地址的前 8M 一一映射，而之后的页目录都是拷贝而来，所以对每一个用户进程，其实其逻辑地址的前 8M 都对应物理地址的前 8M，所以其实内核的地址是没有隔离的，两个用户进程的内核不可以访问相同的地址，但用户空间的完全可以相同（`fork` 后相同的逻辑地址对应不同的物理地址）
