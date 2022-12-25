# bootloader 补充说明

- boot.asm
- loader.asm

bootloader 的主要功能就是从 BIOS 加载内核，并且提供内核需要的参数。


## 0xaa55

魔数，就是用来检测错误；

- A 1010
- 5 0101
- 1010101001010101


## 0x7c00

- IBM PC 5150
- DOS 1.0
- 32K = 0x8000

栈：一般会放在内存的最后，因为栈时向下增长的；

32k - 1k = 0x7c00