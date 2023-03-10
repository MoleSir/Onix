#include <onix/task.h>
#include <onix/onix.h>
#include <onix/types.h>
#include <onix/printk.h>
#include <onix/debug.h>
#include <onix/memory.h>
#include <onix/interrupt.h>
#include <onix/assert.h>
#include <onix/syscall.h>
#include <onix/global.h>
#include <onix/arena.h>
#include <ds/bitmap.h>
#include <string.h>
#include <ds/list.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define NR_TASKS 64

// 全局时间数量
extern u32 volatile jiffies;
extern u32 jiffy;
extern bitmap_t kernel_map;
extern tss_t tss;
extern void task_switch(task_t* next);
extern file_t file_table[];

static task_t* task_table[NR_TASKS];
static list_t block_list;
static list_t sleep_list;
static task_t* idle_task;

// 获取任务数组第一个空闲位置，并且构建一个返回
static task_t* get_free_task()
{
    for (size_t i = 0; i < NR_TASKS; ++i)
    {
        if (task_table[i] == NULL)
        {
            task_t* task = (task_t*)alloc_kpage(1);
            memset(task, 0, PAGE_SIZE);
            task->pid = i;
            task_table[i] = task;
            return task;
        }
    }
    panic("No more tasks");
}

// 获取进程 id
pid_t sys_getpid()
{
    task_t* task = running_task();
    return task->pid;
}

// 获取父进程 id
pid_t sys_getppid()
{
    task_t* task = running_task();
    return task->ppid;
}

// 获得当前进程的第一个空闲
fd_t task_get_fd(task_t* task)
{
    fd_t i;
    for (i = 3; i < TASK_FILE_NR; i++)
    {
        if (!(task->files[i]))
            break;
    }
    if (1 == TASK_FILE_NR)
        panic("Exceed task max open files.");
    return i;
}

void task_put_fd(task_t* task, fd_t fd)
{
    assert(fd < TASK_FILE_NR);
    task->files[fd] = NULL;
}

extern pid_t sys_getppid();

// 从任务数组中查找到某种状态的任务，自己除外
static task_t* task_search(task_state_t state)
{
    // 原子操作，保证中断被关闭
    assert(!get_interrupt_state());
    task_t* task = NULL;
    task_t* current = running_task();

    for (size_t i = 0; i < NR_TASKS; ++i)
    {
        task_t* ptr = task_table[i];
        if (ptr == NULL)
            continue;

        if (ptr->state != state)
            continue;
        if (current == ptr)
            continue;
        
        // 找到 ticks 更大或 jiffies 更大的
        if (task == NULL || task->ticks < ptr->ticks || ptr->jiffies < task->jiffies)
            task = ptr;
    }

    // 所有任务都阻塞了！！！运行空闲进行
    if (task == NULL && state == TASK_REDAY)
        task = idle_task;
    
    return task;
}

// 激活任务
void task_active(task_t* task)
{
    assert(task->magic == ONIX_MAGIC);

    if (task->pde != get_cr3())
        set_cr3(task->pde);

    if (task->uid != KERNEL_USER)
        tss.esp0 = (u32)task + PAGE_SIZE;
}

// 获得当前任务
task_t* running_task()
{
    // 当前运行的页面起始处保存 task_t 结构体
    asm volatile(
        "movl %esp, %eax\n"
        "andl $0xfffff000, %eax\n"
    );
}

// 调度
void schedule()
{
    // 不可中断
    assert(!get_interrupt_state());
    
    // 获取当前、与下一关任务
    task_t* current = running_task();
    task_t* next = task_search(TASK_REDAY);

    assert(next != NULL);
    assert(next->magic == ONIX_MAGIC);

    // 切换当前任务状态
    if (current->state == TASK_RUNNING)
        current->state = TASK_REDAY;
    
    if (!current->ticks)
        current->ticks = current->priority; 

    // 切换下一关任务状态
    next->state = TASK_RUNNING;
    if (next == current)
        return;
    
    //printk("switch to 0x%p\n", next);
    // tss.esp0 与 tss.ss0 在 int 时被自动切换?
    task_active(next);
    task_switch(next);
}

