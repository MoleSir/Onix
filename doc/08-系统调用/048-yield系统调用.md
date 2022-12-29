# 系统调用 `yield`

## 封装系统调用

对用户应用程序来说，应该对操作系统是封闭的，所以应用程序不知道系统调用的中断号，也不知道哪个系统调用对应哪个系统调用号；

所以为了向用户提供服务，操作系统需要将一个个系统调用封装；

所以，在新建了 syscall.h 与 syscall.c，将系统调用封装成 C 函数给用户使用；



## `yield`

`yield()`系统调用，执行这个系统调用的进程主动交出执行权，调度执行其他进程；

### 定义系统调用号与接口

在 syscall.h 中定义：

````c
typedef enum syscall_t
{
    SYS_NR_TEST,
    SYS_NR_YIELD,
} syscall_t;

u32 test();
void yield();
````

目前就两个系统调用：

- `test`：就是之前打印输出 `syscall test...` 的那个 0 号系统调用；
- `yield`：二号系统调用，使得执行这个函数的用户进程主动放弃执行权，相当于调用 `schedule`；

之后每增加一个系统调用就在这个文件中的 `syscall_t` 新增一个枚举，新增一个接口；

### 系统函数实现

在 syscall.c 中实现：

````c
static _inline u32 _syscall0(u32 nr)
{
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "a="(ret)
        : "a"(nr));
    return ret;
}

u32 test()
{
    return _syscall0(SYS_NR_TEST);
}

void yield()
{
    _syscall0(SYS_NR_YIELD);
}
````

用户可以做的只是传入系统调用号，之后执行 `int 0x80`，如果还有参数的还可以穿参数，这里两个都没有，之后会拓展多参数的；

> 系统调用的接口只负责产生 `int 0x80` 中断，进入内核之后才有足够的权限；
>
> 所以这里所谓的封装，就是封装了系统调用号，用户不需要记住系统调用对应的系统号与中断号，只要看函数名称即可；

### 处理函数实现

真正的处理函数是位于 `syscall_table` 中保存的函数指针：

对 0 号系统调用 `test`，处理函数是：

```
static u32 sys_test()
{
    LOGK("syscall test...\n");
    return 255;
}
```

对 1 号系统调用 `yield`，处理函数是：

````c
void task_yield()
{
    schedule();
}
````

在其中执行 `schedule`，进行调度；

### 注册处理函数

在 `syscall_table` 填入处理函数指针即可：

````c
void syscall_init()
{
    for (size_t i = 0; i < SYSCALL_SIZE; ++i)
    {
        syscall_table[i] = sys_default;
    }

    syscall_table[SYS_NR_TEST] = sys_test;
    syscall_table[SYS_NR_YIELD] = task_yield;
}
````



