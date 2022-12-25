#ifndef __IO_HH__
#define __IO_HH__

#include <onix/types.h>

// 输入一个字节
extern u8 inb(u16 port);
// 输入一个字
extern u8 inw(u16 port);

// 输出一个字节
extern void outb(u16 port, u8 value);
// 输出一个字
extern void outw(u16 port, u16 value);

#endif