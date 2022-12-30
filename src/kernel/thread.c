#include <onix/interrupt.h>
#include <onix/syscall.h>
#include <onix/debug.h>
#include <onix/mutex.h>

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

mutex_t mutex;

void init_thread()
{
    mutex_init(&mutex);
    set_interrupt_state(true);

    while (true)
    {
        mutex_lock(&mutex);
        LOGK("init task...\n");
        mutex_unlock(&mutex);
    }
}

void test_thread()
{
    set_interrupt_state(true);
    u32 counter = 0;

    while (true)
    {
        mutex_lock(&mutex);
        LOGK("test task %d...\n", counter++);
        mutex_unlock(&mutex);
    }
}