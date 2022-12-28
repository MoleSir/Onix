#include <onix/memory.h>
#include <onix/types.h>
#include <onix/debug.h>
#include <onix/assert.h>
#include <onix/onix.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define ZONE_VALID 1    // ards 可用区域 
#define ZONE_RESERVED 2 // ards 不可用区域

// 获取 addr 的页索引，一页大小为 0x1000，所以每页最底 12 位是 0，右移 12 位得到第几个页
#define IDX(addr) ((u32)addr >> 12) 

typedef struct ards_t
{
    u64 base;   // 内存基地址
    u64 size;   // 内存长度
    u32 type;   // 类型
} _packed ards_t;

u32 memory_base = 0;    // 可用内存基地址，等于 1M
u32 memory_size = 0;    // 可用内存大小
u32 total_pages = 0;    // 所有内存页面数量
u32 free_pages = 0;     // 空闲内存页数量

#define used_pages (total_pages - free_pages);  // 已使用的页数

void memory_init(u32 magic, u32 addr)
{
    u32 count;
    ards_t* ptr;

    // 如果是 onix loader 进入
    if (magic != ONIX_MAGIC)
        panic("Memory init magic unknow 0x%p\n", magic);

    count = *((u32*)addr);
    // loader.asm 中，定义的，ards_count 下面的内存就是保存 ards 数组的位置
    ptr = (ards_t*)(addr + 4);

    for (size_t i = 0; i < count; ++i, ++ptr)
    {
        LOGK("Memeory base 0x%p\n", (u32)(ptr->base));
        LOGK("Memory size 0x%p\n", (u32)(ptr->size));
        LOGK("Memory type 0x%p\n", (u32)(ptr->type));
        if (ptr->type == ZONE_VALID && ptr->size > memory_size)
        {
            memory_base = (u32)(ptr->base);
            memory_size = (u32)(ptr->size);
        }
    }

    LOGK("ARDS count %d\n", count);
    LOGK("Memeory base 0x%p\n", (u32)memory_base);
    LOGK("Memory size 0x%p\n", (u32)memory_size);

    assert(memory_base == MEMORY_BASE);
    assert((memory_size & 0xff) == 0);

    total_pages = IDX(memory_size) + IDX(memory_base);
    free_pages = IDX(memory_size);

    LOGK("Total pages %d\n", total_pages);
    LOGK("Free pages %d\n", free_pages);
}