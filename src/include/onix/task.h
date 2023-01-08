#ifndef __ONIX_TASK_HH__
#define __ONIX_TASK_HH__

#include <onix/types.h>
#include <ds/bitmap.h>
#include <ds/list.h>
#include <onix/fs.h>

#define KERNEL_USER 0
#define NORMAL_USER 1000

#define TASK_NAME_LEN 16
#define TASK_FILE_NR 16

typedef void (*target_t)();

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
    u32* stack;                         // 内核栈
    list_node_t node;                   // 任务阻塞节点
    task_state_t state;                 // 任务状态
    u32 priority;                       // 任务优先级，每次初始化 tick 的值
    u32 ticks;                          // 剩余时间片，每次时钟后减去 1，到 0 调度
    u32 jiffies;                        // 上次执行时全局时间片
    char name[TASK_NAME_LEN];           // 任务名称
    u32 uid;                            // 用户 id
    u32 gid;                            // 用户组 id
    pid_t pid;                          // 任务 id
    pid_t ppid;                         // 父进程 id
    u32 pde;                            // 页目录物理地址
    struct bitmap_t* vmap;              // 进出虚拟内存位图
    u32 brk;                            // 进程堆内存最高地址
    int status;                         // 进程特殊状态
    pid_t waitpid;                      // 进程等待的 pids
    struct inode_t* ipwd;               // 进程当前的目录 inode
    struct inode_t* iroot;              // 进程根目录 inode
    u16 umask;                          // 进程用户权限
    struct file_t* files[TASK_FILE_NR]; // 进程文件表
    u32 magic;                          // 内核魔数，校验溢出
} task_t;

typedef struct task_frame_t
{   
    u32 edi;
    u32 esi;
    u32 ebx;
    u32 ebp;
    void (*eip)(void);
} task_frame_t;

typedef struct intr_frame_t
{
    u32 vector;

    u32 edi;
    u32 esi;
    u32 ebp;
    // 虽然 pushad 把 esp 也压入，但 esp 是不断变化的，所以会被 popad 忽略
    u32 esp_dummy;

    u32 ebx;
    u32 edx;
    u32 ecx;
    u32 eax;

    u32 gs;
    u32 fs;
    u32 es;
    u32 ds;

    u32 vector0;

    u32 error;

    u32 eip;
    u32 cs;
    u32 eflags;
    u32 esp;
    u32 ss;
} intr_frame_t;

void task_yield();
void task_exit(int status);
pid_t task_fork();
pid_t task_waitpid(pid_t pid, int32* status);

task_t* running_task();
void schedule();

void task_block(task_t* task, list_t* blist, task_state_t state);
void task_unblock(task_t* task);

void task_sleep(u32 ms);
void task_wakeup();

fd_t task_get_fd(task_t* task);
void task_put_fd(task_t* task, fd_t fd);

#endif