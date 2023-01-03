# 系统调用 fork

实现系统调用 `fork`：

```c
pid_t fork(void);
```

功能是拷贝一个相同的进程，并且函数只在父进程调用，却可分别在子进程返回：

- 子进程返回 0；
- 父进程返回值为子进程 id。


## 注册 `fork`

在 syscall.h 中增加 `fork` 的声明，在 syscall.c 中调用 `_syscall0` 传递系统调研号，真正的处理函数 `task_fork` 在 task.c 中实现，在 gate.c 中注册。


## `task_fork` 实现

````c
pid_t task_fork()
{
    task_t* task = running_task();

    // 当前进程非阻塞，并且正在执行
    assert(task->node.next == NULL && task->node.prve == NULL && task->state == TASK_RUNNING);

    // 拷贝内核 和 PCB
    task_t* child = get_free_task();
    pid_t pid = child->pid;
    // 父、子进程整页复制
    memcpy(child, task, PAGE_SIZE);

    // 改变子进程的若干字段
    child->pid = pid;
    child->ppid = task->pid;
    child->ticks = child->priority;
    child->state = TASK_REDAY;

    // 拷贝用户进程虚拟内存位图
    child->vmap = kmalloc(sizeof(bitmap_t));
    memcpy(child->vmap, task->vmap, sizeof(bitmap_t));

    // 拷贝虚拟内存位图缓存
    void* buf = (void*)alloc_kpage(1);
    memcpy(buf, task->vmap->bits, PAGE_SIZE);
    child->vmap->bits = buf;

    // 拷贝页目录
    child->pde = (u32)copy_pde();

    // 构造 child 内核栈
    task_build_stack(child);

    // 父进程返回子进程 pid
    return child->pid;
}
````

主要实现逻辑：

1. 在 task_table 中申请一个空任务 task，设置 task 的父 id 为当前任务 id；
2. 赋值当前任务的到子进程，整页赋值；
3. 修改子进程页面中的一些值；
4. 拷贝用户进程虚拟内存位图；
5. 拷贝虚拟内存位图缓存；
6. 拷贝页目录；
7. 构造 child 内核栈，使得 child 进程可用在任务调度之后顺利执行；
8. 父进程返回子进程 id

此函数由父进程的内核栈执行，看父进程看来，只不过调用了一次返回而已，这出现了第一次返回；

## `task_build_stack` 实现

`task_fork` 中的重要函数，保证了子进程可在任务调度后顺序执行：

````c
static void task_build_stack(task_t* task)
{
    // 构造中断返回现场
    u32 addr = (u32)task + PAGE_SIZE;
    addr -= sizeof(intr_frame_t);
    intr_frame_t* iframe = (intr_frame_t*)addr;
    // 特别的是 eax 为保存返回值 0
    iframe->eax = 0;

    // 构造返回现场
    addr -= sizeof(task_frame_t);
    task_frame_t* frame = (task_frame_t*)addr;

    frame->ebp = 0xaa55aa55;
    frame->ebx = 0xaa55aa55;
    frame->edi = 0xaa55aa55;
    frame->esi = 0xaa55aa55;

    frame->eip = interrupt_exit;

    task->stack = (u32*)frame;
}
````

原理跟最开始轮流打印 A、B 的类型，但需要注意，现在有了用户态，任何一个用户任务要发生切换，都必须先中断到内核，即发生系统调用；

由于 PCB 位于一页内核内存的起始位置，`stack` 又是 `task_frame_t` 的第一个字段，这里把 `frame` 的地址保存在这页内存的开始位置；

由于 `task` 这个结构体已经由 `get_free_task` 函数创建，之后发生调度时，其总有一次会被选中，执行 `swtch_task(task)`；

在那个函数中，当前进程会先保存寄存器，之后取出参数 `task` 指向的地址的数据，那就是 `frame` 的值
通过这个值（一个地址）又可用找到其中的各个字段，分别赋值给 esp、eip 等（`task_frame_t` 结构体的结构就是模仿函数调用返回），之后返回到一个设置好的地址；

> 而当这个进程不是第一次被调度，就可用之前从调度之前的一系列压栈操作获得信息了；

