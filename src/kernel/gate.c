#include <onix/interrupt.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/syscall.h>
#include <onix/console.h>
#include <onix/memory.h>
#include <onix/task.h>
#include <onix/ide.h>
#include <onix/device.h>
#include <onix/buffer.h>
#include <string.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define SYSCALL_SIZE 256

handler_t syscall_table[SYSCALL_SIZE];

void syscall_check(u32 nr)
{
    if (nr >= SYSCALL_SIZE)
        panic("syscall nr error!!!");
}

static void sys_default()
{
    panic("syscall not implement!!!");
}

static task_t* task = NULL;

extern ide_ctrl_t controllers[IDE_CTRL_NR];
extern void dir_test();

static u32 sys_test()
{
    char ch;
    device_t* device;

    device_t* serial = device_find(DEV_SERIAL, 0);
    assert(serial);

    device_t* keyboard = device_find(DEV_KEYBOARD, 0);
    assert(keyboard);

    device_t* console = device_find(DEV_CONSOLE, 0);
    assert(console);

    device_read(serial->dev, &ch, 1, 0, 0);
    device_write(serial->dev, &ch, 1, 0, 0);
    device_write(console->dev, &ch, 1, 0, 0);
}

extern int32 console_write(void* dev, char* buf, u32 count);

extern u32 sys_write(fd_t fd, char* buf, u32 count);
extern u32 sys_read(fd_t fd, char* buf, u32 count);
extern int sys_lseek(fd_t fd, off_t offset, int whence);

extern pid_t sys_getpid();
extern pid_t sys_getppid();
extern void task_yield();
extern u32 sys_time();
extern mode_t sys_umask(mode_t mask);
extern int sys_mkdir(char*, int);
extern int sys_rmdir(char*);

extern int sys_link(char*, char*);
extern int sys_unlink(char*);
extern int sys_readdir(fd_t fd, dirent_t* dir, u32 count);

extern fd_t sys_open(char* filename, int flags, int mode);
extern fd_t sys_create(char* filename, int mode);
extern void sys_close(fd_t fd);

extern int sys_chdir(char *pathname);
extern int sys_chroot(char *pathname);
extern char* sys_getcwd(char *buf, size_t size);

extern void console_clear();

extern int sys_stat(char *filename, stat_t *statbuf);
extern int sys_fstat(fd_t fd, stat_t *statbuf);

extern int sys_mknod(char* filename, int mode, int dev);

extern int sys_mount(char *devname, char *dirname, int flags);
extern int sys_umount(char *target);

extern int sys_mkfs(char* devname, int icount);

extern int32 sys_brk(void* addr);
extern void* sys_mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset);
extern int sys_munmap(void* addr, size_t length);

extern int sys_execve(char* filename, char* argvp[], char* envp[]);

void syscall_init()
{
    for (size_t i = 0; i < SYSCALL_SIZE; ++i)
    {
        syscall_table[i] = sys_default;
    }

    syscall_table[SYS_NR_TEST] = sys_test;
    syscall_table[SYS_NR_EXIT] = task_exit;
    syscall_table[SYS_NR_FORK] = task_fork;
    syscall_table[SYS_NR_WAITPID] = task_waitpid;
    syscall_table[SYS_NR_TIME] = sys_time;
    syscall_table[SYS_NR_BRK] = sys_brk;
    syscall_table[SYS_NR_WRITE] = sys_write;
    syscall_table[SYS_NR_READ] = sys_read;
    syscall_table[SYS_NR_SLEEP] = task_sleep;
    syscall_table[SYS_NR_YIELD] = task_yield;
    syscall_table[SYS_NR_GETPID] = sys_getpid;
    syscall_table[SYS_NR_GETPPID] = sys_getppid;
    syscall_table[SYS_NR_UMASK] = sys_umask;
    syscall_table[SYS_NR_MKDIR] = sys_mkdir;
    syscall_table[SYS_NR_RMDIR] = sys_rmdir;
    syscall_table[SYS_NR_LINK] = sys_link;
    syscall_table[SYS_NR_UNLINK] = sys_unlink;
    syscall_table[SYS_NR_OPEN] = sys_open;
    syscall_table[SYS_NR_CLOSE] = sys_close;
    syscall_table[SYS_NR_CREATE] = sys_create;
    syscall_table[SYS_NR_LSEEK] = sys_lseek;
    syscall_table[SYS_NR_GETCWD] = sys_getcwd;
    syscall_table[SYS_NR_CHROOT] = sys_chroot;
    syscall_table[SYS_NR_CHDIR] = sys_chdir;
    syscall_table[SYS_NR_READDIR] = sys_readdir;
    syscall_table[SYS_NR_CLEAR] = console_clear;
    syscall_table[SYS_NR_STAT] = sys_stat;
    syscall_table[SYS_NR_FSTAT] = sys_fstat;
    syscall_table[SYS_NR_MKNOD] = sys_mknod;
    syscall_table[SYS_NR_MOUNT] = sys_mount;
    syscall_table[SYS_NR_UMOUNT] = sys_umount;
    syscall_table[SYS_NR_MKFS] = sys_mkfs;
    syscall_table[SYS_NR_MMAP] = sys_mmap;
    syscall_table[SYS_NR_MUNMAP] = sys_munmap;
    syscall_table[SYS_NR_EXECVE] = sys_execve;
}