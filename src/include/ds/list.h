#ifndef __ONIX_LIST_HH__
#define __ONIX_LIST_HH__

#include <onix/types.h>

// 获取 type 结构体中 member 字段的偏移
#define element_offset(type, member) (u32)(&((type *)0)->member)
#define element_entry(type, member, ptr) (type *)((u32)ptr - element_offset(type, member))

// 链表节点
typedef struct list_node_t
{
    struct list_node_t* prve;
    struct list_node_t* next;
} list_node_t;

// 链表
typedef struct list_t
{
    list_node_t head;
    list_node_t tail;
} list_t;

// 初始化链表
void list_init(list_t* list);

// 在 anchor 节点前插入 node
void list_insert_before(list_node_t* anchor, list_node_t* node);

// 在 anchor 节点后插入 node
void list_insert_after(list_node_t* anchor, list_node_t* node);

// 插入到头节点后
void list_push(list_t* list, list_node_t* node);

// 移除头节点后的节点
list_node_t* list_pop(list_t* list);

// 插入到尾节点之
void list_pushback(list_t* list, list_node_t* node);

// 移除尾节点前的节点
list_node_t* list_popback(list_t* list);

// 查找列表中节点是否存在
bool list_search(list_t* list, list_node_t* node);

// 从链表中删除节点
void list_remove(list_node_t* node);

// 判断链表是否为空
bool list_empty(list_t* list);

// 获取链表长度
u32 list_size(list_t* list);

#endif