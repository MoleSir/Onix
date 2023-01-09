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
// name 可以是多级目录，find_entry 只会判断第一级，并且把第一季去掉返回给 next
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

// 在 dir 目录中添加 name 目录项，name 只能是一级，不能分隔符
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
        // 说明，inode 就是进程的根目录（是进程根目录，不是总的根目录）
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
        brelse(buf);
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
// pathname 是相对当前进程的根目录，'/' 表示当前进程的根目录！！不是系统的根目录
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

// 创建一个目录
int sys_mkdir(char* pathname, int mode)
{
    char* next = NULL;
    buffer_t* ebuf = NULL;
    // 得到需要创建目录的父目录 inode
    inode_t* dir = named(pathname, &next);

    // 父目录不存在
    if (!dir)
        goto rollback;

    // 目录名为空
    if (!(*next))
        goto rollback;
    
    // 父目录无写权限
    if (!permission(dir, P_WRITE))
        goto rollback;
    
    char* name = next;
    dentry_t* entry;

    ebuf = find_entry(&dir, name, &next, &entry);
    // 目录已经存在
    if (ebuf)
        goto rollback;

    // 不存在，添加一个 dentry_t 项
    ebuf = add_entry(dir, name, &entry);
    ebuf->dirty = true;
    // 申请一个 inode 节点
    entry->nr = ialloc(dir->dev);

    task_t* task = running_task();
    // 读入新申请的 inode
    inode_t* inode = iget(dir->dev, entry->nr);
    inode->buf->dirty = true;

    inode->desc->gid = task->gid;
    inode->desc->uid = task->uid;
    inode->desc->mode = (mode & 0777 & ~(task->umask)) | IFDIR;
    // 默认新增一个目录都有上一个目录 .. 与当前文件 . 两个 dentry_t
    inode->desc->size = sizeof(dentry_t) * 2;
    inode->desc->mtime = time();
    // 有两个 dentry_t 指向这个 inode，一个是 . dentry，一个是父目录中的一项（可以找到 inode）
    inode->desc->nlinks = 2;

    // 父目录链接数量 + 1，因为多了一个目录 dentry_t，其中的 ..
    dir->buf->dirty = true;
    dir->desc->nlinks++;

    // 申请一个文件块，写入 inode 目录中的默认目录项（两个 . 于 ..）
    buffer_t* zbuf = bread(inode->dev, bmap(inode, 0, true));
    zbuf->dirty = true;

    // 获得 inode 文件内容所在文件块的缓冲
    entry = (dentry_t*)(zbuf->data);

    // 配置两个 dentry_t：名称与指向的 inode 索引号
    strcpy(entry->name, ".");
    // 指向本身
    entry->nr = inode->nr;

    strcpy(entry->name, "..");
    // 指向父目录 inode
    entry->nr = dir->nr;

    iput(inode);
    iput(dir);

    brelse(ebuf);
    brelse(zbuf);
    return 0;

rollback:
    brelse(ebuf);
    iput(dir);
    return EOF;
}

// 判断 inode 所指向的目录是否为空目录：只有 . 与 ..
static bool is_empty(inode_t* inode)
{
    assert(ISDIR(inode->desc->mode));

    // 目录项数量
    int entries = inode->desc->size / sizeof(dentry_t);
    if (entries < 2 || !inode->desc->zone[0])
    {
        LOGK("bad directory on dev\n", inode->dev);
        return false;
    }

    idx_t i = 0;
    idx_t block = 0;
    buffer_t* buf = NULL;
    dentry_t* entry;
    int count = 0;

    for (; i < entries; ++i)
    {
        if (!buf || (u32)entry >= (u32)(buf->data) + BLOCK_SIZE)
        {
            brelse(buf);
            block = bmap(inode, i / BLOCK_DENTRIES, false);
            assert(block);

            buf = bread(inode->dev, block);
            entry = (dentry_t*)(buf->data);
        }
        // 计算目录中的 dentry_t 有多少有效的数量
        if (entry->nr)
            count++;
    }

    brelse(buf);

    if (count < 2)
    {
        LOGK("bad directory on dev\n", inode->dev);
        return false;   
    }

    return count == 2;
}

