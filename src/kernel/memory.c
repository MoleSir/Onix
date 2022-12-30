#include <onix/memory.h>
#include <onix/types.h>
#include <onix/debug.h>
#include <onix/assert.h>
#include <onix/onix.h>
#include <stdlib.h>
#include <string.h>
#include <ds/bitmap.h>
#include <onix/multiboot2.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define ZONE_VALID 1    // ards 可用区域 
#define ZONE_RESERVED 2 // ards 不可用区域

// 获取 addr 的页索引，一页大小为 0x1000，所以每页最底 12 位是 0，右移 12 位得到第几个页
#define IDX(addr) ((u32)addr >> 12) 
// 获取 addr 的页目录索引
#define DIDX(addr) (((u32)addr >> 22) & 0x3ff)
// 获取 addr 的页表索引
#define TIDX(addr) (((u32)addr >> 12) & 0x3ff)
// 由页面索引，得到页面起始地址
#define PAGE(idx) ((u32)idx << 12)
#define ASSERT_PAGE(addr) assert((addr & 0xfff) == 0)

#define KERNEL_MAP_BITS 0x4000

// 内核内存大小
#define KERNEL_MEMORY_SIZE (0x100000 * sizeof(KERNEL_PAGE_TABLE))

bitmap_t kernel_map;

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

// 内存初始化
void memory_init(u32 magic, u32 addr)
{
    u32 count = 0;

    // 如果是 onix loader 进入的内核
    if (magic == ONIX_MAGIC)
    {
        count = *(u32 *)addr;
        ards_t *ptr = (ards_t *)(addr + 4);

        for (size_t i = 0; i < count; i++, ptr++)
        {
            LOGK("Memory base 0x%p size 0x%p type %d\n",
                 (u32)ptr->base, (u32)ptr->size, (u32)ptr->type);
            if (ptr->type == ZONE_VALID && ptr->size > memory_size)
            {
                memory_base = (u32)ptr->base;
                memory_size = (u32)ptr->size;
            }
        }
    }
    // 如果是 multiboot2 进入的内核
    else if (magic == MULTIBOOT2_MAGIC)
    {
        u32 size = *(unsigned int *)addr;
        multi_tag_t *tag = (multi_tag_t *)(addr + 8);

        LOGK("Announced mbi size 0x%x\n", size);
        while (tag->type != MULTIBOOT_TAG_TYPE_END)
        {
            if (tag->type == MULTIBOOT_TAG_TYPE_MMAP)
                break;
            // 下一个 tag 对齐到了 8 字节
            tag = (multi_tag_t *)((u32)tag + ((tag->size + 7) & ~7));
        }

        multi_tag_mmap_t *mtag = (multi_tag_mmap_t *)tag;
        multi_mmap_entry_t *entry = mtag->entries;
        while ((u32)entry < (u32)tag + tag->size)
        {
            LOGK("Memory base 0x%p size 0x%p type %d\n",
                 (u32)entry->addr, (u32)entry->len, (u32)entry->type);
            count++;
            if (entry->type == ZONE_VALID && entry->len > memory_size)
            {
                memory_base = (u32)entry->addr;
                memory_size = (u32)entry->len;
            }
            entry = (multi_mmap_entry_t *)((u32)entry + mtag->entry_size);
        }
    }
    else
    {
        panic("Memory init magic unknown 0x%p\n", magic);
    }

    LOGK("ARDS count %d\n", count);
    LOGK("Memory base 0x%p\n", (u32)memory_base);
    LOGK("Memory size 0x%p\n", (u32)memory_size);

    assert(memory_base == MEMORY_BASE); // 内存开始的位置为 1M
    assert((memory_size & 0xfff) == 0); // 要求按页对齐

    total_pages = IDX(memory_size) + IDX(MEMORY_BASE);
    free_pages = IDX(memory_size);

    LOGK("Total pages %d\n", total_pages);
    LOGK("Free pages %d\n", free_pages);

    if (memory_size < KERNEL_MEMORY_SIZE)
    {
        panic("System memory is %dM too small, at least %dM needed\n",
              memory_size / MEMORY_BASE, KERNEL_MEMORY_SIZE / MEMORY_BASE);
    }
}

static u32 start_page = 0;  // 可分配物理内存起始位置
static u8* memory_map;     // 物理内存数组，每个字节来管理一个物理页
static u32 memory_map_pages;// 物理内存数组占用的页数量

// mmap 初始化
void memory_map_init()
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

    // 初始化内核虚拟内存位图，需要 8 位对齐
    u32 length = (IDX(KERNEL_MEMORY_SIZE) - IDX(MEMORY_BASE)) / 8;
    bitmap_init(&kernel_map, (u8*)KERNEL_MAP_BITS, length, IDX(MEMORY_BASE));
    bitmap_scan(&kernel_map, memory_map_pages);
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

// 获取 cr3 寄存器
u32 get_cr3()
{
    asm volatile("movl %cr3, %eax\n");
}

// 设置 cr3 寄存器
void set_cr3(u32 pde)
{
    ASSERT_PAGE(pde);
    asm volatile("movl %%eax, %%cr3\n" ::"a"(pde));
}

