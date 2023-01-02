#ifndef __ONIX_STDIO_HH__
#define __ONIX_STDIO_HH__

#include <stdarg.h>

int vsprintf(char* buf, const char* fmt, va_list args);
int sprintf(char* buf, const char* fmt, ...);
int printf(const char* fmt, ...);

#endif