由于现在进程的调度存在内核与用户的间隔，任务必须在内核栈调度，那么除了模拟一次调度的发生，还需要模拟中断返回，所以返回地址被设置为 `interrupt_exit`，在这里模拟一次中断的返回，这就是为什么在 `task_frame_t` 结构体之前还有一个 `task_frame_t` 结构体；

因为此时父子进程的内核栈完全相同，所以子进程回到 `interrupt_exit` 时候的栈情况跟父进程执行完系统调用，回到
`interrupt_exit` 完全一致，除了手动设置的返回值 `eax` 成为 0 之外，所以子进程也会回到父进程执行完 fork 后的下一行代码，并且带有返回值 0，这就是第二次返回！


## `copy_pde` 实现

在 `task_fork` 中调用了 `copy_pde` 使得拷贝父进程的页目录：

````c
page_entry_t* copy_pde()
{
    // task 为父进程
    task_t* task = running_task();
    // pde 是逻辑地址，但前 8M 的内存就 vaddr = paddr，所以 pde 可直接返回给子进程使用
    page_entry_t* pde = (page_entry_t*)alloc_kpage(1);
    // 拷贝一份页目录
    memcpy(pde, (void*)task->pde, PAGE_SIZE);

    // 最后一项指向自己
    page_entry_t* entry = pde + 1023;
    entry_init(entry, IDX(pde));

    // 遍历页目录，拷贝已经存在的页表，遍历的是子进程的页目录，其中的内容跟父进程一致
    page_entry_t* dentry;
    // 0、1 是内核态占据的 8M 内存
    for (size_t didx = 2; didx < 1023; ++didx)
    {
        // 两个页目录，内容完全一致
        dentry = pde + didx;
        if (!dentry->present)
            continue;

        // 页目录项目存在指向的页表，pte 是页表逻辑地址（页表有两个逻辑地址！可用用 0xffc 开头连查两次页目录，或者直接前 8M paddr = vaddr）
        // 父、子进程，两个页目录都指向这个页表地址
        page_entry_t* pte = (page_entry_t*)(PDE_MASK | (didx << 12));

        // 遍历页表
        for (size_t tidx = 0; tidx < 1024; ++tidx)
        {
            entry = pte + tidx;
            if (!entry->present)
                continue;

            // 页表项存在指向的页面
            assert(memory_map[entry->index] > 0);

            // 设置为只读，写时拷贝
            entry->write = false;

            // 物理引用 + 1
            memory_map[entry->index]++;
            assert(memory_map[entry->index] < 255);
        }

        // 拷贝这个页表，把逻辑地址 pte 这个页拷贝一份到 paddr 这个物理地址
        u32 paddr = copy_page(pte);

        // 设置子进程的页目录项指向新的页表!!!
        dentry->index = IDX(paddr);
    }

    set_cr3(task->pde);

    return pde;
}
````

1. 首先申请一页内核内存作为子进程的页目录，从父进程中完全拷贝过来；
2. 遍历子进程页目录的每一个表项，判断是否存在；
3. 如果存在，那么此时父、子进程页目录的某个表项都指向了这个页表。再遍历这个页表的每一项，判断是否存在；
4. 如果存在，把这个物理页面设置为不可写（设置页表表项的 write 位），那么现在父子进程这张共享的物理页面都不可写，并且引用次数增加；
5. 遍历完页表后，还需要复制一份这个页表给子进程；

执行完这个函数后，父子进程拥有独立的页目录、页表。但指向的物理页没有改变，所以相同的逻辑地址被指向相同的物理地址。并且这些页面全部都是不可写；

当二者有谁返回，尝试写页面，将触发 `page_falut`，需要在那里处理这种情况。这样的机制成为写时拷贝。

这个函数还有一个比较令人费解的函数 `copy_page`：

````c
static u32 copy_page(void* page)
{
    // 申请一页，返回的是物理地址的页索引
    u32 paddr = get_page();

    // 现在不能直接访问 paddr 的物理地址，必须先给一个映射
    // 为了把 page 上的内容拷贝到 paddr 上，先把 paddr 映射到逻辑地址 0 处
    page_entry_t* entry = get_pte(0, false);
    entry_init(entry, IDX(paddr));
    
    // 再使用逻辑地址进行拷贝
    memcpy((void*)0, (void*)page, PAGE_SIZE);

    // 这个页面临时使用，其中的 Index 没有意义，所以这个存在位为 0
    entry->present = false;
    return paddr;
}
````

