#ifdef ONIX
#include <onix/types.h>
#include <stdio.h>
#include <onix/syscall.h>
#include <string.h>
#include <onix/fs.h>
#else
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#endif

#define BUFLEN 1024

char buf[BUFLEN];

int main(int argc, char const *argv[])
{
    // 检查参数
    if (argc < 2)
        return EOF;

    // 打开文件
    int fd = open((char*)argv[1], O_RDONLY, 0);
    if (fd == EOF)
    {
        printf("file %s not exits\n", argv[1]);
        return EOF;
    }

    while (1)
    {
        // 读取文件
        int len = read(fd, buf, BUFLEN);
        if (len == EOF)
            break;
        // 写文件，写到 1 号文件
        write(1, buf, len);
    }
    // 关闭文件
    close(fd);
    return 0;
}