void task_sleep(u32 ms)
{
    assert(!get_interrupt_state());

    // 需要睡眠的时间片：总毫秒数除以一个时间片的毫秒值
    u32 ticks = ms / jiffy;
    ticks = ticks > 0 ? ticks : 1;

    // 记录目标全局时间片，在那个时刻需要唤醒任务
    task_t* current = running_task();
    // 全局时间片到达这个值后，任务被唤醒
    current->ticks = jiffies + ticks;

    // 从睡眠链表找到第一个比当前任务唤醒时间更晚的任务，进行插入
    list_t* list = &sleep_list;
    list_node_t* anchor = &list->tail;

    // 越早被唤醒的任务排在越前面 
    for (list_node_t* ptr = list->head.next; ptr != &(list->tail); ptr = ptr->next)
    {
        task_t * task = element_entry(task_t, node, ptr);

        if (task->ticks > current->ticks)
        {
            anchor = ptr;
            break;
        }
    }

    assert(current->node.next == NULL);
    assert(current->node.prve == NULL);
    
    // 插入
    list_insert_before(anchor, &(current->node));

    // 更新任务状态
    current->state = TASK_SLEEPING;

    // 调度
    schedule();
}

// 唤醒睡眠链表中的应该唤醒的任务
void task_wakeup()
{
    assert(!get_interrupt_state());

    // 从睡眠链表中找到第一个 tick 大于当前时间片的，这个以及之后都不能被唤醒
    list_t* list = &(sleep_list);
    for (list_node_t* ptr = list->head.next; ptr != &(list->tail);)
    {
        task_t* task = element_entry(task_t, node, ptr);
        if (task->ticks > jiffies)
            break;
        
        ptr = ptr->next;

        task->ticks = 0;
        task_unblock(task);
    }
}

// 创建任务
static task_t* task_create(target_t target, const char* name, u32 priority, u32 uid)
{
    task_t* task = get_free_task();

    // 页尾做栈顶（高地址）
    u32 stack = (u32)task + PAGE_SIZE;

    // 空出部分内存做位 task_frame_t
    stack -= sizeof(task_frame_t);
    task_frame_t* frame = (task_frame_t*)stack; 
    frame->ebx = 0x11111111;
    frame->esi = 0x22222222;
    frame->edi = 0x33333333;
    frame->ebp = 0x44444444;
    frame->eip = (void*)target;

    strcpy((char*)(task->name), name);

    task->stack = (u32*)stack;
    task->priority = priority;
    task->ticks = task->priority;
    task->jiffies = 0;
    task->state = TASK_REDAY;
    task->uid = uid;
    task->gid = 0;
    task->vmap = &kernel_map;
    task->pde = KERNEL_PAGE_DIR;
    task->brk = USER_EXEC_ADDR;
    task->text = USER_EXEC_ADDR;
    task->data = USER_EXEC_ADDR;
    task->end = USER_EXEC_ADDR;
    task->iexec = NULL;
    task->magic = ONIX_MAGIC;
    task->iroot = task->ipwd = get_root_inode();
    task->iroot->count += 2;

    task->pwd = (void*)alloc_kpage(1);
    strcpy(task->pwd, "/");
    task->umask = 0022; // 对应 0755

    // 获得全局文件中的标准输入输出设备
    task->files[STDIN_FILENO] = &file_table[STDIN_FILENO];
    task->files[STDOUT_FILENO] = &file_table[STDOUT_FILENO];
    task->files[STDERR_FILENO] = &file_table[STDERR_FILENO];
    task->files[STDIN_FILENO]->count++;
    task->files[STDOUT_FILENO]->count++;
    task->files[STDERR_FILENO]->count++;

    return task;
}

extern int sys_execve(char* filename, char* argvp[], char* envp[]);

