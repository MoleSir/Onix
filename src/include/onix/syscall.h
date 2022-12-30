#ifndef __ONIX_SYSCALL_HH__
#define __ONIX_SYSCALL_HH__

#include <onix/types.h>

typedef enum syscall_t
{
    SYS_NR_TEST = 0,
    SYS_NR_SLEEP,
    SYS_NR_YIELD,
} syscall_t;

u32 test();
void yield();
void sleep(u32 ms);

#endif