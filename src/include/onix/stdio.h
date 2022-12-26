#ifndef __ONIX_STDIO_HH__
#define __ONIX_STDIO_HH__

#include <onix/stdarg.h>

int vsprintf(char* buf, const char* fmt, va_list args);
int sprintf(char* buf, const char* fmt, ...);

#endif