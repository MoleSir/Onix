#ifndef __ONIX_TYPES_HH__
#define __ONIX_TYPES_HH__


#define EOF -1 // END OF FILE

#define NULL 0 // NULL pointer

#define bool _Bool
#define true 1
#define false 0

#define _packed __attribute((_packed)) // 定义特殊结构体

typedef unsigned int size_t;

typedef char int8;
typedef short int16;
typedef int int32;
typedef long long int64;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

#endif