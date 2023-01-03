#include <onix/interrupt.h>
#include <onix/syscall.h>
#include <onix/debug.h>
#include <onix/task.h>
#include <onix/printk.h>
#include <onix/mutex.h>
#include <onix/arena.h>
#include <stdio.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

void idle_thread()
{
    set_interrupt_state(true);
    u32 counter = 0;
    while (true)
    {
        // LOGK("idle task... %d\n", counter++);
        asm volatile(
            "sti\n"
            "hlt\n"
        );
        yield();
    }
}

extern u32 keyboard_read(char* buf, u32 count);

static void user_init_thread()
{
    u32 counter = 0;
    while (true)
    {
        pid_t pid = fork();
        int status;

        if (pid)
        {
            printf("fork after parent %d, %d, %d\n", pid, getpid(), getppid());
            sleep(1000);
            pid_t child = waitpid(pid, &status);
            printf("wait pid %d status %d %d\n", child, status, time());
        }
        else
        {
            printf("fork after child %d, %d, %d\n", pid, getpid(), getppid());
            //sleep(1000);
            exit(0);
        }
        sleep(1000);
    }
}

extern void task_to_user_mode(target_t target);
void init_thread()
{
    // set_interrupt_state(true);
    char temp[100];
    task_to_user_mode(user_init_thread);        
}

void test_thread()
{
    set_interrupt_state(true);
    u32 counter = 0;

    while (true)
    {
        printk("task thread %d, %d, %d\n", getpid(), getppid(), counter++);
        sleep(2000);
    }
}