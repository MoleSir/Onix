#ifndef __ONIX_GLOBAL_HH__
#define __ONIX_GLOBAL_HH__

#include <onix/types.h>

#define GDT_SIZE 128

// 全局描述符
typedef struct descriptor_t /* 共 8 个字节 */
{
    unsigned short limit_low;      // 段界限 0 ~ 15 位
    unsigned int base_low : 24;    // 基地址 0 ~ 23 位 16M
    unsigned char type : 4;        // 段类型
    unsigned char segment : 1;     // 1 表示代码段或数据段，0 表示系统段
    unsigned char DPL : 2;         // Descriptor Privilege Level 描述符特权等级 0 ~ 3
    unsigned char present : 1;     // 存在位，1 在内存中，0 在磁盘上
    unsigned char limit_high : 4;  // 段界限 16 ~ 19;
    unsigned char available : 1;   // 该安排的都安排了，送给操作系统吧
    unsigned char long_mode : 1;   // 64 位扩展标志
    unsigned char big : 1;         // 32 位 还是 16 位;
    unsigned char granularity : 1; // 粒度 4KB 或 1B
    unsigned char base_high;       // 基地址 24 ~ 31 位
} _packed descriptor_t;

// 段选择子
typedef struct selector_t
{
    unsigned char RPL : 2;          // Request PL，特权级，对应全局描述符中的 DPL
    unsigned char TI : 1;           // 0 表示这个段选择子定位的是全局描述符表， 1 表示定位局部描述符表 
    unsigned short index : 13;      // 描述符表索引
} _packed selector_t;

// 全局描述符指针
typedef struct pointer_t
{
    unsigned short limit; // size - 1
    unsigned int base;
} _packed pointer_t;

void gdt_init();

#endif 