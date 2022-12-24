[org 0x7c00]

; 设置屏幕为文本模式，清除模式
mov ax, 3
int 0x10

; 初始化段寄存器
mov ax, 0
mov ds, ax
mov es, ax
mov ss, ax
mov sp, 0x7c00

; 0xb8000 文本显示器的内存
mov ax, 0xb800
mov ds, ax
mov byte [0], 'H'
mov byte [2], 'e'
mov byte [4], 'l'
mov byte [6], 'l'
mov byte [8], '0'
mov byte [10], ' '
mov byte [12], 'W'
mov byte [14], 'o'
mov byte [16], 'r'
mov byte [18], 'l'
mov byte [20], 'd'
mov byte [22], '!'

; 阻塞
jmp $

; 填充 0
times 510 - ($ - $$) db 0

; 主引导扇区的最后两个字节必须是 0x55 0xaa
db 0x55, 0xaa