// 调用该函数的地方不能用任何局部变量
// 调用前栈顶需要准备足够的空间
void task_to_user_mode()
{
    task_t* task = running_task(); 

    task->vmap = kmalloc(sizeof(bitmap_t));
    // 用一页空间来保存位图
    void* buf = (void*)alloc_kpage(1);
    bitmap_init(task->vmap, buf, USER_MMAP_SIZE / PAGE_SIZE / 8, USER_MMAP_ADDR / PAGE_SIZE);

    // 创建用户进程页表
    task->pde = (u32)copy_pde();
    set_cr3(task->pde);

    u32 addr = (u32)task + PAGE_SIZE;

    addr -= sizeof(intr_frame_t);
    intr_frame_t* iframe = (intr_frame_t*)(addr);

    iframe->vector = 0x21;
    iframe->edi = 1;
    iframe->esi = 2;
    iframe->ebp = 3;
    iframe->esp_dummy = 4;
    iframe->ebx = 5;
    iframe->edx = 6;
    iframe->ecx = 7;
    iframe->eax = 8;

    iframe->gs = 0;
    iframe->ds = USER_DATA_SELECTOR;
    iframe->es = USER_DATA_SELECTOR;
    iframe->fs = USER_DATA_SELECTOR;
    iframe->ss = USER_DATA_SELECTOR;
    iframe->cs = USER_CODE_SELECTOR;

    iframe->error = ONIX_MAGIC;

    iframe->eip = 0;
    iframe->eflags = (0 << 12 | 0b10 | 1 << 9); 
    iframe->esp = USER_STACK_TOP;

    int err = sys_execve("/bin/init.out", NULL, NULL);
    panic("exec /bin/init.out failure!");
}

// 任务启动
static void task_setup()
{
    // 获取当前任务
    task_t* task = running_task();
    task->magic = ONIX_MAGIC;
    task->ticks = 1;

    // 设置起始任务数组为空
    memset(task_table, 0, sizeof(task_table));
}

void task_yield()
{
    schedule();
}

void task_block(task_t* task, list_t* blist, task_state_t state)
{
    assert(!get_interrupt_state());
    assert(task->node.next == NULL);
    assert(task->node.prve == NULL);

    if (blist == NULL)
        blist = &block_list;
    
    list_push(blist, &(task->node));

    assert(state != TASK_REDAY && state != TASK_RUNNING);

    task->state = state;

    task_t* current = running_task();
    if (current == task)
        schedule();
}

void task_unblock(task_t* task)
{
    assert(!get_interrupt_state());

    list_remove(&(task->node));

    assert(task->node.next == NULL);
    assert(task->node.prve == NULL);

    task->state = TASK_REDAY;
}

extern void interrupt_exit();

// 构建栈
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

// fork!!
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

    // 拷贝 pwd
    child->pwd = (char*)alloc_kpage(1);
    strncpy(child->pwd, task->pwd, PAGE_SIZE);

    task->ipwd->count++;
    task->iroot->count++;
    if (task->iexec)
        task->iexec->count++;

    // 文件引用加 1
    for (size_t i = 0; i < TASK_FILE_NR; ++i)
    {
        file_t* file = child->files[i];
        if (file)
            file->count++;
    }

    // 构造 child 内核栈
    task_build_stack(child);

    // 父进程返回子进程 pid
    return child->pid;
}

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

    // 释放文件
    free_kpage((u32)task->pwd, 1);
    iput(task->ipwd);
    iput(task->iroot);
    iput(task->iexec);

    // 关闭打开的文件
    for (size_t i = 0; i < TASK_FILE_NR; i++)
    {
        file_t *file = task->files[i];
        if (file)
        {
            close(i);
        }
    }

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

extern void idle_thread();
extern void init_thread();
extern void test_thread();

// 任务初始化
void task_init()
{
    list_init(&block_list);
    list_init(&sleep_list);

    task_setup();

    idle_task = task_create(idle_thread, "idle", 1, KERNEL_USER);
    task_create(init_thread, "init", 5, NORMAL_USER);
    task_create(test_thread, "test", 5, KERNEL_USER);
}