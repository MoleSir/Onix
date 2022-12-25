[bits 32]

section .text; 代码段

global inb; 将 inb 导出
inb:
    push  ebp
    mov   ebp, esp

    xor eax, eax ; 清空 eax
    mov edx, [ebp + 8]; port 
    in al, dx; 将端口号 dx 的 8 bit 输入到 ax

    jmp $+2 ; 一点点延迟
    jmp $+2 ; 一点点延迟
    jmp $+2 ; 一点点延迟

    leave 
    ret

global outb; 将 outb 导出
outb:
    push  ebp
    mov   ebp, esp

    mov edx, [ebp + 8]; port 
    mov eax, [ebp + 12]; value
    out dx, al; 将 al 中的 8 bit 写入端口 dx

    jmp $+2 ; 一点点延迟
    jmp $+2 ; 一点点延迟
    jmp $+2 ; 一点点延迟

    leave 
    ret

global inw; 将 inw 导出
inw:
    push  ebp
    mov   ebp, esp

    xor eax, eax ; 清空 eax
    mov edx, [ebp + 8]; port 
    in ax, dx; 将端口号 dx 的 16 bit 输入到 ax

    jmp $+2 ; 一点点延迟
    jmp $+2 ; 一点点延迟
    jmp $+2 ; 一点点延迟

    leave 
    ret

global outw; 将 outb 导出
outw:
    push  ebp
    mov   ebp, esp

    mov edx, [ebp + 8]; port 
    mov eax, [ebp + 12]; value
    out dx, ax; 将 al 中的 16 bit 写入端口 dx

    jmp $+2 ; 一点点延迟
    jmp $+2 ; 一点点延迟
    jmp $+2 ; 一点点延迟

    leave 
    ret