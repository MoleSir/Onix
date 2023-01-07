#include <onix/task.h>

mode_t sys_umask(mode_t mask)
{
    task_t* task = running_task();
    mode_t old_mask = task->umask;
    // 8 进制的 777 -> 表示最低 9 位全部为 1 -> 0b111111111
    task->umask = mask & 0777;
    return old_mask;
}
