#ifndef __ONIX_TASK_HH__
#define __ONIX_TASK_HH__

#include <onix/types.h>
#include <ds/bitmap.h>

#define KERNEL_USER 0
#define NORMAL_USER 1

#define TASK_NAME_LEN 16

typedef u32 (*target_t)();

typedef enum task_state_t
{
    TASK_INIT,      // 初始化
    TASK_RUNNING,   // 运行
    TASK_REDAY,     // 就绪
    TASK_BLOCKED,   // 阻塞
    TASK_SLEEPING,  // 休眠
    TASK_WAITING,   // 等待
    TASK_DIED,      // 死亡
} task_state_t;

typedef struct task_t
{
    u32* stack;             // 内核栈
    task_state_t state;     // 任务状态
    u32 priority;           // 任务优先级，每次初始化 tick 的值
    u32 ticks;              // 剩余时间片，每次时钟后减去 1，到 0 调度
    u32 jiffies;            // 上次执行时全局时间片
    char name[TASK_NAME_LEN];// 任务名称
    u32 uid;                // 用户 id
    u32 pde;                // 页目录物理地址
    struct bitmap_t* vmap;  // 进出虚拟内存位图
    u32 magic;              // 内核魔数，校验溢出
} task_t;

typedef struct task_frame_t
{   
    u32 edi;
    u32 esi;
    u32 ebx;
    u32 ebp;
    void (*eip)(void);
} task_frame_t;

task_t* running_task();
void schedule();

#endif