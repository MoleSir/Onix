#include <onix/memory.h>
#include <onix/types.h>
#include <onix/debug.h>
#include <onix/assert.h>
#include <onix/onix.h>
#include <onix/task.h>
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

#define PDE_MASK 0xffc00000

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

// 获取 cr2 寄存器
u32 get_cr2()
{
    asm volatile("movl %cr2, %eax\n");
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
static page_entry_t* get_pde()
{
    return (page_entry_t*)(0xfffff000);
}

// 根据 vaddr 最高 10 位作为索引，作为页目录的索引，得到 vaddr 地址对应的页目录项
static page_entry_t* get_pte(u32 vaddr, bool create)
{
    // return (page_entry_t*)(0xffc00000 | (DIDX(vaddr) << 12));
    // 得到页目录与要找的页表在页目录的索引
    page_entry_t* pde = get_pde();
    u32 idx = DIDX(vaddr);
    // entry 可能不存在
    page_entry_t* entry = pde + idx;

    // 创建或（已经存在不创建）
    assert(create || (!create && entry->present));
    
    // 实际上 vaddr 应该对应的页表地址（一个页表起始地址可以有两个逻辑地址，因为页目录的最后一项是本身）
    page_entry_t* table = (page_entry_t*)(PDE_MASK | (idx << 12));

    // 如果页表不存在，需要创建
    if (!entry->present)
    {
        LOGK("Get and create page table entry ofr 0x%p\n", vaddr);
        // 申请一个页表
        u32 page = get_page();
        // 配置页目录项，指向 page 所在的页面
        entry_init(entry, IDX(page));
        memset(table, 0, PAGE_SIZE);
    }

    return table;
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

// 将 vaddr 映射物理内存
void link_page(u32 vaddr)
{
    ASSERT_PAGE(vaddr);

    // 得到一个页表，指明需要创建，如果存在那也不需要创建
    page_entry_t* pte = get_pte(vaddr, true);
    // 计算页表项地址 pte + 中间 10 位偏移
    page_entry_t* entry = pte + (TIDX(vaddr));

    task_t* task = running_task();
    bitmap_t* map = task->vmap;
    // 页面在位图中的索引
    u32 index = IDX(vaddr);

    // 如果页面存在，直接返回
    if (entry->present)
    {
        assert(bitmap_test(map, index));
        return;
    }

    assert(!bitmap_test(map, index));
    // 设置 bitmap 对应索引位置为真，表示在内存中有了真实的页面
    bitmap_set(map, index, true);

    // 找到一个空页
    u32 paddr = get_page();
    // 初始化页面的这个表项指向 paddr 的页面号
    entry_init(entry, IDX(paddr));
    flush_tlb(vaddr);

    LOGK("LINK from 0x%p to 0x%p", vaddr, paddr);
}

// 去掉 vaddr 对应物理内存映射
void unlink_page(u32 vaddr)
{
    ASSERT_PAGE(vaddr);

    // 得到页表与页表项
    page_entry_t* pte = get_pte(vaddr, false);
    page_entry_t* entry = pte + TIDX(vaddr);

    // 得到内存位图与逻辑地址的页目录号
    task_t* task = running_task();
    bitmap_t* map = task->vmap;
    u32 index = IDX(vaddr);

    // 如果本来就没有映射关系
    if (!entry->present)
    {
        assert(!bitmap_test(map, index));
        return;
    }
    assert(entry->present && bitmap_test(map, index));

    // 设置存在位为 0
    entry->present = false;
    bitmap_set(map, index, false);

    // 得到原来对应的物理地址
    u32 paddr = PAGE(entry->index);
    LOGK("UNLINK from 0x%p to 0x%p", vaddr, paddr);
    
    // 把这个页面释放掉
    put_page(paddr);

    flush_tlb(vaddr);
}

// 对 page 地址的一页拷贝一份，并且返回新页的物理地址，page 本身是虚拟地址
static u32 copy_page(void* page)
{
    // 申请一页，返回的是物理地址的页索引
    u32 paddr = get_page();

    // 现在不能直接访问 paddr 的物理地址，必须先给一个映射
    // 为了把 page 上的内容拷贝到 paddr 上，先把 paddr 映射到逻辑地址 0 处
    page_entry_t* entry = get_pte(0, false);
    entry_init(entry, IDX(paddr));
    
    // 再使用逻辑地址进行拷贝
    memcpy((void*)0, (void*)page, PAGE_SIZE);

    // 这个页面临时使用，其中的 Index 没有意义，所以这个存在位为 0
    entry->present = false;
    return paddr;
}

// 拷贝当前进程页目录
page_entry_t* copy_pde()
{
    // task 为父进程
    task_t* task = running_task();
    // pde 是逻辑地址，但前 8M 的内存就 vaddr = paddr，所以 pde 可直接返回给子进程使用
    page_entry_t* pde = (page_entry_t*)alloc_kpage(1);
    // 拷贝一份页目录
    memcpy(pde, (void*)task->pde, PAGE_SIZE);

    // 最后一项指向自己
    page_entry_t* entry = pde + 1023;
    entry_init(entry, IDX(pde));

    // 遍历页目录，拷贝已经存在的页表，遍历的是子进程的页目录，其中的内容跟父进程一致
    page_entry_t* dentry;
    // 0、1 是内核态占据的 8M 内存
    for (size_t didx = 2; didx < 1023; ++didx)
    {
        // 两个页目录，内容完全一致
        dentry = pde + didx;
        if (!dentry->present)
            continue;

        // 页目录项目存在指向的页表，pte 是页表逻辑地址（页表有两个逻辑地址！可用用 0xffc 开头连查两次页目录，或者直接前 8M paddr = vaddr）
        // 父、子进程，两个页目录都指向这个页表地址
        page_entry_t* pte = (page_entry_t*)(PDE_MASK | (didx << 12));

        // 遍历页表
        for (size_t tidx = 0; tidx < 1024; ++tidx)
        {
            entry = pte + tidx;
            if (!entry->present)
                continue;

            // 页表项存在指向的页面
            assert(memory_map[entry->index] > 0);

            // 设置为只读，写时拷贝
            entry->write = false;

            // 物理引用 + 1
            memory_map[entry->index]++;
            assert(memory_map[entry->index] < 255);
        }

        // 拷贝这个页表，把逻辑地址 pte 这个页拷贝一份到 paddr 这个物理地址
        u32 paddr = copy_page(pte);

        // 设置子进程的页目录项指向新的页表!!!
        dentry->index = IDX(paddr);
    }

    set_cr3(task->pde);

    return pde;
}

// 释放页目录
void free_pde()
{
    task_t* task = running_task();
    assert(task->uid != KERNEL_USER);

    page_entry_t* pde = get_pde();

    for (size_t didx = 2; didx < 1023; didx++)
    {
        page_entry_t* dentry = pde + didx;
        if (!dentry->present)
            continue;
        
        page_entry_t* pte = (page_entry_t*)(PDE_MASK | (didx << 12));
        for (size_t tidx = 0; tidx < 1024; tidx++)
        {
            page_entry_t* entry = pte + tidx;
            if (!entry->present)
                continue;
            
            // 释放页面
            assert(memory_map[entry->index] > 0);
            put_page(PAGE(entry->index));
        }

        // 释放页表
        put_page(PAGE(dentry->index));
    }

    // 释放页目录
    free_kpage(task->pde, 1);
    LOGK("free pages %d\n", free_pages);
}

// 缺页异常时，CPU 会自动压入错误码
typedef struct page_error_code_t
{
    u8 present : 1;
    u8 write : 1;
    u8 user : 1;
    u8 reserved0 : 1;
    u8 fetch : 1;
    u8 protection : 1;
    u8 shadow : 1;
    u8 reserved1 : 8;
    u8 sgx : 1;
    u16 reserved2;
} _packed page_error_code_t;

// page fault
void page_fault(
    int vector,
    u32 edi, u32 esi, u32 ebp, u32 esp,
    u32 ebx, u32 edx, u32 ecx, u32 eax,
    u32 gs, u32 fs, u32 es, u32 ds,
    u32 vector0, u32 error, u32 eip, u32 cs, u32 eflags)
{
    assert(vector == 0xe);
    u32 vaddr = get_cr2();
    LOGK("fault address 0x%p\n", vaddr);

    page_error_code_t* code = (page_entry_t*)(&error);
    task_t* task = running_task();
    
    assert(KERNEL_MEMORY_SIZE <= vaddr && vaddr < USER_STACK_TOP);

    // 错误码表示，这个地址存在物理页
    if (code->present)
    {
        assert(code->write);

        // 获取该虚拟地址对应的页表地址与页表表项
        page_entry_t* pte = get_pte(vaddr, false);
        page_entry_t* entry = pte + TIDX(vaddr);

        // 这个表项应该存在，页面有被使用
        assert(entry->present);
        assert(memory_map[entry->index] > 0);

        // 判断该页面的使用次数
        if (memory_map[entry->index] == 1)
        {   
            // 只被一个进程使用，那么直接把写允许打开
            entry->write = true;
            LOGK("WRITE page for 0x%p\n", vaddr);
        }
        else
        {
            // 多个进程同时拥有一个只读页面，其中一个进程想写
            // 获得虚拟地址对应的页面起始位置
            void* page = (void*)PAGE(IDX(vaddr));
            // 把这个页面拷贝一份，并且返回物理地址
            u32 paddr = copy_page(page);
            memory_map[entry->index]--;
            entry_init(entry, IDX(paddr));
            flush_tlb(vaddr);
            LOGK("COPY page for 0x%p\n", vaddr);
        }
        return;
    }

    if (!code->present && (vaddr < task->brk || vaddr >= USER_STACK_BUTTOM))
    {
        // 获得页面起始地址
        u32 page = PAGE(IDX(vaddr));
        // 申请一页内存映射
        link_page(page);
        // BMB;
        return;
    }

    panic("page fault!!!\n");
}

int32 sys_brk(void* addr)
{
    LOGK("task brk 0x%p\n", addr);
    u32 brk = (u32)addr;
    ASSERT_PAGE(brk);

    task_t* task = running_task();
    assert(task->uid != KERNEL_USER);

    assert(KERNEL_MEMORY_SIZE < brk < USER_STACK_BUTTOM);

    u32 old_brk = task->brk;

    if (old_brk > brk)
    {
        for (u32 page = brk; page < old_brk; page += PAGE_SIZE)
            unlink_page(page);
    }
    else if (IDX(brk - old_brk) > free_pages)
    {
        return -1;
    }
    
    task->brk = brk;
    return 0;
}