申请一页物理页，将参数 `page` （逻辑地址）指向的页面拷贝到物理页中，并且返回这个物理页的地址（物理地址）；

其中使用的 0 地址只不过是暂用，因为开启分页后，无法直接使用 paddr 来进行拷贝，需要先把物理内存映射到 0 处，再对 0 的地址拷贝，才可把内存写到 paddr 中，最后返回；

> 内核页面中的第一页没有被映射，就是出于这个考虑

可用看到，在 `copy_pde` 函数中的这两句：

````c
// 拷贝这个页表，把逻辑地址 pte 这个页拷贝一份到 paddr 这个物理地址
u32 paddr = copy_page(pte);

// 设置子进程的页目录项指向新的页表!!!
dentry->index = IDX(paddr);
````

拷贝页表后，还需要对子进程的页目录重新连接指向这个新的物理页，而不是原来父进程的；

> `get_page` 函数通过 mmap 判断内存空余，用来申请一页内存，因为物理内存的前 8M 全部为操作系统使用，已经申请过，所以此函数只会从 8M 之后的内存开始申请。这些都是用户的内存，内核内存需要使用 `alloc_kpage` 通过系统内存位图来申请。所以可用看到用户页面、页表、页目录都是用 `get_page`，而跟操作系统相关的内核栈的构建都使用 `alloc_kpage` 或 `kmalloc`（内部也是通过 `alloc_kpage`）；


## `page_fault` 补充

`page_falut` 要解决写时拷贝的问题，这种异常发生，CPU 压入的错误码会提醒，这个错误不是由于缺页导致，而是因为不可写：

````c
void page_fault(
    int vector,
    u32 edi, u32 esi, u32 ebp, u32 esp,
    u32 ebx, u32 edx, u32 ecx, u32 eax,
    u32 gs, u32 fs, u32 es, u32 ds,
    u32 vector0, u32 error, u32 eip, u32 cs, u32 eflags)
{
    assert(vector == 0xe);
    u32 vaddr = get_cr2();
    LOGK("fault address 0x%p\n", vaddr);

    page_error_code_t* code = (page_entry_t*)(&error);
    task_t* task = running_task();
    
    assert(KERNEL_MEMORY_SIZE <= vaddr && vaddr < USER_STACK_TOP);

    // 错误码表示，这个地址存在物理页
    if (code->present)
    {
        assert(code->write);

        // 获取该虚拟地址对应的页表地址与页表表项
        page_entry_t* pte = get_pte(vaddr, false);
        page_entry_t* entry = pte + TIDX(vaddr);

        // 这个表项应该存在，页面有被使用
        assert(entry->present);
        assert(memory_map[entry->index] > 0);

        // 判断该页面的使用次数
        if (memory_map[entry->index] == 1)
        {   
            // 只被一个进程使用，那么直接把写允许打开
            entry->write = true;
            LOGK("WRITE page for 0x%p\n", vaddr);
        }
        else
        {
            // 多个进程同时拥有一个只读页面，其中一个进程想写
            // 获得虚拟地址对应的页面起始位置
            void* page = (void*)PAGE(IDX(vaddr));
            // 把这个页面拷贝一份，并且返回物理地址
            u32 paddr = copy_page(page);
            // 原来物理页的应用减 1
            memory_map[entry->index]--;
            // 重新连接新的物理页
            entry_init(entry, IDX(paddr));
            // 刷新 tlb
            flush_tlb(vaddr);
            LOGK("COPY page for 0x%p\n", vaddr);
        }
        return;
    }

    if (!code->present && (vaddr < task->brk || vaddr >= USER_STACK_BUTTOM))
    {
        // 获得页面起始地址
        u32 page = PAGE(IDX(vaddr));
        // 申请一页内存映射
        link_page(page);
        // BMB;
        return;
    }

    panic("page fault!!!\n");
}
````

逻辑不复杂，注释详细。