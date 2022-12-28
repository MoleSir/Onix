[bits 32]

extern kernel_init
extern console_init
extern memory_init

global _start
_start:
    push ebx; ards_count
    push eax; magic

    call console_init; 控制台初始化
    call memory_init;
    
    jmp $