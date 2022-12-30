#include <onix/interrupt.h>
#include <onix/syscall.h>
#include <onix/debug.h>

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

void init_thread()
{
    set_interrupt_state(true);

    while (true)
    {
        LOGK("init task...\n");
        sleep(500);
    }
}

void test_thread()
{
    set_interrupt_state(true);
    u32 counter = 0;

    while (true)
    {
        LOGK("test task %d...\n", counter++);
        sleep(709);
    }
}