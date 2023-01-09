# 系统调用 getcwd,chdir

完成以下系统调用：

```c++
// 获取当前路径
char *getcwd(char *buf, size_t size);
// 切换当前目录
int chdir(char *pathname);
// 切换根目录
int chroot(char *pathname);
```


## 进程根目录与当前目录

在 `task_t` 结构体中保存：

````c
typedef struct task_t
{
    ...
    char* pwd;                          // 进程当前目录
    struct inode_t* ipwd;               // 进程当前的目录 inode
    struct inode_t* iroot;              // 进程根目录 inode
    ...
} task_t;
````

- `iroot`：表示当前进程的根目录（默认是系统根目录，可以修改）inode。在某个进程中 `/` 这个目录就代表了进程的根目录，二不是系统的根目录，虽然通常二者是相同的；
- `ipwd`：表示当前进程所在的目录的 inode；
- `pwd`：表示进程当前所在的目录每次，相对进程根目录。比如进程根目录位于 `/d1`，那么如果进程当前位于 `/d1/d2`，`pwd` 只要写 `/d2` 即可。


## 获取当前路径

直接从 `task_t` 中拷贝即可：

````c
char* sys_getcwd(char *buf, size_t size)
{
    // 从 task_t 中读取
    task_t* task = running_task();
    strncpy(buf, task->pwd, size);
    return buf;
}
````


## 切换根目录

````c
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
````

1. 获得传入目录的 inode；
2. 检查是否为目录或是否已经是根目录；
3. 释放原来根目录的 inode；
4. 设置新的 inode；


## 切换当前目录

````c
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
````

流程几乎和切换根目录一样，只不过增加一步：设置 `task->pwd`；