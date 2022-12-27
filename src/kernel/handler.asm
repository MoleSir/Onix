[bits 32]
; 中断处理函数入口

extern handler_table

section .text

; 宏的第一个参数表示中断对应处理函数的指针在中断向量表的下标，第二个参数表示这个中断函数是否需要压入参数
; 如果不需要，就手动 push 一个 0x20222202，没有别的意义，只是为了让中断函数的栈格式相同
; 这些函数的主要作用都是转递中断号，去调用 interrupt_entry
%macro INTERRUPT_HANDLER 2
interrupt_handler_%1
%ifn %2
    push 0x20222202
%endif
    push %1
    jmp interrupt_entry
%endmacro

; interrupt_entry 就是根据栈顶去 handler_table 里面找函数
interrupt_entry:
    ; 从栈顶取出中断向量表的下标
    mov eax, [esp]

    ; 调用中断处理函数，handler_table 中存储了处理函数的指针
    ; 一个指针占 4 个字节
    call [handler_table + eax * 4]

    ; 对应 push % 1，调用结束恢复栈
    add esp, 8
    iret


INTERRUPT_HANDLER 0x00, 0   ; 0 号中断：divide by zero，没有额外参数
INTERRUPT_HANDLER 0x01, 0   ; 1 号中断：bug，没有额外参数
INTERRUPT_HANDLER 0x02, 0   ; 2 号中断：non maskable interrupt，没有额外参数
INTERRUPT_HANDLER 0x03, 0   ; 3 号中断：breakpoint，没有额外参数

INTERRUPT_HANDLER 0x04, 0   ; 4 号中断：overfolw，没有额外参数
INTERRUPT_HANDLER 0x05, 0   ; 5 号中断：bound range exceeded，没有额外参数
INTERRUPT_HANDLER 0x06, 0   ; 6 号中断：invalid opcode，没有额外参数
INTERRUPT_HANDLER 0x07, 0   ; 7 号中断：device not avilable，没有额外参数

INTERRUPT_HANDLER 0x08, 1   ; 8 号中断：double fault，有额外参数
INTERRUPT_HANDLER 0x09, 0   ; 9 号中断：coprocessor segment overrun，没有额外参数
INTERRUPT_HANDLER 0x0a, 1   ; a 号中断：invalid TSS，有额外参数
INTERRUPT_HANDLER 0x0b, 1   ; b 号中断：segment not present，有额外参数

INTERRUPT_HANDLER 0x0c, 1   ; c 号中断：stack segment fault，有额外参数
INTERRUPT_HANDLER 0x0d, 1   ; d 号中断：general protection fault，有额外参数
INTERRUPT_HANDLER 0x0e, 1   ; e 号中断：page fault，有额外参数
INTERRUPT_HANDLER 0x0f, 0   ; f 号中断：reserved

INTERRUPT_HANDLER 0x10, 0   ; 10 号中断：x87 floating point exception，没有额外参数
INTERRUPT_HANDLER 0x11, 1   ; 11 号中断：aligment check，有额外参数
INTERRUPT_HANDLER 0x12, 0   ; 12 号中断：machine check，没有额外参数
INTERRUPT_HANDLER 0x13, 0   ; 13 号中断：SIMD Floating - Point Exception

INTERRUPT_HANDLER 0x14, 0   ; 14 号中断：Virtualiztion Excetion，有额外参数
INTERRUPT_HANDLER 0x15, 1   ; 15 号中断：control protection exception，有额外参数
INTERRUPT_HANDLER 0x16, 0   ; 16 号中断：reserved
INTERRUPT_HANDLER 0x17, 0   ; 17 号中断：reserved

INTERRUPT_HANDLER 0x18, 0   ; 18 号中断：reserved
INTERRUPT_HANDLER 0x19, 0   ; 19 号中断：reserved
INTERRUPT_HANDLER 0x1a, 0   ; 1a 号中断：reserved
INTERRUPT_HANDLER 0x1b, 0   ; 1b 号中断：reserved

INTERRUPT_HANDLER 0x1c, 0   ; 1c 号中断：reserved
INTERRUPT_HANDLER 0x1d, 0   ; 1d 号中断：reserved
INTERRUPT_HANDLER 0x1e, 0   ; 1e 号中断：reserved
INTERRUPT_HANDLER 0x1f, 0   ; 1f 号中断：reserved

; 存放函数指针
section .data
global handler_entry_table
handler_entry_table:
    dd interrupt_handler_0x00
    dd interrupt_handler_0x01
    dd interrupt_handler_0x02
    dd interrupt_handler_0x03
    dd interrupt_handler_0x04
    dd interrupt_handler_0x05
    dd interrupt_handler_0x06
    dd interrupt_handler_0x07
    dd interrupt_handler_0x08
    dd interrupt_handler_0x09
    dd interrupt_handler_0x0a
    dd interrupt_handler_0x0b
    dd interrupt_handler_0x0c
    dd interrupt_handler_0x0d
    dd interrupt_handler_0x0f
    dd interrupt_handler_0x10
    dd interrupt_handler_0x11
    dd interrupt_handler_0x12
    dd interrupt_handler_0x13
    dd interrupt_handler_0x14
    dd interrupt_handler_0x15
    dd interrupt_handler_0x16
    dd interrupt_handler_0x17
    dd interrupt_handler_0x18
    dd interrupt_handler_0x19
    dd interrupt_handler_0x1a
    dd interrupt_handler_0x1b
    dd interrupt_handler_0x1c
    dd interrupt_handler_0x1d
    dd interrupt_handler_0x1f