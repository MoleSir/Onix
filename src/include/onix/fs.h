#ifndef __ONIX_FS_HH__
#define __ONIX_FS_HH__

#include <onix/types.h>
#include <ds/list.h>
#include <onix/buffer.h>

// 块大小
#define BLOCK_SIZE 1024
// 扇区大小
#define SECTOR_SIZE 512
// 块的 bit 数量
#define BLOCK_BITS (BLOCK_SIZE * 8)

// 文件系统魔数
#define MINIX1_MAGIC 0x137f
// 文件名长度
#define NAME_LEN 14

#define IMAP_NR 8
#define ZMAP_NR 8

// 块 inode 数量
#define BLOCK_INODES (BLOCK_SIZE / sizeof(inode_desc_t))
// 块 dentry 数量
#define BLOCK_DENTRIES (BLOCK_SIZE / sizeof(dentry_t))
// 块索引数量
#define BLOCK_INDEXES (BLOCK_SIZE / sizeof(u16))

// 直接块数量
#define DIRECT_BLOCK (7)
// 一级间接块数量
#define INDIRECT1_BLOCK BLOCK_INDEXES
// 二级间接块数量
#define INDIRECT2_BLOCK (INDIRECT1_BLOCK * INDIRECT1_BLOCK)
// 全部块数量
#define TOTAL_BLOCK (DIRECT_BLOCK + INDIRECT1_BLOCK + INDIRECT2_BLOCK)

// 目录分割符
#define SEPARATOR1 '/'
#define SEPARATOR2 '\\'
// 判断字符是否为分割符
#define IS_SEPARATOR(c) (c == SEPARATOR1 || c == SEPARATOR2)

enum file_flag
{
    O_RDONLY = 00,      // 只读方式
    O_WRONLY = 01,      // 只写方式
    O_RDWR = 02,        // 读写方式
    O_ACCMODE = 03,     // 文件访问模式屏蔽码
    O_CREAT = 00100,    // 如果文件不存在就创建
    O_EXCL = 00200,     // 独占使用文件标志
    O_NOCTTY = 00400,   // 不分配控制终端
    O_TRUNC = 01000,    // 若文件已存在且是写操作，则长度截为 0
    O_APPEND = 02000,   // 以添加方式打开，文件指针置为文件尾
    O_NONBLOCK = 04000, // 非阻塞方式打开和操作文件
};

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
    time_t ctime;           // 修改时间
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
    u32 count;                      // 引用计数
    list_t inode_list;              // 该文件系统中使用中的 inode 链表
    inode_t* iroot;                 // 根目录的 inode
    inode_t* imount;                // 安装到的 inode
} super_block_t;

typedef struct dentry_t
{
    u16 nr;                 // i 节点
    char name[NAME_LEN];    // 文件名称
} dentry_t;

typedef dentry_t dirent_t;

typedef struct file_t
{
    inode_t* inode;     // 文件 inode
    u32 count;          // 引用计数，多少进程打开了文件
    off_t offset;       // 文件偏移
    int flags;          // 文件标记
    int mode;           // 文件模式
} file_t;

typedef enum whence_t
{
    SEEK_SET = 1,   // 直接设置偏移
    SEEK_CUR,       // 当前位置偏移
    SEEK_END,       // 结束位置偏移
} whence_t;

// 获取设备 dev 的超级快
super_block_t* get_super(dev_t dev);

// 读设备 dev 的超级块
super_block_t* read_super(dev_t dev);

// 挂载根文件系统
void mount_root();

// 分配一个文件块
idx_t balloc(dev_t dev);

// 释放一个文件块
void bfree(dev_t dev, idx_t idx);

// 分配一个文件系统 inode
idx_t ialloc(dev_t dev);

// 释放一个文件系统 inode
void ifree(dev_t dev, idx_t idx);

// 获取 inode 第 block 块索引，如果不存在，并且 create 为 true 则创建
idx_t bmap(inode_t* inode, idx_t blokc, bool create);

// 获取根目录的 inode
inode_t* get_root_inode();

// 获得设备 dev 的 nr indoe
inode_t* iget(dev_t dev, idx_t nr);

// 释放 inode
void iput(inode_t* inode);

// 创建新的 inode 
inode_t* new_inode(dev_t dev, idx_t nr);

// 获取 pathname 对应的父目录 inode
inode_t *named(char *pathname, char **next);

// 获取 pathname 对应的 inode
inode_t *namei(char *pathname);

// 从 inode 的 offset 处，读 len 个字节到 buf
int inode_read(inode_t *inode, char *buf, u32 len, off_t offset);

// 从 inode 的 offset 处，将 buf 的 len 个字节写入磁盘
int inode_write(inode_t *inode, char *buf, u32 len, off_t offset);

// 让 inode 文件在内容清零
void inode_truncate(inode_t *inode);

// 打开文件，返回 inode，用于系统调用 open
inode_t *inode_open(char *pathname, int flag, int mode);

// 格式化文件系统
int devmkfs(dev_t dev, u32 icount);

#endif