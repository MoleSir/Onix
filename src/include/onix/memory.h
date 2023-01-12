#ifndef __ONIX_MEMORY_HH__
#define __ONIX_MEMORY_HH__

#include <onix/types.h>

#define PAGE_SIZE 0x1000     // 一页的大小 4K
#define MEMORY_BASE 0x100000 // 1M，可用内存开始的位置

// 内核内存空间，16M
#define KERNEL_MEMORY_SIZE 0x1000000

// 内核缓存地址
#define KERNEL_BUFFER_MEM 0x800000

// 内核缓存大小
#define KERNEL_BUFFER_SIZE 0x400000

// 内存虚拟磁盘地址
#define KERNEL_RAMDISK_MEM (KERNEL_BUFFER_MEM + KERNEL_BUFFER_SIZE)

// 内存虚拟磁盘大小
#define KERNEL_RAMDISK_SIZE 0x400000

// 用户程序地址
#define USER_EXEC_ADDR KERNEL_MEMORY_SIZE

// 内存映射开始位置 128M
#define USER_MMAP_ADDR 0x8000000

// 用户映射内存大小 128M
#define USER_MMAP_SIZE 0x8000000

// 用户栈顶 256M
#define USER_STACK_TOP 0x10000000

// 用户栈最大 2M
#define USER_STACK_SIZE 0x200000

// 用户栈低地址
#define USER_STACK_BUTTOM (USER_STACK_TOP - USER_STACK_SIZE)

// 内核页目录索引
#define KERNEL_PAGE_DIR 0x1000

typedef struct page_entry_t
{
    u8 present : 1;  // 在内存中
    u8 write : 1;    // 0 只读 1 可读可写
    u8 user : 1;     // 1 所有人 0 超级用户 DPL < 3
    u8 pwt : 1;      // page write through 1 直写模式，0 回写模式
    u8 pcd : 1;      // page cache disable 禁止该页缓冲
    u8 accessed : 1; // 被访问过，用于统计使用频率
    u8 dirty : 1;    // 脏页，表示该页缓冲被写过
    u8 pat : 1;      // page attribute table 页大小 4K/4M
    u8 global : 1;   // 全局，所有进程都用到了，该页不刷新缓冲
    u8 shared : 1;   // 共享内存页，与 CPU 无关
    u8 privat : 1;   // 私有内存页，与 CPU 无关
    u8 flag : 1;     // 送给操作系统，不需要
    u32 index : 20;  // 页索引
} _packed page_entry_t;

// 获取 cr2 寄存器
u32 get_cr2();

// 获取 cr3 寄存器
u32 get_cr3();

// 设置 cr3 寄存器
void set_cr3(u32 pde);

// 分配 count 个连续的内核页
u32 alloc_kpage(u32 page);

// 释放 count 个连续的内核页
void free_kpage(u32 vaddr, u32 count);

// 将 vaddr 映射物理内存
void link_page(u32 vaddr);

// 去掉 vaddr 对应物理内存映射
void unlink_page(u32 vaddr);

// 拷贝页目录
page_entry_t* copy_pde();

// 释放页目录
void free_pde();

// 获取页表项目
page_entry_t* get_entry(u32 vaddr, bool create);

// 刷新快表
void flush_tlb(u32 vaddr);

#endif