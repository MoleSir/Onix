#ifndef __ONIX_ARENA_HH__
#define __ONIX_ARENA_HH__

#include <onix/types.h>
#include <ds/list.h>

#define DESC_COUNT 7

// 把每个空闲块的起始作为 list_node_t 结构体链接起来
typedef list_node_t block_t;

// 内存描述符
typedef struct arena_descriptor_t
{
    u32 total_block;    // 一页内存分成了多少快
    u32 block_size;     // 块大小
    list_t free_list;   // 空闲列表
} arena_descriptor_t;

// 描述一页或多页内存的分配情况
typedef struct arena_t
{
    arena_descriptor_t* desc;   // 该 arena 的描述符
    u32 count;                  // 当前剩余多少块，或页数
    u32 large;                  // 表示是不是超过了 1024 字节
    u32 magic;                  // 魔数
} arena_t;

void* kmalloc(size_t size);
void kfree(void* ptr);

#endif