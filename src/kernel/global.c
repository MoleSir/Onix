#include <onix/global.h>
#include <onix/string.h>
#include <onix/debug.h>

descriptor_t gdt[GDT_SIZE];
pointer_t gdt_ptr;

// 初始化内存全局描述符表
void gdt_init()
{
    DEBUGK("init gdt!!!\n");

    // 将 loader.asm 定义好的 gdt 指针赋值过来
    asm volatile("sgdt gdt_ptr");

    // 拷贝到 gdt 
    memcpy(gdt, (void*)gdt_ptr.base, gdt_ptr.limit + 1);

    // 更新 gdt_ptr 的内容，因为 gdt 已经被拷贝了
    gdt_ptr.base = (u32)&gdt;
    gdt_ptr.limit = sizeof(gdt) - 1;
    // 重新设置 gdt_ptr 给 CPU
    asm volatile("lgdt gdt_ptr");
}