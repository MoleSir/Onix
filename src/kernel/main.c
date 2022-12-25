#include <onix/onix.h>

int magic = ONIX_MAGIC;
char message[] = "hello onix!!!";
char buf[1024]; //.bss

void kernel_init()
{
    // 文本显示器的内存位置
    char* video = (char*) 0xb8000;  
    for (int i = 0; i < sizeof(message); ++i)
    {
        video[i << 1] = message[i];
    }
}