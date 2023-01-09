# 系统调用 stat,fstat

完成以下系统调用：

```c++
// 获取文件状态，通过文件名或文件描述符
int stat(char *filename, stat_t *statbuf);
int fstat(fd_t fd, stat_t *statbuf);
```
