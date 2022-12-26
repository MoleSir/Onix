#ifndef __ONIX_DEBUG_HH__
#define __ONIX_DEBUG_HH__

void debugk(char* file, int line, const char* fmt, ...);

#define BMB asm volatile("xchgw %bx, %bx")
#define DEBUGK(fmt, args...) debugk(__BASE_FILE__, __LINE__, fmt, ##args)

#endif