// 将 cr00 寄存器最高位 PE 设置位 1，启动分页
static _inline void enable_page()
{
    // 0b1000_0000_0000_0000_0000_0000_0000_0000
    asm volatile(
        "movl %cr0, %eax\n"
        "orl $0x80000000, %eax\n"
        "movl %eax, %cr0\n");
}

// 初始化页表项
static void entry_init(page_entry_t* entry, u32 index)
{
    // 首先全部清零
    *(u32*)entry = 0;
    entry->present = 1;
    entry->write = 1;
    entry->user = 1;
    // 这个页表项指向物理内存的 index 号页面
    entry->index = index;
}

// 内存映射初始化
void mapping_init()
{
    // 将 KERNEL_PAGE_DIR 的位置视为 page_entry_t，并且设置为 0
    // PS: pde -> page diretory entry，表示页目录项，所以 KERNEL_PAGE_DIR 所在的这一页就作为系统的页目录
    page_entry_t* pde = (page_entry_t*)KERNEL_PAGE_DIR;
    memset(pde, 0, PAGE_SIZE);

    idx_t index = 0;

    // 对内核的每一个页面
    for (idx_t didx = 0; didx < (sizeof(KERNEL_PAGE_TABLE) / 4); didx++)
    {
        // 得到页面起始地址，并且清空页面
        page_entry_t* pte = (page_entry_t*)(KERNEL_PAGE_TABLE[didx]);
        memset(pte, 0, PAGE_SIZE);

        // 设置页目录项目
        page_entry_t* dentry = &pde[didx];
        entry_init(dentry, IDX((u32)pte));

        // 设置页面的 1024 个表项目
        // 每次设置一个页目录都让 index + 1，使得所有页面顺序映射内存的起始部分内存
        for (size_t tidx = 0; tidx < 1024; tidx++, index++)
        {
            // 第 0 也不映射，为了造成空指针访问，缺页异常，排查错误
            if (index == 0) 
                continue;
            
            // 对每一个页目录设置，指向内存的第 index 个页面
            page_entry_t* tentry = &pte[tidx];
            entry_init(tentry, index);
            memory_map[index] = 1;
        }
    }

    // 页目录的最后一个页目录项，指向页目录所在的页面，这样就可用在开启分页后修改页目录与页表
    // 这样设置之后，逻辑地址的 0xfffff000 - 0xffffffff 这个部分，连续找到两次组后一表项，还得得到页目录地址 0x1000
    // 再加上偏置，0xfffff000 - 0xffffffff 被映射到 0x1000 - 0x1fff
    // 又对 0xffc00000 - 0xffc01fff，首先还是找到页目录表的最后一项，还是页目录，再找其中前两个表项（也即前两个页表）
    // 的所有项目，即找到了两个页表的物理地址，得到了：
    // 0xffc00000 - 0xffc01fff 到 0x2000 - 0x3fff 的映射
    page_entry_t* entry = &pde[1023];
    entry_init(entry, IDX(KERNEL_PAGE_DIR));

    // 设置 cr3 
    set_cr3((u32)pde);

    // 开启分页有效
    enable_page();
}

// 通过页目录最后一个表项找到页面，通过页面的最后一个表项找到页面
// 因为 pde 最后一个表项指向本身，所以找了两次最后一项，最后得到 pde 地址；
// 所以开启分页后 0xffffff000 映射到 0x1000
static page_entry_t* get_pde()
{
    return (page_entry_t*)(0xfffff000);
}

// 根据 vaddr 最高 10 位作为索引，作为页目录的索引，得到 vaddr 地址对应的页目录项
static page_entry_t* get_pte(u32 vaddr)
{
    return (page_entry_t*)(0xffc00000 | (DIDX(vaddr) << 12));
}

// 刷新虚拟地址 vaddr 的块表
static void flush_tlb(u32 vaddr)
{
    asm volatile("invlpg (%0)" ::"r"(vaddr)
                 : "memory");
}

// 从位图中扫描 count 个连续的页
static u32 scan_page(bitmap_t *map, u32 count)
{
    assert(count > 0);
    int32 index = bitmap_scan(map, count);

    if (index == EOF)
    {
        panic("Scan page fail!!!");
    }

    u32 addr = PAGE(index);
    LOGK("Scan page 0x%p count %d\n", addr, count);
    return addr;
}

// 与 scan_page 相对，重置相应的页
static void reset_page(bitmap_t *map, u32 addr, u32 count)
{
    ASSERT_PAGE(addr);
    assert(count > 0);
    u32 index = IDX(addr);

    for (size_t i = 0; i < count; i++)
    {
        assert(bitmap_test(map, index + i));
        bitmap_set(map, index + i, 0);
    }
}

// 分配 count 个连续的内核页
u32 alloc_kpage(u32 count)
{
    assert(count > 0);
    u32 vaddr = scan_page(&kernel_map, count);
    LOGK("ALLOC kernel pages 0x%p count %d\n", vaddr, count);
    return vaddr;
}

// 释放 count 个连续的内核页
void free_kpage(u32 vaddr, u32 count)
{
    ASSERT_PAGE(vaddr);
    assert(count > 0);
    reset_page(&kernel_map, vaddr, count);
    LOGK("FREE  kernel pages 0x%p count %d\n", vaddr, count);
}
