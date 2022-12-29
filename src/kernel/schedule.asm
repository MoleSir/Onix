[bits 32]

section .text

global task_switch
task_switch:
    push ebp
    mov ebp, esp

    push ebx
    push esi
    push edi

    ; 将当前栈顶放入 eax 寄存器
    mov eax, esp
    ; 将 eax 寄存器的低 3 字节设置为 0，即找到每个页开始的地址（一页 0x1000，最后 3 字节为 0）
    and eax, 0xfffff000;

    ; 将当前这个任务的 esp 传入 eax 寄存器指向的内存，也极当前页面的第一个内存位置，保存了当前任务的 esp
    mov [eax], esp

    ; 取出函数的参数，是一个指针，到 eax 中
    mov eax, [ebp + 8]
    ; 取出 eax 指向的内存的值放入到 esp，这个值是另一个任务之前保存在页面起始位置的 esp
    mov esp, [eax]

    pop edi
    pop esi
    pop ebx
    pop ebp

    ret