#ifndef __ONIX_CONSOLE_HH__
#define __ONIX_CONSOLE_HH__

#include <onix/types.h>

void console_init();
void console_clear();
void console_write(char* buf, u32 count);

#endif