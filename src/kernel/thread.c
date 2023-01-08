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

extern u32 keyboard_read(char* buf, u32 count);

static void user_init_thread()
{
    char buf[256];
    memset(buf, 'A', sizeof(buf));

    fd_t fd;
    int len = 0;
    fd = open("/hello.txt", O_RDWR, 0755);
    lseek(fd, 5, SEEK_SET);
    len = write(fd, buf, sizeof(buf));
    close(fd);
    
    while (true)
    {
        char ch;
        read(stdin, &ch, 1);
        write(stdout, &ch, 1);
    }
}

extern void task_to_user_mode(target_t target);
void init_thread()
{
    char temp[100];
    task_to_user_mode(user_init_thread);        
}

void test_thread()
{
    set_interrupt_state(true);
    while (true)
    {
        sleep(10000);
    }
}