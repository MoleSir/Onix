#include <onix/mutex.h>
#include <onix/task.h>
#include <onix/interrupt.h>
#include <onix/assert.h>
#include <onix/onix.h>

// 初始化互斥量
void mutex_init(mutex_t* mutex)
{
    // 初始化无人持有
    mutex->value = false;
    list_init(&mutex->waiters);
}

// 尝试持有互斥量
void mutex_lock(mutex_t* mutex)
{
    // 关闭中断
    bool intr = interrupt_disable();

    // 判断是否有人持有
    task_t* current = running_task();
    while (mutex->value == true)
        task_block(current, &mutex->waiters, TASK_BLOCKED);

    assert(mutex->value == false);

    // 设置为有人持有
    mutex->value++;
    assert(mutex->value == true);

    // 恢复中断状态
    set_interrupt_state(intr); 
}

// 释放互斥量
void mutex_unlock(mutex_t* mutex)
{
    // 关闭中断
    bool intr = interrupt_disable();

    assert(mutex->value == true);

    // 取消持有
    mutex->value--;
    assert(mutex->value == false);

    // 如果还任务在等待队列中，则唤醒
    if (!list_empty(&(mutex->waiters)))
    {
        // 从后开始唤醒
        task_t* task = element_entry(task_t, node, mutex->waiters.tail.prve);
        assert(task->magic == ONIX_MAGIC);
        // 唤醒，加入就绪状态
        task_unblock(task);
        // 放弃处理
        task_yield();
    }

    // 恢复之前的中断状态
    set_interrupt_state(intr);
}

// 锁初始化
void lock_init(lock_t *lock)
{
    lock->holder = NULL;
    lock->repeat = 0;
    mutex_init(&lock->mutex);
}

// 加锁
void lock_acquire(lock_t *lock)
{
    task_t* current = running_task();

    // 非持有者
    if (lock->holder != current)
    {
        // 申请资源
        mutex_lock(&lock->mutex);
        // 本任务持有
        lock->holder = current;
        assert(lock->repeat == 0);
        lock->repeat = 1;
    }
    // 持有
    else
    {
        lock->repeat++;
    }
}

// 解锁
void lock_release(lock_t *lock)
{
    task_t* current = running_task();
    assert(lock->holder == current);

    if (lock->repeat > 1)
    {
        lock->repeat--;
        return;
    }

    assert(lock->repeat == 1);

    lock->holder = NULL;
    lock->repeat = 0;
    mutex_unlock(&lock->mutex);
}