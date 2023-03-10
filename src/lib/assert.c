#include <onix/assert.h>
#include <onix/types.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdio.h>

static u8 buf[1024];

// 强制阻塞
static void spin(char* name)
{
    printf("spinning in %s ...\n", name);
    while (true)
        ;
}


void assertion_failure(char* exp, char* file, char* base, int line)
{
    printf(
        "\n--> assert(%s) failed!!!\n"
        "--> file: %s \n"
        "--> base: %s \n"
        "--> line: %d \n",
        exp, file, base, line);

    spin("assertion_failure()");

    asm volatile("ud2");
}

void panic(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int i = vsprintf(buf, fmt, args);
    va_end(args);

    printf("!!! panic !!!\n%s \n", buf);
    spin("panic()");

    asm volatile("ud2");
}