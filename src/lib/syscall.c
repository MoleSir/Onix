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

static _inline u32 _syscall4(u32 nr, u32 arg1, u32 arg2, u32 arg3, u32 arg4)
{
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(nr), "b"(arg1), "c"(arg2), "d"(arg3), "s"(arg4));
    return ret;
}

static _inline u32 _syscall5(u32 nr, u32 arg1, u32 arg2, u32 arg3, u32 arg4, u32 arg5)
{
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(nr), "b"(arg1), "c"(arg2), "d"(arg3), "s"(arg4), "D"(arg5));
    return ret;
}

static _inline u32 _syscall6(u32 nr, u32 arg1, u32 arg2, u32 arg3, u32 arg4, u32 arg5, u32 arg6)
{
    u32 ret;
    asm volatile(
        "pushl %%ebp\n"
        "movl %7, %%ebp\n"
        "int $0x80\n"
        "popl %%ebp\n"
        : "=a"(ret)
        : "a"(nr), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4), "D"(arg5), "m"(arg6));
    return ret;
}

u32 test()
{
    return _syscall0(SYS_NR_TEST);
}

int32 brk(void* addr)
{
    return _syscall1(SYS_NR_BRK, (u32)addr);
}

void yield()
{
    _syscall0(SYS_NR_YIELD);
}

void sleep(u32 ms)
{
    _syscall1(SYS_NR_SLEEP, (u32)ms);
}

int32 write(fd_t fd, char* buf, u32 len)
{
    return _syscall3(SYS_NR_WRITE, (u32)fd, (u32)buf, (u32)len);
}

int32 read(fd_t fd, char* buf, u32 len)
{
    return _syscall3(SYS_NR_READ, (u32)fd, (u32)buf, (u32)len);
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
    return _syscall2(SYS_NR_WAITPID, pid, (u32)status);
}

time_t time()
{
    return _syscall0(SYS_NR_TIME);
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

int lseek(fd_t fd, off_t offset, int whence)
{
    return _syscall3(SYS_NR_LSEEK, (u32)fd, (u32)offset, (u32)whence);
}

char* getcwd(char *buf, size_t size)
{
    return (char*) _syscall2(SYS_NR_GETCWD, (u32)buf, (u32)size);
}

int chdir(char *pathname)
{
    return _syscall1(SYS_NR_CHDIR, (u32)pathname);
}

int chroot(char *pathname)
{
    return _syscall1(SYS_NR_CHROOT, (u32)pathname);
}

int readdir(fd_t fd, void* dir, u32 count)
{
    return _syscall3(SYS_NR_READDIR, fd, (u32)dir, (u32)count);
}

void clear()
{
    _syscall0(SYS_NR_CLEAR);
}

int stat(char *filename, stat_t *statbuf)
{
    return _syscall2(SYS_NR_STAT, (u32)filename, (u32)statbuf);
}

int fstat(fd_t fd, stat_t *statbuf)
{
    return _syscall2(SYS_NR_FSTAT, (u32)fd, (u32)statbuf);
}

int mknod(char* filename, int mode, int dev)
{
    return _syscall3(SYS_NR_MKNOD, (u32)filename, (u32)mode, (u32)dev);
}

int mount(char *devname, char *dirname, int flags)
{
    return _syscall3(SYS_NR_MOUNT, (u32)devname, (u32)dirname, (u32)flags);
}

int umount(char *target)
{
    return _syscall1(SYS_NR_UMOUNT, (u32)target);
}

int mkfs(char* devname, int icount)
{
    return _syscall2(SYS_NR_MKFS, (u32)devname, (u32)icount);
}

void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    return (void*) _syscall6(SYS_NR_MMAP, 
                  (u32)addr, (u32)length, (u32)prot, (u32)flags, (u32)fd, (u32)offset);
}

int munmap(void* addr, size_t length)
{
    return _syscall2(SYS_NR_MUNMAP, (u32)addr, (u32)length);
}

int execve(char* filename, char* argvp[], char* envp[])
{
    return _syscall3(SYS_NR_EXECVE, (u32)filename, (u32)argvp, (u32)envp);
}

fd_t dup(fd_t oldfd)
{
    return _syscall1(SYS_NR_DUP, (u32)oldfd);
}

fd_t dup2(fd_t oldfd, fd_t newfd)
{
    return _syscall2(SYS_NR_DUP2, (u32)oldfd, (u32)newfd);
}

int pipe(fd_t pipefd[2])
{
    return _syscall1(SYS_NR_PIPE, (u32)pipefd);
}