// 删除目录
int sys_rmdir(char* pathname)
{
    char* next = NULL;
    buffer_t* ebuf = NULL;
    inode_t* dir = named(pathname, &next);
    inode_t* inode = NULL;
    int ret = EOF;

    // 没有父目录
    if (!dir)
        goto rollback;

    // 目录名为 空
    if (!*(next))
        goto rollback;
    
    // 父目录无写权限
    if (!permission(dir, P_WRITE))
        goto rollback;

    char* name = next;
    dentry_t* entry;

    // 找到等待删除的目录的 dentry_t 结构体
    ebuf = find_entry(&dir, name, &next, &entry);
    // 不存在
    if (!ebuf)
        goto rollback;
    
    // 根据 dentry_t 结构体，得到该目录指向的 inode
    inode = iget(dir->dev, entry->nr);
    if (!inode)
        goto rollback;

    // 删除 ..
    if (inode == dir)
        goto rollback;

    // inode 不是目录 
    if (!ISDIR(inode->desc->mode))
        goto rollback;
    
    // 判断权限
    task_t* task = running_task();
    if ((dir->desc->mode & ISVTX) && (task->uid != inode->desc->uid))
        goto rollback;

    if (dir->dev != inode->dev || inode->count > 1)
        goto rollback;

    if (!is_empty(inode))
        goto rollback;

    assert(inode->desc->nlinks == 2); 

    inode_truncate(inode);
    ifree(inode->dev, inode->nr);

    inode->desc->nlinks = 0;
    inode->buf->dirty = true;
    inode->nr = 0;

    dir->desc->nlinks--;
    dir->ctime = dir->atime = dir->desc->mtime = time();
    dir->buf->dirty = true;
    assert(dir->desc->nlinks > 0);

    entry->nr = 0;
    ebuf->dirty = true;

rollback:
    iput(inode);
    iput(dir);
    brelse(ebuf);
    return ret;    
}

// 将 oldname 所指向的文件硬链接到 newnmae 文件
// oldname 原来就存在，newname 是新建的，指向的也是 oldname
int sys_link(char* oldname, char* newname)
{
    int ret = EOF;
    buffer_t* buf = NULL;
    inode_t* dir = NULL;
    inode_t* inode = NULL;
    char* name = NULL;
    char* next = NULL;
    dentry_t* entry = NULL;

    // 找到 oldname 对应的 inode
    inode = namei(oldname);
    if (!inode)
        goto rollback;

    // 只能对文件硬链接，不能对目录
    if (ISDIR(inode->desc->mode))
        goto rollback;
    
    // 得到要创建的新目录的父目录，next 保存 oldname 的最后一级名称
    dir = named(newname, &next);
    if (!dir)
        goto rollback;

    // 名称不存在
    if (!(*next))
        goto rollback;

    // 不同设备不允许硬链接
    if (dir->dev != inode->dev)
        goto rollback;
    
    // 没有写权限
    if (!permission(dir, P_WRITE))
        goto rollback;

    // 此时 next 为最后生成的文件名称，判断其是不是已经存在
    name = next;
    buf = find_entry(&dir, name, &next, &entry);
    if (buf)
        goto rollback;

    // 添加一个新目录项
    buf = add_entry(dir, name, &entry);
    // 新目录项与 olename 文件指向同一个 inode
    entry->nr = inode->nr;
    buf->dirty = true;

    // 文件新增一个引用
    inode->desc->nlinks++;
    inode->ctime = time();
    inode->buf->dirty = true;
    ret = 0;

rollback:
    brelse(buf);
    iput(inode);
    iput(dir);
    return ret;
}

// 解除 filename 的对文件的链接
int sys_unlink(char* filename)
{
    int ret = EOF;
    char* next = NULL;
    char* name = NULL;
    inode_t* inode = NULL;
    buffer_t* buf = NULL;
    dentry_t* entry = NULL;

    // 找到 filename 目录的 inode
    inode_t* dir = named(filename, &next);
    if (!dir)
        goto rollback;

    // 文件名称不存在
    if (!(*next))
        goto rollback;

    // 权限
    if (!permission(dir, P_WRITE))
        goto rollback;

    // 找到 filename 对应的 inode
    name = next;
    buf = find_entry(&dir, name, &next, &entry);
    if (!buf)
        goto rollback;

    // 根据 entry 中的 nr 找到 inode
    inode = iget(dir->dev, entry->nr);
    if (ISDIR(inode->desc->mode))
        goto rollback;
    
    task_t* task = running_task();
    if ((inode->desc->mode & ISVTX) && task->uid != inode->desc->uid)
        goto rollback;

    if (!inode->desc->nlinks)
        LOGK("deleting non exists file (%04x:%d)\n", inode->dev, inode->nr);

    // 目录项指向空 nr
    entry->nr = 0;
    buf->dirty = true;

    // 文件链接数量 -1
    inode->desc->nlinks--;
    inode->buf->dirty = true;

    if (inode->desc->nlinks == 0)
    {
        // 文件块可以不要了！
        inode_truncate(inode);
        // 释放这个 inode （位图）
        ifree(inode->dev, inode->nr);
    }

rollback:
    brelse(buf);
    iput(inode);
    iput(dir);
    return ret;
}

