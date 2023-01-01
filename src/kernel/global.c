#include <onix/global.h>
#include <string.h>
#include <onix/debug.h>

// 内核全局描述符
descriptor_t gdt[GDT_SIZE];
// 内核全局描述符表指针
pointer_t gdt_ptr;
// 任务状态段
tss_t tss;

void descriptor_init(descriptor_t *desc, u32 base, u32 limit)
{
    desc->base_low = base & 0xffffff;
    desc->base_high = (base >> 24) & 0xff;
    desc->limit_low = limit & 0xffff;
    desc->limit_high = (limit >> 16) & 0xf;
} 

// 初始化内存全局描述符表
void gdt_init()
{
    DEBUGK("init gdt!!!\n");

    memset(gdt, 0, sizeof(gdt));

    descriptor_t *desc;
    // 配置内核代码段全描述符，位于 gdt 的 1 号索引
    desc = gdt + KERNEL_CODE_IDX;
    descriptor_init(desc, 0, 0xFFFFF);
    desc->segment = 1;     // 代码段
    desc->granularity = 1; // 4K
    desc->big = 1;         // 32 位
    desc->long_mode = 0;   // 不是 64 位
    desc->present = 1;     // 在内存中
    desc->DPL = 0;         // 内核特权级
    desc->type = 0b1010;   // 代码 / 非依从 / 可读 / 没有被访问过

    // 配置内核数据段全局描述符，位于 gdt 的 2 号索引
    desc = gdt + KERNEL_DATA_IDX;
    descriptor_init(desc, 0, 0xFFFFF);
    desc->segment = 1;     // 数据段
    desc->granularity = 1; // 4K
    desc->big = 1;         // 32 位
    desc->long_mode = 0;   // 不是 64 位
    desc->present = 1;     // 在内存中
    desc->DPL = 0;         // 内核特权级
    desc->type = 0b0010;   // 数据 / 向上增长 / 可写 / 没有被访问过

    // 配置用户代码段全局描述符号，位于 gdt 的 4 号索引
    desc = gdt + USER_CODE_IDX;
    descriptor_init(desc, 0, 0xFFFFF);
    desc->segment = 1;     // 数据段
    desc->granularity = 1; // 4K
    desc->big = 1;         // 32 位
    desc->long_mode = 0;   // 不是 64 位
    desc->present = 1;     // 在内存中
    desc->DPL = 3;         // 用户特权级
    desc->type = 0b1010;   // 代码 / 非依从 / 可读 / 没有被访问过

    // 配置用户数据段全局描述符，位于 gdt 的 5 号索引
    desc = gdt + USER_DATA_IDX;
    descriptor_init(desc, 0, 0xFFFFF);
    desc->segment = 1;     // 数据段
    desc->granularity = 1; // 4K
    desc->big = 1;         // 32 位
    desc->long_mode = 0;   // 不是 64 位
    desc->present = 1;     // 在内存中
    desc->DPL = 3;         // 用户特权级
    desc->type = 0b0010;   // 数据 / 向上增长 / 可写 / 没有被访问过

    // 保存 gdt
    gdt_ptr.base = (u32)&gdt;
    gdt_ptr.limit = sizeof(gdt) - 1;
}

void tss_init()
{
    memset(&tss, 0, sizeof(tss));

    tss.ss0 = KERNEL_DATA_SELECTOR;
    tss.iobase = sizeof(tss);

    // 配置 TSS 全局描述符，位于 gdt 的 1 号索引
    descriptor_t* desc = gdt + KERNEL_TSS_IDX;
    descriptor_init(desc, (u32)&tss, sizeof(tss) - 1);
    desc->segment = 0;      // 系统段
    desc->granularity = 0;  // 长度单位为字节
    desc->big = 0;          // 固定为 0
    desc->long_mode = 0;    // 固定为 0
    desc->present = 1;      // 在内存中
    desc->DPL = 0;          // 任务门或调用门
    desc->type = 0b1001;    // 32 为可用 tss

    // 使用 ltr 将 tss 选择子保存
    asm volatile(
        "ltr %%ax\n" ::"a"(KERNEL_TSS_SELECTOR));
}