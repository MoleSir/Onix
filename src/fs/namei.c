#include <onix/fs.h>
#include <onix/buffer.h>
#include <onix/stat.h>
#include <onix/syscall.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <string.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

// 判断文件名是否相等
// name 可能是一个文件名称，也可能是连续的目录，比如 "world.txt" 或者 "d1/d2/d3/d4"
// entry_name 是从 dentry 结构体取出的名称，只可能是单个名称：比如 "hello.txt" 或 "d3" 
static bool match_name(const char *name, const char *entry_name, char **next)
{
    char* lhs = (char*)name;
    char* rhs = (char*)entry_name;

    // 从头开始比较，比较到二者出现不不同的字符，或者某一方结束
    while (*lhs == *rhs && *lhs != EOS && *rhs != EOS)
    {
        lhs++;
        rhs++;
    }

    // 如果 entry_name 还有字符，那么一定不匹配，因为 entry_name 是 name 的前缀（也可能完全相同）
    if (*rhs)
        return false;
    
    // 如果 entry_name 没有字符了，同时 name 有，这种情况如果匹配的话就说明 name 是连续目录，而 entry_name 是第一个：
    if (*lhs && !IS_SEPARATOR(*lhs))
        return false;
    
    // 到这里说明匹配成功，有两张情况：name 已经结束、或者 name 没有结束但是下一关是分隔符
    if (IS_SEPARATOR(*lhs))
        lhs++;
    *next = lhs;
    return true;
}

// 获取 dir 目录下的 name 目录 所在的 dentry_t 和 buffer_t
buffer_t *find_entry(inode_t **dir, const char *name, char **next, dentry_t **result)
{
    // 保证 dir 是目录
    assert(ISDIR((*dir)->desc->mode));

    // 获取目录所在的超级块
    super_block_t* sb = read_super((*dir)->dev);

    // dir 目录最多子目录数量
    u32 entries = (*dir)->desc->size / sizeof(dentry_t);

    idx_t i = 0;
    idx_t block = 0;
    buffer_t* buf = NULL;
    dentry_t* entry = NULL;
    idx_t nr = EOF;

    // 遍历该目录下的所有 dentry
    for (; i < entries; ++i, ++entry)
    {
        // 因为这个目录占有的磁盘块可能不止一个，每当 entry 大于 buf->data 一个磁盘块大小时
        // 说明要读取下一个块了，就进入这个if更新一下
        if (!buf || (u32)entry >= (u32)(buf->data) + BLOCK_SIZE)
        {
            brelse(buf);
            // 得到第 i 个 dentry 所在的所在的磁盘逻辑块号
            block = bmap((*dir), i / BLOCK_DENTRIES, false);
            assert(block);

            // 读取磁盘信息，即读出第 i 个 dentry 信息
            buf = bread((*dir)->dev, block);
            // 解释为 dentry_t*
            entry = (dentry_t*)buf->data;
        }
        // 判断名称是否匹配
        if (match_name(name, entry->name, next))
        {
            // 找到了 entry，并且返回 entry 所在磁盘块的缓冲 
            *result = entry;
            return buf;
        }
    }

    brelse(buf);
    return NULL;
}

// 在 dir 目录中添加 name 目录项
buffer_t *add_entry(inode_t *dir, const char *name, dentry_t **result)
{
    char* next = NULL;
    
    // 检查是否已经添加过这个目录了
    buffer_t* buf = find_entry(&dir, name, &next, result);
    if (buf)
        return buf;

    // name 中不可以有分隔符
    for (size_t i = 0; i < NAME_LEN && name[i]; ++i)
        assert(!IS_SEPARATOR(name[i]));

    idx_t i = 0;
    idx_t block = 0;
    dentry_t* entry;

    // 尝试遍历 dir 目录下的所有 dentry
    for (; true; i++, entry++)
    {
        // 因为这个目录占有的磁盘块可能不止一个，每当 entry 大于 buf->data 一个磁盘块大小时
        // 说明要读取下一个块了，就进入这个if更新一下
        if (!buf || (u32)entry >= (u32)(buf->data) + BLOCK_SIZE)
        {
            brelse(buf);
            // 得到第 i 个 dentry 所在的磁盘逻辑块号
            block = bmap(dir, i / BLOCK_DENTRIES, false);
            assert(block);

            // 读取磁盘信息，即读出第 i 个 dentry 信息
            buf = bread(dir->dev, block);
            // 解释为 dentry_t*
            entry = (dentry_t*)buf->data;
        }

        // 一直遍历完所有的 dentry，再申请一个新的 dentry
        if (i * sizeof(dentry_t) >= dir->desc->size)
        {
            entry->nr = 0;
            // 更新目录内容的大小，因为要增加一个 dentry
            dir->desc->size = (i + 1) * sizeof(dentry_t);
            // 修改了目录文件的大小信息，标记为脏
            dir->buf->dirty = true;
        }
        
        // 还没到空闲的 dentry，继续找下一关
        if (entry->nr)
            continue;

        // 赋值目录名称
        strncpy(entry->name, name, NAME_LEN);

        // 修改了 dentry 所在磁盘块的信息（名称），标记为脏
        buf->dirty = true;
        // 目录被修改了，更新时间
        dir->desc->mtime = time();
        dir->buf->dirty = true;

        // 返回新建的 dentry 地址与其所在的磁盘块缓冲
        *result = entry;
        return buf;
    }
}

#include <onix/task.h>

void dir_test()
{
    task_t* task = running_task();
    inode_t* inode = task->iroot;
    inode->count++;

    char* next = NULL;
    dentry_t* entry = NULL;
    dentry_t* buf = NULL;

    buf = find_entry(&inode, "hello.txt", &next, &entry);
    idx_t nr = entry->nr;
    brelse(buf);

    buf = add_entry(inode, "world.txt", &entry);
    entry->nr = nr;

    inode_t* hello = iget(inode->dev, nr);
    hello->desc->nlinks++;
    hello->buf->dirty = true;

    iput(inode);
    iput(hello);
    brelse(buf);

    // char pathname[] = "d1/d2/d3/d4";
    // dev_t dev = inode->dev;
    // char* name = pathname;
    // buf = find_entry(&inode, name, &next, &entry);
    // brelse(buf);

    // iput(inode);
    // inode = iget(dev, entry->nr);

    // name = next;
    // buf = find_entry(&inode, name, &next, &entry);
    // brelse(buf);

    // iput(inode);
    // inode = iget(dev, entry->nr);

    // name = next;
    // buf = find_entry(&inode, name, &next, &entry);
    // brelse(buf);

    // iput(inode);
    // inode = iget(dev, entry->nr);

    // name = next;
    // buf = find_entry(&inode, name, &next, &entry);
    // brelse(buf);
    // iput(inode);
}