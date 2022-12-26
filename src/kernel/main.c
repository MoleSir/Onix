#include <onix/onix.h>
#include <onix/types.h>
#include <onix/io.h>
#include <onix/string.h>
#include <onix/console.h>
#include <onix/printk.h>
#include <onix/assert.h>

void kernel_init()
{
    console_init();
    assert(5 > 3);
    //assert(3 > 4);
    panic("....");
    return;
}