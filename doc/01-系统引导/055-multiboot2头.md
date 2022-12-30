# multiboot2 头

要支持 multiboot2，内核必须添加一个 multiboot 头，而且必须再内核开始的 32768(0x8000) 字节，而且必须 64 字节对齐；

| 偏移  | 类型 | 名称                | 备注 |
| ----- | ---- | ------------------- | ---- |
| 0     | u32  | 魔数 (magic)        | 必须 |
| 4     | u32  | 架构 (architecture) | 必须 |
| 8     | u32  | 头部长度 (header_length)   | 必须 |
| 12    | u32  | 校验和 (checksum)   | 必须 |
| 16-XX |      | 标记 (tags)         | 必须 |

- `magic` = 0xE85250D6
- `architecture`:
    - 0：32 位保护模式
- `checksum`：与 `magic`, `architecture`, `header_length` 相加必须为 `0`


