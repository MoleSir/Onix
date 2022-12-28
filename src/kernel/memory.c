#include <onix/memory.h>
#include <onix/types.h>
#include <onix/debug.h>
#include <onix/assert.h>
#include <onix/onix.h>
#include <stdlib.h>
#include <string.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define ZONE_VALID 1    // ards 可用区域 
#define ZONE_RESERVED 2 // ards 不可用区域

// 获取 addr 的页索引，一页大小为 0x1000，所以每页最底 12 位是 0，右移 12 位得到第几个页
#define IDX(addr) ((u32)addr >> 12) 
// 由页面索引，得到页面起始地址
#define PAGE(idx) ((u32)idx << 12)
#define ASSERT_PAGE(addr) assert((addr & 0xfff) == 0)

typedef struct ards_t
{
    u64 base;   // 内存基地址
    u64 size;   // 内存长度
    u32 type;   // 类型
} _packed ards_t;

static u32 memory_base = 0;    // 可用内存基地址，等于 1M
static u32 memory_size = 0;    // 可用内存大小
static u32 total_pages = 0;    // 所有内存页面数量
static u32 free_pages = 0;     // 空闲内存页数量

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

static u32 start_page = 0;  // 可分配物理内存起始位置
static u8* memory_map;     // 物理内存数组，每个字节来管理一个物理页
static u32 memory_map_pages;// 物理内存数组占用的页数量

memory_map_init()
{
    // 初始化物理内存数组，以字节为单位
    memory_map = (u8*)memory_base;

    // 计算物理内存数组占用的页面数，即管理物理也的数据结构 memory_map 需要占据的页面数量
    memory_map_pages = div_round_up(total_pages, PAGE_SIZE);
    LOGK("Memory map page count %d\n", memory_map_pages);

    // 空闲页减少，因为用作 memory_map 使用
    free_pages -= memory_map_pages;

    // 清空物理内存数组
    memset((void*)memory_map, 0, memory_map_pages * PAGE_SIZE);

    // 目前使用的所有页面为：MEMORY_BASE 的页面下标 + memory_map 所占用的内存页面数量
    // MEMORY_BASE 之前的内存用作 gdt 与 idt 等结构
    start_page = IDX(MEMORY_BASE) + memory_map_pages;
    // 这些页面都被使用了一次，所以对应的 memory_map 都为 1
    for (size_t i = 0; i < start_page; ++i)
        memory_map[i] = 1;

    LOGK("Total pages %d free pages %d\n", total_pages, free_pages);
}

// 分配一页物理内存
static u32 get_page()
{
    // 从 start_page 的位置开始找，因为之前的都被使用了
    for (size_t i = start_page; i < total_pages; i++)
    {
        // 如果还存在空闲页
        if (!memory_map[i])
        {
            memory_map[i] = 1;
            free_pages--;
            // 根据索引得到页面起始地址
            u32 page = PAGE(i);
            LOGK("GET page 0x%p\n", page);
            return page;
        }
    }

    // 没有空闲内存
    panic("Out of Memory!!!");
}

// 释放一页物理内存
static void put_page(u32 addr)
{
    ASSERT_PAGE(addr);

    u32 idx = IDX(addr);

    // 不可以释放 start_page 之前的页面，也不可以超过总页面数量
    assert(idx >= start_page && idx < total_pages);

    // 空闲页不能释放
    assert(memory_map[idx] >= 1);
    memory_map[idx]--;

    // 如果释放后引用为 0，增加一个空闲页面
    if (!memory_map[idx])
        free_pages++;
    
    assert(free_pages < total_pages);
    LOGK("PUT page 0x%p\n", addr);
}

void memory_test()
{
    u32 pages[10];
    for (size_t i = 0; i < 10; ++i)
        pages[i] = get_page();

    for (size_t i = 0; i < 10; ++i)
        put_page(pages[i]);
}
