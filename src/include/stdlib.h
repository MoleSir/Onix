#ifndef __ONIX_STDLIB_HH__
#define __ONIX_STDLIB_HH__

#include <onix/types.h>

void delay(u32 count);
void hang();

u8 bcd_to_bin(u8 value);
u8 bin_to_bcd(u8 value);

#endif