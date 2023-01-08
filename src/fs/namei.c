#include <onix/fs.h>
#include <onix/buffer.h>
#include <onix/stat.h>
#include <onix/syscall.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <string.h>
#include <onix/task.h>
#include <onix/memory.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define P_EXEC IXOTH
#define P_READ IROTH
#define P_WRITE IWOTH

bool permission(inode_t *inode, u16 mask)
{
    // 文件权限
    u16 mode = inode->desc->mode;

    // inode 对应的硬连接是 0
    if (!inode->desc->nlinks)
        return false;

    task_t *task = running_task();

    // 用户态权限最高
    if (task->uid == KERNEL_USER)
        return true;

    // 自己的文件，判断前三位
    if (task->uid == inode->desc->uid)
        mode >>= 6;
    // 同组的文件，研判后三位
    else if (task->gid == inode->desc->gid)
        mode >>= 3;

    // 判断最后
    if ((mode & mask & 0b111) == mask)
        return true;
    return false;
}

// 获得第一个文件分隔符
char* strsep(const char* str)
{
    char* ptr =  (char*)str;
    while (true)
    {
        if (IS_SEPARATOR(*ptr))
            return ptr;
        if (*(ptr++) == EOS)
            return NULL;
    }
}

// 获取最后一个分隔符
char* strrsep(const char* str)
{
    char* last = NULL;
    char* ptr = (char*)str;
    while (true)
    {
        if (IS_SEPARATOR(*ptr))
            last = ptr;
        if (*(ptr++) == EOS)
            return last;
    }
}

// 判断文件名是否相等
// entry_name 是否为 name 的前缀
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

// 获取 pathname 中最低级路径的上一级目录
// 比如 "/home/d1/d2/hello.c"，返回 "/home/di/d2" 这个目录的 inode
inode_t *named(char *pathname, char **next)
{
    inode_t* inode = NULL;
    task_t* task = running_task();
    char* left = pathname;

    // 第一个字符是分隔符，说明从根目录开始
    if (IS_SEPARATOR(left[0]))
    {
        // 说明，inode 就是进程的根目录
        inode = task->iroot;
        // 跳过分隔符
        left++;
    }

    // left 为空，表示当前目录
    else if (left[0])
        inode = task->ipwd;
    else
        return NULL;

    // 如果是根目录就已经把 '/' 去掉了
    inode->count++;

    // *next 表示 pathname 去掉根目录后的字符串
    *next = left;

    // 没有子目录，直接返回根目录，或当前目录
    if (!*left)
        return inode;

    // 存在子目录，找到路径中最右侧的分隔符
    char* right = strrsep(left);
    if (!right || right < left)
        return inode;
    
    // 跳过分隔符，得到最后一级名称
    right++;

    *next = left;
    dentry_t* entry = NULL;
    buffer_t* buf = NULL;
    while (true)
    {
        // inode 是路径的最高目录，left 是去掉最高目录的路径，调用 find_entry
        // 获得 left 的 inode 与 buf，如果 left 还不是最后一级，把之后的路径放入 next 返回
        buf = find_entry(&inode, left, next, &entry);

        // 没找到，失败
        if (!buf)
            goto failure;
        
        // 找到了，获取 left 对应的 inode
        dev_t dev = inode->dev;
        iput(inode);
        inode = iget(dev, entry->nr);

        // 如果不是目录或权限不允许，失败
        if (!ISDIR(inode->desc->mode) || !permission(inode, P_EXEC))
            goto failure;
        
        // 如果此时，最后一级名称等于 left 的后级，说明找到了，成功
        if (right == *next)
            goto success;

        // 不成功、也不失败，继续找下一级
        left = *next;
    }

success:
    brelse(buf);
    return inode;

failure:
    brelse(buf);
    iput(inode);
    return NULL;
}

// 获取 pathname 对应的 inode
inode_t *namei(char *pathname)
{
    char* next = NULL;
    // 先获得 pathname 次高级目录，比如 "/home/hello.c"，这里先得到 "/home" 的 inode
    inode_t* dir = named(pathname, &next);
    if (!dir)
        return NULL;
    if (!(*next))
        return dir;

    // 再到此高级目录中找
    char* name = next;
    dentry_t* entry = NULL;
    // 调用 find_entry 寻找
    buffer_t* buf = find_entry(&dir, name, &next, &entry);
    if (!buf)
    {
        iput(dir);
        return NULL;
    }

    inode_t* inode = iget(dir->dev, entry->nr);

    iput(dir);
    brelse(buf);
    return inode;
}

#include <onix/task.h>

void dir_test()
{
    char pathname[] = "/";
    char* name = NULL;
    inode_t* inode = named(pathname, &name);
    iput(inode);

    inode = namei("/home/hello.txt");
    LOGK("get inode %d\n", inode->nr);
    iput(inode);
}