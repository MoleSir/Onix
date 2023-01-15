#include <onix/interrupt.h>
#include <onix/syscall.h>
#include <onix/debug.h>
#include <onix/task.h>
#include <onix/printk.h>
#include <onix/mutex.h>
#include <onix/arena.h>
#include <onix/types.h>
#include <stdio.h>
#include <string.h>

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

extern void dev_init();
void init_thread()
{
    char temp[100];
    dev_init();
    task_to_user_mode();        
}

void test_thread()
{
    set_interrupt_state(true);
    while (true)
    {
        sleep(10000);
    }
}