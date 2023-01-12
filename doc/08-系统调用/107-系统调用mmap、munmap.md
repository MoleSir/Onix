# 系统调用 mmap,munmap

目前的内存布局：

![](./pics/memory_map.drawio.svg)

实现系统调用，以调整文件映射内存。

```c++
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void *addr, size_t length);
```

Linux 系统 `mmap` 支持的参数比较多，可以参考相关文档，这里只实现 **共享内存** 和 私有内存。


## `task_t` 的调整

原来 task_t 中的 `vmap` 表示用户态的页表映射，现在改为用户映射区的使用情况：

````c
void task_to_user_mode(target_t target)
{
    task_t* task = running_task(); 

    task->vmap = kmalloc(sizeof(bitmap_t));
    // 用一页空间来保存位图
    void* buf = (void*)alloc_kpage(1);
    bitmap_init(task->vmap, buf, USER_MMAP_SIZE / PAGE_SIZE / 8, USER_MMAP_ADDR / PAGE_SIZE);

    // 创建用户进程页表
    ...
}
````

同时，由于修改了 vamp 的意义，在 memory.c 中使用到这个字段的函数都需要微调，但改动不大，比如 `link_page` 与 `unlink_page`；


## 修改页目录项

原来的 `page_entry_t` 有三位没有作用，可以留给操作系统发挥，这里取出两位：

```c
typedef struct page_entry_t
{
    u8 present : 1;  // 在内存中
    u8 write : 1;    // 0 只读 1 可读可写
    u8 user : 1;     // 1 所有人 0 超级用户 DPL < 3
    u8 pwt : 1;      // page write through 1 直写模式，0 回写模式
    u8 pcd : 1;      // page cache disable 禁止该页缓冲
    u8 accessed : 1; // 被访问过，用于统计使用频率
    u8 dirty : 1;    // 脏页，表示该页缓冲被写过
    u8 pat : 1;      // page attribute table 页大小 4K/4M
    u8 global : 1;   // 全局，所有进程都用到了，该页不刷新缓冲
    u8 shared : 1;   // 共享内存页，与 CPU 无关
    u8 privat : 1;   // 私有内存页，与 CPU 无关
    u8 flag : 1;     // 送给操作系统，不需要
    u32 index : 20;  // 页索引
} _packed page_entry_t;
```

shared 表示内存是否是共享的，这很重要，`fork` 会用到；


## 内存映射类型

在 syscall.h 增加：

````c
enum mmap_type_t
{
    PROT_NONE = 0,
    PROT_READ = 1,
    PROT_WRITE = 2,
    PROT_EXEC = 4,

    MAP_SHARED = 1,
    MAP_PRIVATE = 2,
    MAP_FIXED = 0x10,
};
````

来描述内存映射的一些熟悉；


## `mmap` 实现

给内存映射区的一段从 addr 开始，长度为 length 字节的虚拟内存映射到物理内存，如果 addr 传入 0，系统自动在内存映射区找出一段内存；

如果还指定了参数 fd 与 offset，还会将 fd 指向的文件拷贝到获取到的内存上；

最后结果返回内存的起始位置，如果 addr 有值就返回本身，如果是 0 就返回系统找到的那个内存起始位置；

````c
void* sys_mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    ASSERT_PAGE((u32)addr);

    // 计算需要的页面数量
    u32 count = div_round_up(length, PAGE_SIZE);
    u32 vaddr = (u32)addr;

    task_t* task = running_task();
    // 如果调用者没有指定要要映射的地址，默认为第一个空闲页
    if (!vaddr)
        vaddr = scan_page(task->vmap, count);

    assert(vaddr >= USER_MMAP_ADDR && vaddr < USER_STACK_BUTTOM);

    // 对每一个页面：
    for (size_t i = 0; i < count; ++i)
    {
        u32 page = vaddr + PAGE_SIZE * i;
        // 给虚拟地址 page 映射一个物理页
        link_page(page);
        // 设置映射位图
        bitmap_set(task->vmap, IDX(page), true);

        // 得到描述这个物理页的页表项
        page_entry_t* entry = get_entry(page, false);
        entry->user = true;
        entry->write = false;
        if (prot & PROT_WRITE)
        {
            entry->write = true;
        }
        if (flags & MAP_SHARED)
        {
            entry->shared = true;
        }
        if (flags & MAP_PRIVATE)
        {
            entry->privat = true;
        }
    }

    // 如果指定了一个文件，将文件读入获得到的内存
    if (fd != EOF)
    {
        lseek(fd, offset, SEEK_SET);
        read(fd, (char*)vaddr, length);
    }

    return (void*)vaddr;
}
````

1. 计算需要的页面数量 `count`；
2. 如果调用者没有指定要要映射的地址，默认为第一个空闲页；
3. 循环对这 `count` 个页面：
    - 计算页面起始虚拟地址；
    - 给这个虚拟地址一个内存映射（获得物理页）；
    - 设置映射位图；
    - 根据参数传入的属性设置内存描述符；
4. 如果指定了文件，将文件读入映射好的内存中；


## `munmap` 实现

跟 `mmap` 类型：

````c
int sys_munmap(void* addr, size_t length)
{
    task_t* task = running_task();
    u32 vaddr = (u32)addr;
    assert(vaddr >= USER_MMAP_ADDR && vaddr < USER_STACK_BUTTOM);

    ASSERT_PAGE(vaddr);
    u32 count = div_round_up(length, PAGE_SIZE);
    
    // 对每一个页面：
    for (size_t i = 0; i < count; ++i)
    {
        u32 page = vaddr + i * PAGE_SIZE;
        unlink_page(page);
        assert(bitmap_test(task->vmap, IDX(page)));
        bitmap_set(task->vmap, IDX(page), false);
    }

    return 0;
}
````


## 修改 `fork` 

`fork` 中调用了 `copy_pde` 赋值父进程的页目录与页表，并且将所有页目录设置为不可写，这样虽然一开始父子进程的虚拟地址将映射到完全相同的物理地址，但之后只要有进程写内存就会进入 `page_fault`，重新复制一份，称为写时复制；

现在修改，设置不可以之前，判断是否为共享内存：

```c
            // 若不是共享内存，设置为只读
            if (!entry->shared)
                entry->write = false;
```

如果是共享内存，还是可写的，这意味着：父子进程在内存映射区中的所有虚拟地址都被映射到同一个物理地址上（已经被映射的那些内存映射区，`fork` 之后再映射的就不相同了）；

这可以实现两个进程访问同一块内存，实现进程间的通信！


## 实例

````c
void builtin_test(int argc, char *argv[])
{
    u32 status;

    int* counter = (int*)mmap(0, sizeof(int), PROT_WRITE, MAP_SHARED, EOF, 0);
    pid_t pid = fork();

    if (pid)
    {
        while (true)
        {
            (*counter)++;
            sleep(300);
        }
    }
    else
    {
        while (true)
        {
            printf("counter %d\n", *counter);
            sleep(100);
        }
    }
}
````

父进程先映射了虚拟内存，并且指定这块内存是共享的、可写的，之后调用 `fork`：

- 父进程不断修改共享内存；
- 子进程不断打印；

会看到子进程打印的结果在改变，说明两个进程的 counter 虚拟地址对应了同一物理地址；