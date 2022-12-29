#include <onix/task.h>
#include <onix/onix.h>
#include <onix/types.h>
#include <onix/printk.h>
#include <onix/debug.h>
#include <onix/memory.h>
#include <onix/interrupt.h>
#include <onix/assert.h>
#include <ds/bitmap.h>
#include <string.h>

extern bitmap_t kernel_map;
extern void task_switch(task_t* next);

#define NR_TASKS 64
static task_t* task_table[NR_TASKS];

// 获取任务数组第一个空闲位置，并且构建一个返回
static task_t* get_free_task()
{
    for (size_t i = 0; i < NR_TASKS; ++i)
    {
        if (task_table[i] == NULL)
        {
            task_table[i] = (task_t*)alloc_kpage(1);
            return task_table[i];
        }
    }
    panic("No more tasks");
}

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
    
    return task;
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
    // 获取当前、与下一关任务
    task_t* current = running_task();
    task_t* next = task_search(TASK_REDAY);

    assert(next != NULL);
    assert(next->magic == ONIX_MAGIC);

    // 切换当前任务状态
    if (current->state == TASK_RUNNING)
        current->state = TASK_REDAY;
    
    // 切换下一关任务状态
    next->state = TASK_RUNNING;
    if (next == current)
        return;
    
    task_switch(next);
}

// 创建任务
static task_t* task_create(target_t target, const char* name, u32 priority, u32 uid)
{
    task_t* task = get_free_task();
    memset(task, 0, PAGE_SIZE);

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
    task->vmap = &kernel_map;
    task->pde = KERNEL_PAGE_DIR;
    task->magic = ONIX_MAGIC;

    return task;
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

u32 thread_a()
{
    set_interrupt_state(true);
    while (true)
        printk("A");
}

u32 thread_b()
{
    set_interrupt_state(true);
    while (true)
        printk("B");
}

u32 thread_c()
{
    set_interrupt_state(true);
    while (true)
        printk("C");
}

// 任务初始化
void task_init()
{
    task_setup();

    task_create(thread_a, "thread a", 5, KERNEL_USER);
    task_create(thread_b, "thread b", 5, KERNEL_USER);
    task_create(thread_c, "thread c", 5, KERNEL_USER);
}