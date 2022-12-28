#ifndef __ONIX_RTC_HH__
#define __ONIX_RTC_HH__

#include <onix/types.h>

void set_alarm(u32 secs);
u8 cmos_read(u8 addr);
void cmos_write(u8 addr, u8 value);

#endif