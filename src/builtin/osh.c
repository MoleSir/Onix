#include <stdio.h>
#include <onix/syscall.h>
#include <string.h>
#include <stdlib.h>
#include <onix/assert.h>
#include <onix/fs.h>
#include <onix/stat.h>
#include <onix/time.h>

#define MAX_CMD_LEN 256
#define MAX_ARG_NR 16
#define MAX_PATH_LEN 1024
#define BUFLEN 1024

static char cwd[MAX_PATH_LEN];
static char cmd[MAX_CMD_LEN];
static char *args[MAX_ARG_NR];
static char buf[BUFLEN];

static char *onix_logo[] = {
    "                                ____       _      \n\0",
    "                               / __ \\___  (_)_ __ \n\0",
    "                              / /_/ / _ \\/ /\\ \\ / \n\0",
    "                              \\____/_//_/_//_\\_\\  \n\0",
};

extern char *strsep(const char *str);
extern char *strrsep(const char *str);

static void strftime(time_t stamp, char *buf)
{
    tm time;
    localtime(stamp, &time);
    sprintf(buf, "%d-%02d-%02d %02d:%02d:%02d",
            time.tm_year + 1900,
            time.tm_mon,
            time.tm_mday,
            time.tm_hour,
            time.tm_min,
            time.tm_sec);
}

void builtin_logo()
{
    clear();
    for (size_t i = 0; i < 4; i++)
    {
        printf(onix_logo[i]);
    }
}

void builtin_test(int argc, char *argv[])
{
    u32 status;

    int* counter = (int*)mmap(0, sizeof(int), PROT_WRITE, MAP_SHARED, EOF, 0);
    pid_t pid = fork();

    if (pid)
    {
        while (true)
        {
            (*counter)++;
            sleep(300);
        }
    }
    else
    {
        while (true)
        {
            printf("counter %d\n", *counter);
            sleep(100);
        }
    }
}

void builtin_pwd()
{
    getcwd(cwd, MAX_PATH_LEN);
    printf("%s\n", cwd);
}

void builtin_clear()
{
    clear();
}

static void parsemode(int mode, char *buf)
{
    memset(buf, '-', 10);
    buf[10] = '\0';
    char *ptr = buf;

    switch (mode & IFMT)
    {
    case IFREG:
        *ptr = '-';
        break;
    case IFBLK:
        *ptr = 'b';
        break;
    case IFDIR:
        *ptr = 'd';
        break;
    case IFCHR:
        *ptr = 'c';
        break;
    case IFIFO:
        *ptr = 'p';
        break;
    case IFLNK:
        *ptr = 'l';
        break;
    case IFSOCK:
        *ptr = 's';
        break;
    default:
        *ptr = '?';
        break;
    }
    ptr++;

    for (int i = 6; i >= 0; i -= 3)
    {
        int fmt = (mode >> i) & 07;
        if (fmt & 0b100)
        {
            *ptr = 'r';
        }
        ptr++;
        if (fmt & 0b010)
        {
            *ptr = 'w';
        }
        ptr++;
        if (fmt & 0b001)
        {
            *ptr = 'x';
        }
        ptr++;
    }
}

void builtin_ls(int argc, char* argv[])
{
    fd_t fd = open(cwd, O_RDONLY, 0);
    if (fd == EOF)
        return;
    bool list = false;

    // list -l
    if (argc == 2 && !strcmp(argv[1], "-l"))
        list = true;
    
    lseek(fd, 0, SEEK_SET);
    dentry_t entry;
    while (true)
    {
        int len = readdir(fd, &entry, 1);
        if (len == EOF)
            break;
        if (!entry.nr)
            continue;
        if (!strcmp(entry.name, ".") || !strcmp(entry.name, ".."))
            continue;
        if (!list)
        {
            printf("%s ", entry.name);
            continue;
        }

        stat_t statbuf;
        stat(entry.name, &statbuf);

        parsemode(statbuf.mode, buf);
        printf("%s ", buf);

        strftime(statbuf.mtime, buf);
        printf("% 2d % 2d % 2d % 2d %s %s\n", 
            statbuf.nlinks,
            statbuf.uid,
            statbuf.gid,
            statbuf.size,
            buf,
            entry.name);
    }
    if (!list)
        printf("\n");
    close(fd);
}

void builtin_cd(int argc, char *argv[])
{
    chdir(argv[1]);
}

void builtin_cat(int argc, char* argv[])
{
    fd_t fd = open(argv[1], O_RDONLY, 0);
    if (fd == EOF)
    {
        printf("file %s not exists.\n", argv[1]);
        return;
    }

    while (true)
    {
        int len = read(fd, buf, 1);
        if (len == EOF)
            break;
        write(STDOUT_FILENO, buf, len);
    }
    close(fd);
}

void builtin_mkdir(int argc, char *argv[])
{
    if (argc < 2)
    {
        return;
    }
    mkdir(argv[1], 0755);
}

void builtin_rmdir(int argc, char *argv[])
{
    if (argc < 2)
    {
        return;
    }
    rmdir(argv[1]);
}

void builtin_rm(int argc, char *argv[])
{
    if (argc < 2)
    {
        return;
    }
    unlink(argv[1]);
}

