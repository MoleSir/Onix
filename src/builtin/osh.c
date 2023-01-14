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

static char *envp[] = {
    "HOME=/",
    "PATH=/bin",
    NULL,
};

static char *onix_logo[] = {
    "                                ____       _      \n\0",
    "                               / __ \\___  (_)_ __ \n\0",
    "                              / /_/ / _ \\/ /\\ \\ / \n\0",
    "                              \\____/_//_/_//_\\_\\  \n\0",
};

extern char *strsep(const char *str);
extern char *strrsep(const char *str);

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

void builtin_cd(int argc, char *argv[])
{
    chdir(argv[1]);
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

void builtin_date(int argc, char *argv[])
{
    strftime(time(), buf);
    printf("%s\n", buf);
}

void builtin_mount(int argc, char *argv[])
{
    if (argc < 3)
    {
        return;
    }
    mount(argv[1], argv[2], 0);
}

void builtin_umount(int argc, char *argv[])
{
    if (argc < 2)
    {
        return;
    }
    umount(argv[1]);
}

void builtin_mkfs(int argc, char *argv[])
{
    if (argc < 2)
    {
        return;
    }
    mkfs(argv[1], 0);
}

static void dupfile(int argc, char** argv, fd_t dupfd[3])
{
    dupfd[0] = dupfd[1] = dupfd[2] = EOF;

    int outappend = 0;
    int errappend = 0;

    char* infile = NULL;
    char* outfile = NULL;
    char* errfile = NULL;

    for (size_t i = 0; i < argc; ++i)
    {
        // 如果当前的参数是 "<"，并且下一参数存在
        if (!strcmp(argv[i], "<") && (i + 1) < argc)
        {
            // 设置下一个参数为输入文件
            infile = argv[i + 1];
            // 命令的有效参数截止到此
            argv[i] = NULL;
            i++;
            continue; 
        }
        // 如果当前的参数是 ">"，并且下一参数存在
        if (!strcmp(argv[i], ">") && (i + 1) < argc)
        {
            // 设置下一个参数为输出文件
            outfile = argv[i + 1];
            argv[i] = NULL;
            i++;
            continue; 
        }
        // 如果当前的参数是 ">>"，并且下一参数存在
        if (!strcmp(argv[i], ">>") && (i + 1) < argc)
        {
            // 设置下一个参数为输出文件
            outfile = argv[i + 1];
            argv[i] = NULL;
            // 输出方式为追加
            outappend = O_APPEND;
            i++;
            continue; 
        }
        // 如果当前的参数是 "2>"，并且下一参数存在
        if (!strcmp(argv[i], ">") && (i + 1) < argc)
        {
            // 设置下一个参数为错误输出文件
            errfile = argv[i + 1];
            argv[i] = NULL;
            i++;
            continue; 
        }
        // 如果当前的参数是 "2>>"，并且下一参数存在
        if (!strcmp(argv[i], ">>") && (i + 1) < argc)
        {
            // 设置下一个参数为错误输出文件
            errfile = argv[i + 1];
            argv[i] = NULL;
            // 输出方式为追加
            errappend = O_APPEND;
            i++;
            continue;
        }
    }

    // 如果输入文件存在，就打开，并且 dupfd[0] 保存其文件描述符
    if (infile != NULL)
    {
        fd_t fd = open(infile, O_RDONLY | outappend | O_CREAT, 0755);
        if (fd == EOF)
        {
            printf("open file %s failure\n", infile);
            goto rollback;
        }
        dupfd[0] = fd;
    }
    // 如果输出文件存在，就打开，并且 dupfd[1] 保存其文件描述符
    if (outfile != NULL)
    {
        fd_t fd = open(outfile, O_WRONLY | outappend | O_CREAT, 0755);
        if (fd == EOF)
        {
            printf("open file %s failure\n", infile);
            goto rollback;
        }
        dupfd[1] = fd;
    }
    // 如果错误输出文件存在，就打开，并且 dupfd[2] 保存其文件描述符
    if (errfile != NULL)
    {
        fd_t fd = open(errfile, O_WRONLY | errappend | O_CREAT, 0755);
        if (fd == EOF)
        {
            printf("open file %s failure\n", infile);
            goto rollback;
        }
        dupfd[2] = fd;
    }
    return;

rollback:
    for (size_t i = 0; i < 3; ++i)
    {
        if (dupfd[i] != EOF)
            close(dupfd[i]);
    }
}

pid_t builtin_command(char *filename, char *argv[], fd_t infd, fd_t outfd, fd_t errfd)
{
    int status;
    pid_t pid = fork();
    if (pid)
    {
        // 对父进程，如果存在重定向，把重定向的文件关闭
        if (infd != EOF)
            close(infd);
        if (outfd != EOF)
            close(outfd);
        if (errfd != EOF)
            close(errfd);
        // 父进程返回到 exec，等待子进程结束
        return pid;
    }
    
    // 对子进程
    if (infd != EOF)
    {
        // 存在输入重定向，将 infd 设置到标准输入上
        fd_t fd = dup2(infd, STDIN_FILENO);
        close(infd);
    }
    if (outfd != EOF)
    {
        // 存在输出重定向，将 outfd 设置到标准输出上
        fd_t fd = dup2(outfd, STDOUT_FILENO);
        close(outfd);
    }
    if (errfd != EOF)
    {
        // 存在错误输出重定向，将 errfd 设置到标准错误输出上
        fd_t fd = dup2(errfd, STDERR_FILENO);
        close(errfd);
    }

    // 执行
    int i = execve(filename, argv, envp);
    exit(i);
}

void builtin_exec(int argc, char* argv[])
{
    stat_t statbuf;
    // 找到可执行文件名
    sprintf(buf, "bin/%s.out", argv[0]);
    if (stat(buf, &statbuf) == EOF)
    {
        printf("osh: commnad not found: %s!!!", argv[0]);
        return;
    }

    int status;
    fd_t dupfd[3];
    // 尝试根据命令行参数，对进程的文件描述符进行重定向，标准输入输出文件重定向的结果保存在 dupfd 中
    // 如果 dupfd 中保存 EOF，说明对应标准输入输出没有重定向，保持标准
    dupfile(argc, argv, dupfd);
    
    pid_t pid = builtin_command(buf, &argv[1], dupfd[0], dupfd[1], dupfd[2]);
    waitpid(pid, &status);
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
    if (!strcmp(line, "cd"))
    {
        return builtin_cd(argc, argv);
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
    if (!strcmp(line, "date"))
    {
        return builtin_date(argc, argv);
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
    return builtin_exec(argc, argv);
}

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
            write(STDOUT_FILENO, ptr, 1);
            idx++;
            break;
        }
    }
    buf[idx] = '\0';
}

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

int osh_main()
{
    memset(cmd, 0, sizeof(cmd));
    memset(cwd, 0, sizeof(cwd));

    builtin_logo();

    while (true)
    {
        print_prompt();
        readline(cmd, sizeof(cmd));
        if (cmd[0] == 0)
        {
            continue;
        }
        int argc = cmd_parse(cmd, args);
        if (argc < 0 || argc >= MAX_ARG_NR)
        {
            continue;
        }
        execute(argc, args);
    }
    return 0;
}