// 打开文件，返回 inode，用于系统调用 open
inode_t *inode_open(char *pathname, int flag, int mode)
{
    inode_t* dir = NULL;
    inode_t* inode = NULL;
    buffer_t* buf = NULL;
    dentry_t* entry = NULL;
    char* next = NULL;
    char* name = NULL;

    // 获得父目录 inode
    dir = named(pathname, &next);
    if (!dir)
        goto rollback;

    // 文件名为空
    if (!(*next))
        goto rollback;

    // 又读又写
    if ((flag & O_TRUNC) && ((flag & O_ACCMODE) == O_RDONLY))
        flag |= O_RDWR;

    name = next;
    // 尝试获得文件在目录中的 entry
    buf = find_entry(&dir, name, &next, &entry);
    
    // 文件本来就存在
    if (buf)
    {
        // 拿到文件的 inode
        inode = iget(dir->dev, entry->nr);
        goto makeup;
    }

    // 文件本来是不存在的
    // 如果要求文件不存在就不创建，直接 rollback
    if (!(flag & O_CREAT))
        goto rollback;
    
    // 权限判断
    if (!permission(dir, P_WRITE))
        goto rollback;

    // 增加一个目录项目
    buf = add_entry(dir, name, &entry);
    // 申请一个文件块
    entry->nr = ialloc(dir->dev);
    // 根据 nr 号得到 inode 
    inode = iget(dir->dev, entry->nr);

    task_t* task = running_task();
    mode &= (0777 & (~(task->umask)));
    mode |= IFREG;

    // 配置 inode 信息
    inode->desc->uid = task->uid;
    inode->desc->gid = task->gid;
    inode->desc->mode = mode;
    inode->desc->mtime = time();
    inode->desc->size = 0;
    inode->desc->nlinks = 1;
    inode->buf->dirty = true;
    
makeup:
    // 目录文件或权限不足
    if (ISDIR(inode->desc->mode) || !permission(inode, flag & O_ACCMODE))
        goto rollback;

    inode->atime = time();
    
    if (flag & O_TRUNC)
        inode_truncate(inode);

    brelse(buf);
    iput(dir);
    return inode;

rollback:
    brelse(buf);
    iput(dir);
    iput(inode);
    return NULL;
}

// 计算 当前路径 pwd 和新路径 pathname，存入 pwd
void abspath(char* pwd, const char* pathname)
{
    char *cur = NULL;
    char *ptr = NULL;
    if (IS_SEPARATOR(pathname[0]))
    {
        cur = pwd + 1;
        *cur = 0;
        pathname++;
    }
    else
    {
        cur = strrsep(pwd) + 1;
        *cur = 0;
    }

    while (pathname[0])
    {
        ptr = strsep(pathname);
        if (!ptr)
        {
            break;
        }

        int len = (ptr - pathname) + 1;
        *ptr = '/';
        if (!memcmp(pathname, "./", 2))
        {
            /* code */
        }
        else if (!memcmp(pathname, "../", 3))
        {
            if (cur - 1 != pwd)
            {
                *(cur - 1) = 0;
                cur = strrsep(pwd) + 1;
                *cur = 0;
            }
        }
        else
        {
            strncpy(cur, pathname, len + 1);
            cur += len;
        }
        pathname += len;
    }

    if (!pathname[0])
        return;

    if (!strcmp(pathname, "."))
        return;

    if (strcmp(pathname, ".."))
    {
        strcpy(cur, pathname);
        cur += strlen(pathname);
        *cur = '/';
        *(cur + 1) = '\0';
        return;
    }
    if (cur - 1 != pwd)
    {
        *(cur - 1) = 0;
        cur = strrsep(pwd) + 1;
        *cur = 0;
    }
}

// 切换进程目录
int sys_chdir(char *pathname)
{
    // 找到 pathname 的 inode（相对次进程的根目录）
    task_t* task = running_task();
    inode_t* inode = namei(pathname);
    // 不存在，切换失败
    if (!inode)
        goto rollback;
    // 如果 pathname 不是目录，或者说当前已经在这个目录了，切换失败
    if (!ISDIR(inode->desc->mode) || inode == task->ipwd)
        goto rollback;
    
    // 设置进程的 pwd
    abspath(task->pwd, pathname);

    // 切换进程的 ipwd
    iput(task->ipwd);
    task->ipwd = inode;

    return 0;

rollback:
    iput(inode);
    return EOF;
}

// 切换进程根目录
int sys_chroot(char *pathname)
{
    // 找到 pathname 的 inode
    task_t* task = running_task();
    inode_t* inode = namei(pathname);
    // 不存在，切换失败
    if (!inode)
        goto rollback;
    // 如果 pathname 不是目录，或者说已经是跟目录了，切换失败
    if (!ISDIR(inode->desc->mode) || inode == task->iroot)
        goto rollback;   

    // 释放原来的根目录 inode
    iput(task->iroot);
    // 挂上新 iroor
    task->iroot = inode;
    return 0;
    
rollback:
    iput(inode);
    return EOF;
}

char* sys_getcwd(char *buf, size_t size)
{
    // 从 task_t 中读取
    task_t* task = running_task();
    strncpy(buf, task->pwd, size);
    return buf;
}