void builtin_date(int argc, char* argv[])
{
    strftime(time(), buf);
    printf("%s\n", buf);
}

void builtin_mount(int argc, char* argv[])
{
    if (argc < 3)
        return;
    mount(argv[1], argv[2], 0);
}

void builtin_umount(int argc, char* argv[])
{
    if (argc < 2)
        return;
    umount(argv[1]);
}

void builtin_mkfs(int argc, char* argv[])
{
    if (argc < 2)
        return;
    mkfs(argv[1], 0);
}

// 得到 name 的最后一级目录
char *basename(char *name)
{
    char *ptr = strrsep(name);
    if (!ptr[1])
    {
        return ptr;
    }
    ptr++;
    return ptr;
}

// 打印提示符
void print_prompt()
{
    getcwd(cwd, MAX_PATH_LEN);
    char *ptr = strrsep(cwd);
    if (ptr != cwd)
    {
        *ptr = 0;
    }
    char *base = basename(cwd);
    printf("[root %s]# ", base);
}

// 读取一行命令
void readline(char *buf, u32 count)
{
    assert(buf != NULL);
    char *ptr = buf;
    u32 idx = 0;
    char ch = 0;

    while (idx < count)
    {
        ptr = buf + idx;
        int ret = read(STDIN_FILENO, ptr, 1);
        if (ret == -1)
        {
            *ptr = 0;
            return;
        }

        switch (*ptr)
        {
        // 命令结束标志
        case '\n':
        case '\r':
            *ptr = 0;
            ch = '\n';
            write(STDOUT_FILENO, &ch, 1);
            return;
        case '\b':
            if (buf[0] != '\b')
            {
                idx--;
                ch = '\b';
                write(STDOUT_FILENO, &ch, 1);
            }
            break;
        case '\t':
            continue;
        default:
            // 普通字符，键盘输入一个，显示器输出一个
            write(STDOUT_FILENO, ptr, 1);
            idx++;
            break;
        }
    }

    // 字符串结束
    buf[idx] = '\0';
}

// 解析命令，将字符串 cmd 以空格分割为若干字符串，放入 argv 中
static int cmd_parse(char *cmd, char *argv[])
{
    assert(cmd != NULL);

    char *next = cmd;
    int argc = 0;
    int quot = false;
    while (*next && argc < MAX_ARG_NR)
    {
        while (*next == ' ' || (quot && *next != '"'))
        {
            next++;
        }
        if (*next == 0)
        {
            break;
        }

        if (*next == '"')
        {
            quot = !quot;

            if (quot)
            {
                next++;
                argv[argc++] = next;
            }
            else
            {
                *next = 0;
                next++;
            }
            continue;
        }

        argv[argc++] = next;
        while (*next && *next != ' ')
        {
            next++;
        }

        if (*next)
        {
            *next = 0;
            next++;
        }
    }

    argv[argc] = NULL;
    return argc;
}

static void execute(int argc, char *argv[])
{
    char *line = argv[0];
    if (!strcmp(line, "test"))
    {
        return builtin_test(argc, argv);
    }
    if (!strcmp(line, "logo"))
    {
        return builtin_logo();
    }
    if (!strcmp(line, "pwd"))
    {
        return builtin_pwd();
    }
    if (!strcmp(line, "clear"))
    {
        return builtin_clear();
    }
    if (!strcmp(line, "exit"))
    {
        int code = 0;
        if (argc == 2)
        {
            code = atoi(argv[1]);
        }
        exit(code);
    }
    if (!strcmp(line, "ls"))
    {
        return builtin_ls(argc, argv);
    }
    if (!strcmp(line, "cd"))
    {
        return builtin_cd(argc, argv);
    }
    if (!strcmp(line, "cat"))
    {
        return builtin_cat(argc, argv);
    }
    if (!strcmp(line, "mkdir"))
    {
        return builtin_mkdir(argc, argv);
    }
    if (!strcmp(line, "rmdir"))
    {
        return builtin_rmdir(argc, argv);
    }
    if (!strcmp(line, "rm"))
    {
        return builtin_rm(argc, argv);
    }
    if (!strcmp(line, "mount"))
    {
        return builtin_mount(argc, argv);
    }
    if (!strcmp(line, "umount"))
    {
        return builtin_umount(argc, argv);
    }
    if (!strcmp(line, "mkfs"))
    {
        return builtin_mkfs(argc, argv);
    }
}

int osh_main()
{
    // 缓冲清零
    memset(cmd, 0, sizeof(cmd));
    memset(cwd, 0, sizeof(cwd));

    // 打印 logo
    builtin_logo();

    while (true)
    {
        // 打印提示符
        print_prompt();

        // 读取一行命令
        readline(cmd, sizeof(cmd));

        // 命令没有，继续读取
        if (cmd[0] == 0)
        {
            continue;
        }
    
        // 解析命令
        int argc = cmd_parse(cmd, args);
        
        // 判断解析参数数量
        if (argc < 0 || argc >= MAX_ARG_NR)
        {
            continue;
        }
        
        // 执行命令
        execute(argc, args);
    }
    return 0;
}