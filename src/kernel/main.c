#include <onix/debug.h>
#include <onix/interrupt.h>
#include <onix/types.h>

extern void interrupt_init();
extern void clock_init();
extern void time_init();
extern void rtc_init();
extern void memory_map_init();
extern void mapping_init();
extern void task_init();
extern void syscall_init();
extern void hang();

void kernel_init()
{
    interrupt_init();
    memory_map_init();
    mapping_init();
    clock_init();

    // time_init();
    // rtc_init(); 

    task_init();
    syscall_init();
    
    //asm volatile("sti\n");
    list_test();

    hang();
}