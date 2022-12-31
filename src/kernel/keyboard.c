#include <onix/interrupt.h>
#include <onix/io.h>
#include <onix/assert.h>
#include <onix/debug.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define KEYBOARD_DATA_PROT 0x60
#define KEYBOARD_CTRL_PROT 0x64

void keyboard_handler(int vector)
{
    assert(vector == 0x21);
    // 中断处理完成
    send_eoi(vector);
    // 读取按键信息
    u16 scancode = inb(KEYBOARD_DATA_PROT);
    LOGK("keyboard input %d\n", scancode);
}

void keyboard_init()
{
    // 设置按键中断处理函数
    set_interrupt_handler(IRQ_KEYBOARD, keyboard_handler);
    // 设置按键中断有效
    set_interrupt_mask(IRQ_KEYBOARD, true);
}