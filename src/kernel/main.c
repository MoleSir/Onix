extern void console_init();
extern void gdt_init();
extern void interrupt_init();
extern void clock_init();
extern void hang();

void kernel_init()
{
    console_init();
    gdt_init();
    interrupt_init();
    clock_init();
    //task_init();

    asm volatile("sti");
    hang();
}