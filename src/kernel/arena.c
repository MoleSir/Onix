#include <onix/arena.h>
#include <onix/memory.h>
#include <string.h>
#include <stdlib.h>
#include <onix/assert.h>
#include <onix/onix.h>

extern u32 free_pages;
static arena_descriptor_t descriptors[DESC_COUNT];

// 初始化 arent
void arena_init()
{
    u32 block_size = 16;
    for (size_t i = 0; i < DESC_COUNT; ++i)
    {
        arena_descriptor_t* desc = descriptors + i;
        desc->block_size = block_size;

        // 每页开始，保存一个 arena_t 结构体描述
        desc->total_block = (PAGE_SIZE - sizeof(arena_t)) / block_size;
        list_init(&(desc->free_list));

        // 16、32、64... 1024 字节
        block_size <<= 1;
    }
}

// 获得 arena 第 idx 块内存指针
static void* get_arena_block(arena_t* arena, u32 idx)
{
    assert(arena->desc->total_block > idx);

    // 内存开始是 arena 结构体，跳过
    void *addr = (void*)(arena + 1);

    // 偏移量 = 下标 * 每块的大小
    u32 gap = idx * (arena->desc->block_size);
    return (void*)((u32)addr + gap); 
}

// 通过某个内存对应的 arena 结构体指针
static arena_t* get_block_arena(block_t* block)
{
    return (arena_t*)((u32)block & 0xfffff000);
}

void* kmalloc(size_t size)
{
    arena_descriptor_t* desc = NULL;
    arena_t* arena = NULL;
    block_t* block;
    char* addr;

    // 申请内存超过最大的块 1024，用整夜来装，不用链表
    if (size > 1024)
    {
        // 一页开始的是 arena_t
        u32 asize = size + sizeof(arena_t);
        // 计算需要的页面数量
        u32 count = div_round_up(asize, PAGE_SIZE);

        arena = (arena_t*)alloc_kpage(count);
        memset(arena, 0, count * PAGE_SIZE);

        arena->large = true;
        arena->count = count;
        // 不需要链表
        arena->desc = NULL;
        arena->magic = ONIX_MAGIC;

        // 跳过 arena 结构体地址
        return (void*)(arena + 1);
    }

    // 申请的大小可以在 7 个描述符找到
    for (size_t i = 0; i < DESC_COUNT; ++i)
    {
        desc = descriptors + i;
        // 找到块大小满足申请空间的最小描述符
        if (desc->block_size >= size)
            break;
    }

    assert(desc != NULL);

    // 这种内存还没申请过，构建链表
    if (list_empty(&(desc->free_list)))
    {
        arena = (arena_t*)alloc_kpage(1);
        memset(arena, 0, PAGE_SIZE);

        arena->desc = desc;
        // 没有超出范围，如果超过就把一整页作空间，而不需要空闲链表链接
        arena->large = false;
        // 初始空闲数量为总数
        arena->count = desc->total_block;
        arena->magic = ONIX_MAGIC;

        // 链接所有的空页，此时全空页，依次链接起来
        for (size_t i = 0; i < desc->total_block; ++i)
        {
            block = get_arena_block(arena, i);
            assert(!list_search(&(arena->desc->free_list), block));
            list_push(&(arena->desc->free_list), block);
            assert(list_search(&(arena->desc->free_list), block));
        }
    }

    // 弹出闲链表的第一项
    block = list_pop(&(desc->free_list));

    // 找到块对应的 arena
    arena = get_block_arena(block);
    assert(arena->magic == ONIX_MAGIC && !arena->large);

    // 更新 arena
    arena->count--;
    return block;
}

void kfree(void* ptr)
{
    assert(ptr);

    block_t* block = (block_t*)ptr;
    arena_t* arena = get_block_arena(block);

    assert(arena->large == 1 || arena->large == 0);
    assert(arena->magic == ONIX_MAGIC);

    // 如果大小超过 1024，没有链表的情况
    if (arena->large == true)
    {
        free_kpage((u32)arena, arena->count);
        return;
    }

    // 插入空闲链表头部，会导致链表的顺序不是地址高低的顺序
    list_push(&(arena->desc->free_list), block);
    arena->count++;

    // 满了，所有块都是空闲，
    if (arena->count == arena->desc->total_block)
    {
        // 把链表打散，其实可以不用
        for (size_t i = 0; i < arena->desc->total_block; ++i)
        {
            block = get_arena_block(arena, i);
            assert(list_search(&(arena->desc->free_list), block));
            list_remove(block);
            assert(!list_search(&(arena->desc->free_list), block));
        }
        free_kpage((u32)arena, 1);
    }
}   

