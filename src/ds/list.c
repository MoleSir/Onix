#include <ds/list.h>
#include <onix/assert.h>

// 初始化链表
void list_init(list_t* list)
{
    // 头节点的上一个、尾节点的下一关为空
    list->head.prve = NULL;
    list->tail.next = NULL;
    // 头节点的下一关为尾、尾节点的上一个为头
    list->head.next = &(list->tail);
    list->tail.prve = &(list->head);
}

// 在 anchor 节点前插入 node
void list_insert_before(list_node_t* anchor, list_node_t* node)
{
    // 先链后断
    node->prve = anchor->prve;
    node->next = anchor;

    anchor->prve->next = node;
    anchor->prve = node;
}

// 在 anchor 节点后插入 node
void list_insert_after(list_node_t* anchor, list_node_t* node)
{
    // 先链后断
    node->prve = anchor;
    node->next = anchor->next;

    anchor->next->prve = node;
    anchor->next = node;
}

// 插入到头节点后，相当于插入到第一个
void list_push(list_t* list, list_node_t* node)
{
    assert(!list_search(list, node));
    list_insert_after(&(list->head), node);
}

// 移除头节点后的节点
list_node_t* list_pop(list_t* list)
{
    assert(!list_empty(list));

    list_node_t* node = list->head.next;
    list_remove(node);

    return node;
}

// 插入到尾节点之前，，相当于最后一个
void list_pushback(list_t* list, list_node_t* node)
{
    assert(!list_search(list, node));
    list_insert_before(&(list->tail), node);
}

// 移除尾节点前的节点
list_node_t* list_popback(list_t* list)
{
    assert(!list_empty(list));

    list_node_t* node = list->tail.prve;
    list_remove(node);

    return node;
}

// 查找列表中节点是否存在
bool list_search(list_t* list, list_node_t* node)
{
    list_node_t* next = list->head.next;
    list_node_t* tail = &(list->tail);
    while (next != tail)
    {
        if (node == next)
            return true;
        next = next->next;
    }   
    return false;
}

// 从链表中删除节点
void list_remove(list_node_t* node)
{
    assert(node->prve != NULL);
    assert(node->next != NULL);

    node->prve->next = node->next;
    node->next->prve = node->prve;
    node->prve = NULL;
    node->next = NULL;
}

// 判断链表是否为空
bool list_empty(list_t* list)
{
    return (list->head.next == &(list->tail));
}

// 获取链表长度
u32 list_size(list_t* list)
{
    u32 size = 0;
    list_node_t* next = list->head.next;
    list_node_t* tail = &(list->tail);
    while (next != tail)
    {
        size++;
        next = next->next;
    }   
    return size;
}


#include <onix/memory.h>
#include <onix/debug.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

void list_test()
{
    u32 count = 3;
    list_t holder;
    list_t* list = &holder;
    list_init(list);
    list_node_t* node;

    while (count--)
    {
        node = (list_node_t*)alloc_kpage(1);
        list_push(list, node);
    }

    while (!list_empty(list))
    {
        node = list_pop(list);
        free_kpage((u32)node, 1);
    }

    count = 3;
    while (count--)
    {
        node = (list_node_t*)alloc_kpage(1);
        list_pushback(list, node);
    }

    LOGK("list size: %d\n", list_size(list));

    while (!list_empty(list))
    {
        node = list_popback(list);
        free_kpage((u32)node, 1);
    }

    node = (list_node_t*)alloc_kpage(1);
    list_pushback(list, node);

    LOGK("search node 0x%p --> %d\n", node, list_search(list, node));
    LOGK("search node 0x%p --> %d\n", 0, list_search(list, 0));

    list_remove(node);
    free_kpage((u32)node, 1);
}