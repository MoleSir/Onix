#ifndef __ONIX_SYSCALL_HH__
#define __ONIX_SYSCALL_HH__

#include <onix/types.h>

typedef enum syscall_t
{
    SYS_NR_TEST = 0,
    SYS_NR_EXIT = 1,
    SYS_NR_FORK = 2,
    SYS_NR_WRITE = 4,
    SYS_NR_WAITPID = 7,
    SYS_NR_LINK = 9,
    SYS_NR_UNLINK = 10,
    SYS_NR_TIME = 13,
    SYS_NR_GETPID = 20,
    SYS_NR_MKDIR = 39,
    SYS_NR_RMDIR = 40,
    SYS_NR_BRK = 45,
    SYS_NR_UMASK = 60,
    SYS_NR_GETPPID = 64,
    SYS_NR_YIELD = 158,
    SYS_NR_SLEEP = 162,
} syscall_t;

u32 test();

pid_t fork();
void exit(int status);

pid_t waitpid(pid_t pid, int32* status);

void yield();

void sleep(u32 ms);

int32 brk(void* addr);

int32 write(fd_t fd, char* buf, u32 i);

pid_t getpid();
pid_t getppid();

time_t time();

mode_t umask(mode_t mask);

int mkdir(char* pathname, int mode);
int rmdir(char* pathname);

int link(char* oldname, char* newname);
int unlink(char* filename);

#endif