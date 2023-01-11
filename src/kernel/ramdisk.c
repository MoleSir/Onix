#include <onix/memory.h>
#include <onix/debug.h>
#include <onix/device.h>
#include <onix/assert.h>
#include <onix/types.h>
#include <string.h>
#include <stdio.h>

#define SECTOR_SIZE 512

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define RAMDISK_NR 4

typedef struct ramdisk_t
{   
    u8* start;      // 内存开始的位置
    u32 size;       // 占用内存的大小
} ramdisk_t;

static ramdisk_t ramdisks[RAMDISK_NR];

int ramdisk_ioctl(ramdisk_t* disk, int cmd, void* args, int flags)
{
    switch( cmd )
    {
    case DEV_CMD_SECTOR_START:
        return 0;
        break;
    case DEV_CMD_SECTOR_COUNT:
        return disk->size / SECTOR_SIZE;
        break;
    default:
        panic("cmd not founf!!!\n");
        break;
    }
}

// 以扇区为单位，读以 lba 起始的扇区，读 count 块
int ramdisk_read(ramdisk_t* disk, void* buf, u8 count, idx_t lba)
{
    // 从内存中读
    void* addr = disk->start + lba * SECTOR_SIZE;
    u32 len = count * SECTOR_SIZE;
    assert(((u32)addr + len) < (KERNEL_RAMDISK_MEM + KERNEL_MEMORY_SIZE));
    memcpy(buf, addr, len);
    return count;
}

// 以扇区为单位，写以 lba 起始的扇区，写 count 块
int ramdisk_write(ramdisk_t* disk, void* buf, u8 count, idx_t lba)
{
    // 写入内存
    void* addr = disk->start + lba * SECTOR_SIZE;
    u32 len = count * SECTOR_SIZE;
    assert(((u32)addr + len) < (KERNEL_RAMDISK_MEM + KERNEL_MEMORY_SIZE));
    memcpy(addr, buf, len);
    return count;
}

int ramdisk_init()
{
    LOGK("ramdisk init...\n");

    // 总大小 / 虚拟磁盘数量
    u32 size = KERNEL_RAMDISK_SIZE / RAMDISK_NR;
    assert(size % SECTOR_SIZE == 0);

    char name[32];
    for (size_t i = 0; i < RAMDISK_NR; ++i)
    {
        ramdisk_t* ramdisk = ramdisks + i;
        // 磁盘的起始地址为 RAMDISK_MEM + 这次的偏移
        ramdisk->start = (u8*)(KERNEL_RAMDISK_MEM + size * i);
        ramdisk->size = size;
        sprintf(name, "md%c", i + 'a');
        // 将内存磁盘封装为设备，传入控制、读写函数
        device_install(DEV_BLOCK, DEV_RAMDISK, ramdisk, name, 0,
                       ramdisk_ioctl, ramdisk_read, ramdisk_write);
    }
}