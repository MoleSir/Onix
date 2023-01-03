# 系统调用 waitpid

父进程来回收子进程的内核栈物理页面：

```c
pid_t waitpid(pid_t pid, int32* status);
```

注册系统调用的过程省略，处理函数为：

```c
pid_t task_waitpid(pid_t pid, int32* status)
{
    task_t* task = running_task();
    task_t* child = NULL;

    while (true)
    {
        bool has_child = false;
        for (size_t i = 2; i < NR_TASKS; ++i)
        {
            task_t* ptr = task_table[i];
            // 空任务
            if (!ptr)
                continue;

            // 不是子进程
            if (ptr->ppid != task->pid)
                continue;

            // 不是等待的子进程
            if (pid != ptr->pid && pid != -1)
                continue;
            
            // 满足条件，找到了 pid 的子进程
            // 当前，子进程已经死亡，可以直接回收，跳转即可
            if (ptr->state == TASK_DIED)
            {
                child = ptr;
                task_table[i] = NULL;

                *status = child->status;
                u32 ret = child->pid;

                free_kpage((u32)child, 1);
                return ret;
            }

            // 有目标子进程，但还没有死亡，需要等待这个子进程到死亡为止
            has_child = true;
        }

        if (has_child)
        {
            // 父进程等待子进程的死亡
            task->waitpid = pid;
            // 阻塞，等待子进程死亡时将其唤醒
            task_block(task, NULL, TASK_WAITING);
            continue;
        }

        // 没有要等待的子进程啊
        break;
    }

    // 没有合适的子进程
    return -1;
}
```

主要利用了 `task_block` 函数来阻塞，还需要注意一点就是有两种情况：

- 父进程先调用 `waitpid`，子进程再 `exit`，那么父进程需要一直等待，直到子进程退出为止，再回收资源；
- 子进程先调用 `exit`，父进程再 `waitpid`，子进程的资源会存在一段时间，父进程调用后就被回收；

> 参数 `pid = -1` 时，是等待所有子进程的退出；

还需要配合子进程唤醒：

````c
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

    task_t* parent = task_table[task->ppid];
    // 如果：父进程在等待状态，并且（父进程在等待所有的子进程或父进程在等待这个子进程死亡）
    if (parent->state == TASK_WAITING && 
        (parent->waitpid == -1 || parent->waitpid == task->pid))
    {
        // 唤醒父进程
        task_unblock(parent);
    }    

    schedule();
}
````