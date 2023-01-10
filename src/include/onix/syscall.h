#ifndef __ONIX_SYSCALL_HH__
#define __ONIX_SYSCALL_HH__

#include <onix/types.h>
#include <onix/stat.h>

typedef enum syscall_t
{
    SYS_NR_TEST = 0,
    SYS_NR_EXIT = 1,
    SYS_NR_FORK = 2,
    SYS_NR_READ = 3,
    SYS_NR_WRITE = 4,
    SYS_NR_OPEN = 5,
    SYS_NR_CLOSE = 6,
    SYS_NR_WAITPID = 7,
    SYS_NR_CREATE = 8,
    SYS_NR_LINK = 9,
    SYS_NR_UNLINK = 10,
    SYS_NR_CHDIR = 12,
    SYS_NR_TIME = 13,
    SYS_NR_MKNOD = 14,
    SYS_NR_STAT = 18,
    SYS_NR_LSEEK = 19,
    SYS_NR_GETPID = 20,
    SYS_NR_FSTAT = 28,
    SYS_NR_MKDIR = 39,
    SYS_NR_RMDIR = 40,
    SYS_NR_BRK = 45,
    SYS_NR_UMASK = 60,
    SYS_NR_CHROOT = 61,
    SYS_NR_GETPPID = 64,
    SYS_NR_READDIR = 89,
    SYS_NR_YIELD = 158,
    SYS_NR_SLEEP = 162,
    SYS_NR_GETCWD = 183,
    SYS_NR_CLEAR = 200,
} syscall_t;

u32 test();

pid_t fork();
void exit(int status);

pid_t waitpid(pid_t pid, int32* status);

void yield();

void sleep(u32 ms);

int32 brk(void* addr);

int32 write(fd_t fd, char* buf, u32 len);
int32 read(fd_t fd, char* buf, u32 len);

pid_t getpid();
pid_t getppid();

time_t time();

mode_t umask(mode_t mask);

int mkdir(char* pathname, int mode);
int rmdir(char* pathname);
int lseek(fd_t fd, off_t offset, int whence);
int readdir(fd_t fd, void* dir, u32 count);

int link(char* oldname, char* newname);
int unlink(char* filename);

fd_t open(char* filename, int flags, int mode);
fd_t create(char* filename, int mode);
void close(fd_t fd);

char* getcwd(char *buf, size_t size);
int chdir(char *pathname);
int chroot(char *pathname);

void clear();

int stat(char *filename, stat_t *statbuf);
int fstat(fd_t fd, stat_t *statbuf);

int mknod(char* filename, int mode, int dev);

#endif