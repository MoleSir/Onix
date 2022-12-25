[org] 0x1000

dw 0x55aa; 魔数，用于判断错误

mov si, loading
call print

detect_memory:
    ; 将 ebx 置为 0
    xor ebx, ebx

    ; es:di 结构体的缓存位置
    mov ax, 0
    mov es, ax
    mov edi, ards_buffer

    mov edx, 0x534d4150; 固定签名

.next:
    ; 子功能号
    mov eax, 0xe820
    ; ards 结构大小（对应单位是字节）
    mov ecx, 20
    ; 系统调用
    int 0x15

    ; 如果 CF 置为表示出错
    jc error

    ; 将缓存指针指向下一个结构体
    add di, cx

    ; 将结构体数量加 1
    inc word [ards_count]

    ; 如果 ebx 是 0，表示检测结束
    cmp ebx, 0
    ; 不是 0 继续检查下一关
    jnz .next

    mov si, detecting
    call print

    jmp prepare_protected_mode


prepare_protected_mode:
    cli; 关闭中断

    ; 打开 A20 线
    in al, 0x92; 将 0x92 端口的数据写入 al
    or al, 0b10; 将 al 寄存器的第 1 为设置为 1
    out 0x92, al; 将设置 1 后的 al 写回 0x92 端口

    ; 加载 gdt
    lgdt [gdt_ptr]

    ; 启动保护模式
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; 用跳转来刷新缓存，启用保护模式
    jmp dword code_selector:protect_mode


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

loading:
    db "Loading Onix...", 10, 13, 0;
detecting:
    db "Detecting Memory Success...", 10, 13, 0;

error:
    mov si, .msg
    call print
    hlt; CPU 停止
    jmp $ 
    .msg db "Loading Error!", 10, 13, 0;


[bits 32]
protect_mode:
    mov ax, data_selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax; 初始化段寄存器

    mov esp, 0x10000; 修改栈顶

    ; 读取内核代码到内存 0x10000 处，其位于磁盘的第 10 个扇区，连续 200 个
    mov edi, 0x10000; 读取的目标内存
    mov ecx, 10; 读取扇区数量
    mov bl, 200; 起始扇区位置
    call read_disk

    ; 跳转到内核
    jmp dword code_selector:0x10000

    ud2; 表示出错


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



code_selector equ (1 << 3); 代码段选择子
data_selector equ (2 << 3); 数据段选择子

memory_base equ 0; 内存开始的位置：基地址
memory_limit equ ((1024 * 1024 * 1024 * 4) / (1024 * 4)) - 1; 内存界限（内存长度 = 4G / 4K - 1）

gdt_ptr:
    dw (gdt_end - gdt_base) - 1; gdt 界限，长度 - 1
    dd gdt_base; gdt 起始位置
gdt_base:; gdt 起始位置
    ; 0 索引描述符，要求为 0
    dd 0, 0; 
gdt_code:
    ; 1 索引描述符，为代码段描述符
    dw memory_limit & 0xffff; 段界限的 0 - 15 位
    dw memory_base & 0xffff; 基地址的 0 - 15 位
    db (memory_base >> 16) & 0xff; 基地址 16 - 23 位
    db 0b_1_00_1_1_0_1_0; 存在、dlp 为 00、代码、非依从、可读、没有被访问过
    db 0b1_1_0_0_0000 | (memory_limit >> 16) & 0xf; 4K、32位、不是 64 位、一个无效位、段界限的 16 - 19 
    db (memory_base >> 24) & 0xff; 基地址 24 - 31 位
gdt_data:
    ; 2 索引描述符，为数据段描述符
    dw memory_limit & 0xffff; 段界限的 0 - 15 位
    dw memory_base & 0xffff; 基地址的 0 - 15 位
    db (memory_base >> 16) & 0xff; 基地址 16 - 23 位
    db 0b_1_00_1_0_0_1_0; 存在、dlp 为 00、数据、向上拓展、可写、没有被访问过
    db 0b1_1_0_0_0000 | (memory_limit >> 16) & 0xf; 4K、32位、不是 64 位、一个无效位、段界限的 16 - 19 
    db (memory_base >> 24) & 0xff; 基地址 24 - 31 位
gdt_end:

ards_count:
    dw 0 
ards_buffer:
