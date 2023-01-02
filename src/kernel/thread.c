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

void test_recursion()
{
    char tmp[0x400];
    test_recursion();
}

static void user_init_thread()
{
    u32 counter = 0;
    while (true)
    {
        printf("task is in user mode %d\n", counter++);
        BMB;
        //test_recursion();
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
        LOGK("test task %d...\n", counter++);
        BMB;
        sleep(2000);
    }
}