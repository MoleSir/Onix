#include <onix/interrupt.h>
#include <onix/global.h>
#include <onix/debug.h>
#include <stdlib.h>
#include <onix/printk.h>
#include <onix/io.h>
#include <onix/assert.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)
// #define LOGK(fmt, args...)

#define ENTRY_SIZE 0x30

#define PIC_M_CTRL 0x20 // 主片的控制端口
#define PIC_M_DATA 0x21 // 主片的数据端口
#define PIC_S_CTRL 0xa0 // 从片的控制端口
#define PIC_S_DATA 0xa1 // 从片的数据端口
#define PIC_EOI 0x20    // 通知中断控制器中断结束

gate_t idt[IDT_SIZE];
pointer_t idt_ptr;

handler_t handler_table[IDT_SIZE];
extern handler_t handler_entry_table[ENTRY_SIZE];
extern void syscall_handler();
extern void page_fault();

static char *messages[] = {
    "#DE Divide Error\0",
    "#DB RESERVED\0",
    "--  NMI Interrupt\0",
    "#BP Breakpoint\0",
    "#OF Overflow\0",
    "#BR BOUND Range Exceeded\0",
    "#UD Invalid Opcode (Undefined Opcode)\0",
    "#NM Device Not Available (No Math Coprocessor)\0",
    "#DF Double Fault\0",
    "    Coprocessor Segment Overrun (reserved)\0",
    "#TS Invalid TSS\0",
    "#NP Segment Not Present\0",
    "#SS Stack-Segment Fault\0",
    "#GP General Protection\0",
    "#PF Page Fault\0",
    "--  (Intel reserved. Do not use.)\0",
    "#MF x87 FPU Floating-Point Error (Math Fault)\0",
    "#AC Alignment Check\0",
    "#MC Machine Check\0",
    "#XF SIMD Floating-Point Exception\0",
    "#VE Virtualization Exception\0",
    "#CP Control Protection Exception\0",
};

// 通知中断控制器，中断处理结束
void send_eoi(int vector)
{
    if (vector >= 0x20 && vector < 0x28)
    {
        outb(PIC_M_CTRL, PIC_EOI);
    }
    if (vector >= 0x28 && vector < 0x30)
    {
        outb(PIC_M_CTRL, PIC_EOI);
        outb(PIC_S_CTRL, PIC_EOI);
    }
}

// 对 irq 号外中断设置处理函数
void set_interrupt_handler(u32 irq, handler_t handler)
{
    assert(irq >= 0 && irq < 16);
    handler_table[IRQ_MASTER_NR + irq] = handler;
}

// 启动或关闭第 irq 号外中断
void set_interrupt_mask(u32 irq, bool enable)
{
    assert(irq >= 0 && irq < 16);
    u16 port;
    // 端口号小于 8 是主片控制的中断
    if (irq < 8)
        port = PIC_M_DATA;
    // 大于 8 是从片控制的中断
    else
    {
        port = PIC_S_DATA;
        irq -= 8;
    }
    // 开启或关闭对应的端口，即使能或失能对应的时钟中断（0 有效）
    if (enable)
        outb(port, inb(port) & (~(1 << irq)));
    else
        outb(port, inb(port) | (1 << irq));
}

// 默认外中断处理函数
void default_handler(int vector)
{
    send_eoi(vector);
    DEBUGK("[%x] default interrupt called...\n", vector);
}

// 默认异常处理函数
void exception_handler(
    int vector,
    u32 edi, u32 esi, u32 ebp, u32 esp,
    u32 ebx, u32 edx, u32 ecx, u32 eax,
    u32 gs, u32 fs, u32 es, u32 ds,
    u32 vector0, u32 error, u32 eip, u32 cs, u32 eflags)
{
    printk("\nEXCEPTION : %s \n", messages[vector]);
    printk("   VECTOR : 0x%02X\n", vector);
    printk("    ERROR : 0x%08X\n", error);
    printk("   EFLAGS : 0x%08X\n", eflags);
    printk("       CS : 0x%02X\n", cs);
    printk("      EIP : 0x%08X\n", eip);
    printk("      ESP : 0x%08X\n", esp);
    // 阻塞
    hang();
}

// 初始化中断控制器
void pic_init() 
{
    outb(PIC_M_CTRL, 0x11);
    outb(PIC_S_CTRL, 0x11);

    outb(PIC_M_DATA, 0x20);
    outb(PIC_S_DATA, 0x28);

    outb(PIC_M_DATA, 0x04);
    outb(PIC_S_DATA, 0x02);

    outb(PIC_M_DATA, 0x01);
    outb(PIC_S_DATA, 0x01);

    // 默认所有 16 个外中断全部关闭，后面有需要再打开
    outb(PIC_M_DATA, 0b11111111);
    outb(PIC_S_DATA, 0b11111111);
}

// 清除 IF 位，返回设置之前的值
bool interrupt_disable()
{
    asm volatile(
        "pushfl\n"          // 将当前的 eflags 压入栈中
        "cli\n"             // 清除 IF 位，此时外中断被屏蔽
        "popl %eax\n"       // 将刚才压入的 eflags 弹出到 eax
        "shrl $9, %eax\n"   // 将 eax 右移 9 位，得到 IF 
        "andl $1, %eax\n"   // 只需要 IF，其他位设置为 0
    );
}

// 获得 IF 位
bool get_interrupt_state()
{
    asm volatile(
        "pushfl\n"          // 将当前的 eflags 压入栈中
        "popl %eax\n"       // 入栈的 eflags 进入 eax
        "shrl $9, %eax\n"   // 将 eax 右移 9 位，得到 IF 
        "andl $1, %eax\n"   // 只需要 IF，其他位设置为 0
    );
}

// 设置 IF 位
void set_interrupt_state(bool state)
{
    if (state)
        asm volatile("sti\n");
    else
        asm volatile("cli\n");
}

// 初始化中断描述符，和中断处理函数数组
void idt_init()
{
    for (size_t i = 0; i < ENTRY_SIZE; i++)
    {
        gate_t *gate = &idt[i];
        handler_t handler = handler_entry_table[i];

        gate->offset0 = (u32)handler & 0xffff;
        gate->offset1 = ((u32)handler >> 16) & 0xffff;
        gate->selector = 1 << 3; // 代码段
        gate->reserved = 0;      // 保留不用
        gate->type = 0b1110;     // 中断门
        gate->segment = 0;       // 系统段
        gate->DPL = 0;           // 内核态
        gate->present = 1;       // 有效
    }

    for (size_t i = 0; i < 0x20; i++)
    {
        handler_table[i] = exception_handler;
    }

    handler_table[0xe] = page_fault;

    for (size_t i = 0x20; i < ENTRY_SIZE; i++)
    {
        handler_table[i] = default_handler;
    }

    // 初始化系统调用
    gate_t* gate = &(idt[0x80]);
    gate->offset0 = (u32)syscall_handler & 0xffff;
    gate->offset1 = ((u32)syscall_handler >> 16) & 0xffff;
    gate->selector = 1 << 3; // 代码段
    gate->reserved = 0;      // 保留不用
    gate->type = 0b1110;     // 中断门
    gate->segment = 0;       // 系统段
    gate->DPL = 3;           // 用户态，int 80 可以在用户态执行
    gate->present = 1;       // 有效

    idt_ptr.base = (u32)idt;
    idt_ptr.limit = sizeof(idt) - 1;

    asm volatile("lidt idt_ptr\n");
}

void interrupt_init()
{
    pic_init();
    idt_init();
}