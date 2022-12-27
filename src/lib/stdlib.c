#include <stdlib.h>
#include <onix/types.h>

void delay(u32 count)
{
    while (count --);
}

void hang()
{
    while (true);
}
