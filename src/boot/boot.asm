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

mov si, booting
call print

mov edi, 0x1000; 读取的目标内存
mov ecx, 2; 读取扇区数量
mov bl, 4; 起始扇区位置
call read_disk

cmp word [0x1000], 0x55aa
jnz error
jmp 0:0x1002

; 阻塞
jmp $

read_disk:
    ; 每次的 dx 对应硬盘的几个端口
    mov dx, 0x1f2
    mov al, bl
    out dx, al; out 就是向 dx 端口输入 al

    inc dx; 0x1f3
    mov al, cl; 起始扇区的前 8 位
    out dx, al;

    inc dx; 0x1f4
    shr ecx, 8
    mov al, cl; 起始扇区的中 8 位
    out dx, al;

    inc dx; 0x1f5
    shr ecx, 8
    mov al, cl; 起始扇区的高 8 位
    out dx, al;

    inc dx; 0x1f6
    shr ecx, 8
    and cl, 0b1111; 将高4位设置为 0

    mov al, 0b1110_0000;
    or al, cl
    out dx, al; 主盘 - LBA 模式

    inc dx; 0x1f7
    mov al, 0x20; 读硬盘
    out dx, al

    xor ecx, ecx; 清空 ecx
    mov cl, bl; 得到读写扇区的数量

    .read:
        push cx; 保存 cx 寄存器
        call .waits; 等待数据准备完毕
        call .reads; 读取一个扇区
        pop cx
        loop .read

    ret


    .waits:
        ; 根据 0x1f7 判断是否准备完毕
        mov dx, 0x1F7
        .check:
            in al, dx ; in 与 out 相对，获得端口信息
            jmp $+2; 什么都不做，但会消耗时钟周期
            jmp $+2
            jmp $+2
            and al, 0b10001000
            cmp al, 0b00001000
            jnz .check

        ret
    
    .reads:
        ; 从端口读数据
        mov dx, 0x1f0
        mov cx, 256; 一个扇区 256 个字
        .readw:
            in ax, dx
            jmp $+2; 什么都不做，但会消耗时钟周期
            jmp $+2
            jmp $+2
            mov [edi], ax
            add edi, 2
            loop .readw
        ret


print:
    mov ah, 0x0e
.next:
    mov al, [si]
    cmp al, 0
    jz .done
    int 0x10
    inc si
    jmp .next
.done:
    ret

booting:
    db "Booting Onix...", 10, 13, 0; 10, 13, 0 表示 '\r\n\0'

error:
    mov si, .msg
    call print
    hlt; CPU 停止
    jmp $ 
    .msg db "Booting Error!", 10, 13, 0;

; 填充 0
times 510 - ($ - $$) db 0

; 主引导扇区的最后两个字节必须是 0x55 0xaa
db 0x55, 0xaa
