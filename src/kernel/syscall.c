#include <onix/syscall.h>

static _inline u32 _syscall0(u32 nr)
{
    u32 ret;
    // 执行 int 0x80 之前，把系统调用号放入 eax 寄存器
    // 最后的返回值放入 ret
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(nr));
    return ret;
}

static _inline u32 _syscall1(u32 nr, u32 arg)
{
    u32 ret;
    // 执行 int 0x80 之前，把系统调用号放入 eax 寄存器，参数传入 ebx 寄存器
    // 最后的返回值放入 ret
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(nr), "b"(arg));
    return ret;
}

static _inline u32 _syscall2(u32 nr, u32 arg1, u32 arg2)
{
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(nr), "b"(arg1), "c"(arg2));
    return ret;
}

static _inline u32 _syscall3(u32 nr, u32 arg1, u32 arg2, u32 arg3)
{
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(nr), "b"(arg1), "c"(arg2), "d"(arg3));
    return ret;
}

u32 test()
{
    return _syscall0(SYS_NR_TEST);
}

int32 brk(void* addr)
{
    _syscall1(SYS_NR_BRK, (u32)addr);
}

void yield()
{
    _syscall0(SYS_NR_YIELD);
}

void sleep(u32 ms)
{
    _syscall1(SYS_NR_SLEEP, ms);
}

int32 write(fd_t fd, char* buf, u32 len)
{
    return _syscall3(SYS_NR_WRITE, fd, (u32)buf, len);
}

pid_t getpid()
{
    _syscall0(SYS_NR_GETPID);
}

pid_t getppid()
{
    _syscall0(SYS_NR_GETPPID);
}

pid_t fork()
{
    return _syscall0(SYS_NR_FORK);
}

void exit(int status)
{
    _syscall1(SYS_NR_EXIT, (u32)status);
}

pid_t waitpid(pid_t pid, int32* status)
{
    _syscall2(SYS_NR_WAITPID, pid, (u32)status);
}

time_t time()
{
    _syscall0(SYS_NR_TIME);
}

mode_t umask(mode_t mask)
{
    return _syscall1(SYS_NR_UMASK, (u32)mask);
}

int mkdir(char* pathname, int mode)
{
    return _syscall2(SYS_NR_MKDIR, (u32)pathname, (u32)mode);
}

int rmdir(char* pathname)
{
    return _syscall1(SYS_NR_RMDIR, (u32)pathname);
}

int link(char* oldname, char* newname)
{
    return _syscall2(SYS_NR_LINK, (u32)oldname, (u32)newname);
}

int unlink(char* filename)
{
    return _syscall1(SYS_NR_UNLINK, (u32)filename);
}

fd_t open(char* filename, int flags, int mode)
{
    return _syscall3(SYS_NR_OPEN, (u32)filename, (u32)flags, (u32)mode);
}

fd_t create(char* filename, int mode)
{
    return _syscall2(SYS_NR_CREATE, (u32)filename, (u32)mode);
}

void close(fd_t fd)
{
    _syscall1(SYS_NR_CLOSE, (u32)fd);
}