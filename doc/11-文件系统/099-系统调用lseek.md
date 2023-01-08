# 系统调用 lseek

完成以下系统调用：

```c++
// 设置文件偏移量
int lseek(fd_t fd, off_t offset, int whence);
```

更改进程的 fd 号文件的偏移到指定位置；


## `whence_t` 枚举

在 fs.h 增加以下枚举，描述设置偏移的方式：

```c
typedef enum whence_t
{
    SEEK_SET = 1,   // 直接设置偏移
    SEEK_CUR,       // 当前位置偏移
    SEEK_END,       // 结束位置偏移
} whence_t;
```

- `SEEK_SET`：将偏移设置到: `offset`；
- `SEEK_CUR`：将偏移设置到：文件当前位置 + `offset`；
- `SEEK_END`：将偏移设置到：文件最后的位置 + `offset`。


## `lseek` 实现

跳过系统调用注册过程，实现为：

```c
int sys_lseek(fd_t fd, off_t offset, int whence)
{
    assert(fd < TASK_FILE_NR);
    // 得到文件
    task_t* task = running_task();
    file_t* file = task->files[fd];

    assert(file);
    assert(file->inode);

    // 根据 whence 跳转类型
    switch (whence)
    {
    case SEEK_SET:
        assert(offset >= 0);
        file->offset = offset;
        break;
    case SEEK_CUR:
        assert(file->offset + offset > 0);
        file->offset += offset;
        break;
    case SEEK_END:
        // 超过总大小也没关系，inode_write 会拓容
        assert(file->inode->desc->size + offset >= 0);
        file->offset = file->inode->desc->size + offset;
        break;
    default:
        panic("whence not defined!!!");
        break;
    }
    return file->offset;
}
```

> 偏移设置超过总大小是没关系的，`inode_write` 可以自动拓容；