#include <onix/fs.h>
#include <onix/syscall.h>
#include <onix/stat.h>
#include <stdio.h>
#include <onix/device.h>

// 设备初始化，将 device_t 再抽象为文件
void dev_init()
{
    // 创建目录 "dev"
    mkdir("/dev", 0755);

    device_t* device = NULL;

    // 初始化控制台，为字符文件："/dev/console"，只写
    device = device_find(DEV_CONSOLE, 0);
    mknod("/dev/console", IFCHR | 0200, device->dev);

    // 初始化键盘，为字符文件："/dev/keyboard"，只读
    device = device_find(DEV_KEYBOARD, 0);
    mknod("dev/keyboard", IFCHR | 0400, device->dev);

    char name[32];

    // 初始化磁盘设备，可读可写
    for (size_t i = 0; true; ++i)
    {
        device_t* device = device_find(DEV_IDE_DISK, i);
        if (!device)
            break;

        sprintf(name, "/dev/%s", device->name);
        mknod(name, IFBLK | 0600, device->dev);
    }

    // 初始化块设备，可读可写
    for (size_t i = 0; true; ++i)
    {
        device_t* device = device_find(DEV_IDE_PART, i);
        if (!device)
            break;
        sprintf(name, "/dev/%s", device->name);
        mknod(name, IFBLK | 0600, device->dev);
    }
}
