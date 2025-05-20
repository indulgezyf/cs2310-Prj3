#ifndef BLOCK_CACHE_H
#define BLOCK_CACHE_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"  // for uint

#define CACHE_CAPACITY 1024  // 可缓存的块数，可根据内存和磁盘大小调整

typedef struct cache_entry {
  uint blockno;                     // 逻辑块号
  uchar data[BSIZE];                // 本地缓存的块内容
  bool dirty;                       // 是否被写过，需要回写
  struct cache_entry *prev, *next;  // 双向链表指针，维护 LRU
} cache_entry_t;

// 初始化、销毁
void cache_init(void);
void cache_destroy(void);

// 从缓存或后端加载一个块
void cache_read(uint blockno, uchar *buf);

// 将 buf 写入缓存，并标记 dirty
void cache_write(uint blockno, const uchar *buf);

// 强制把所有 dirty 块回写`
void cache_flush(void);

#endif  // BLOCK_CACHE_H
