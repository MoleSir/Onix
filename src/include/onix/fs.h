#ifndef __ONIX_FS_HH__
#define __ONIX_FS_HH__

#include <onix/types.h>
#include <ds/list.h>
#include <onix/buffer.h>

// 块大小
#define BLOCK_SIZE 1024
// 扇区大小
#define SECTOR_SIZE 512

// 文件系统魔数
#define MINIX1_MAGIC 0x137f
// 文件名长度
#define NAME_LEN 14

#define IMAP_NR 8
#define ZMAP_NR 8

typedef struct inode_desc_t
{
    u16 mode;       // 文件类型和属性
    u16 uid;        // 属于哪个用户
    u32 size;       // 文件大小
    u32 mtime;      // 修改时间戳
    u8 gid;         // 组 id （文件所在者的组）
    u8 nlinks;      // 链接数量（多少文件目录指向次 i 节点）
    u16 zone[9];    // 直接（0-6），间接（7），双重间接（8）
} inode_desc_t;

typedef struct inode_t
{
    inode_desc_t* desc;     // inode 描述符
    struct buffer_t* buf;   // inode 描述符对应的 buffer
    dev_t dev;              // 设备号
    idx_t nr;               // i 节点号
    u32 count;              // 引用计数
    time_t atime;           // 访问时间
    time_t ctime;           // 创建时间
    list_node_t node;       // 链表节点
    dev_t mount;            // 安装设备
} inode_t;

typedef struct super_desc_t
{
    u16 inodes;         // 节点数
    u16 zones;          // 逻辑块数
    u16 imap_block;     // i 节点位图所占的数据块数量
    u16 zmap_block;     // 逻辑块位图所占的数据块数量
    u16 firstdatazone;  // 第一个数据逻辑块号
    u16 log_zone_size;  // log2(每逻辑块数据块)
    u32 max_size;       // 文件最大长度
    u16 magic;          // 文件系统魔数
} super_desc_t;

typedef struct super_block_t
{
    super_desc_t* desc;             // 超级块描述符
    struct buffer_t* buf;           // 超级快描述符 buffer
    struct buffer_t* imaps[IMAP_NR];// inode 位图缓冲
    struct buffer_t* zmaps[ZMAP_NR];// 块位图缓冲
    dev_t dev;                      // 设备号
    list_t inode_list;              // 使用中的 inode 链表
    inode_t* iroot;                 // 根目录的 inode
    inode_t* imount;                // 安装到的 inode
} super_block_t;

typedef struct dentry_t
{
    u16 nr;                 // i 节点
    char name[NAME_LEN];    // 文件名称
} dentry_t;

#endif