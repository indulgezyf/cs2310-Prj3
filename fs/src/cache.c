// cache.c
#include "cache.h"
#include "common.h"
#include "log.h"
#include "tcp_buffer.h"
#include "superblock.h"

// 引用你的全局变量
extern int bds_fd;
extern tcp_buffer *bds_rbuf, *bds_wbuf;
extern int nsec;
extern superblock sb;

#define BSZ BSIZE


// 固定大小数组存放所有条目
static cache_entry_t cache_arr[CACHE_CAPACITY];
static int cache_size = 0;            // 已使用条目数
static cache_entry_t *lru_head = NULL; // 最久未用
static cache_entry_t *lru_tail = NULL; // 最近使用
static uint cache_hits = 0, cache_misses = 0;

// === LRU 辅助 ===

// detach a block from the LRU list
static void detach(cache_entry_t *e) {
    if (e->prev) e->prev->next = e->next;
    else         lru_head = e->next;
    if (e->next) e->next->prev = e->prev;
    else         lru_tail = e->prev;
    e->prev = e->next = NULL;
}

// attach a block to the tail of the LRU list
static void attach_tail(cache_entry_t *e) {
    e->prev = lru_tail;
    e->next = NULL;
    if (lru_tail) lru_tail->next = e;
    lru_tail = e;
    if (!lru_head) lru_head = e;
}

//
static void fetch_block_from_server(uint blockno, uchar *buf) {
    char cmd[64]; 
    // R c s\n 
    int L = snprintf(cmd,sizeof(cmd),"R %u %u\n",
                     blockno / nsec, blockno % nsec);
    buffer_append(bds_wbuf, cmd, L);
    send_buffer(bds_wbuf, bds_fd);
    // 等待 512-byte payload
    while (1) {
        if (read_to_buffer(bds_rbuf, bds_fd) < 0) {
            Error("cache: read_to_buffer failed");
            return;
        }
        char *payload; 
        int plen = buffer_next(bds_rbuf, &payload);
        if (plen > 0) {
            if (plen < 4 + BSZ || strncmp(payload,"Yes ",4)!=0) {
                Error("cache: bad server reply");
                return;
            }
            memcpy(buf, payload+4, BSZ);
            recycle_read(bds_rbuf, 4+plen);
            adjust_buffer(bds_rbuf);
            return;
        }
    }
}

static void push_block_to_server(uint blockno, const uchar *buf) {
    // W c s data\n
    char *msg = malloc(BSZ + 64);
    if(!msg) {
        Error("cache: malloc failed");
        return;
    }
    int pos = snprintf(msg,64,"W %u %u %u ",
            blockno / nsec, blockno % nsec, BSZ);
    memcpy(msg+pos, buf, BSZ);
    pos += BSZ;
    msg[pos++] = '\n';
    buffer_append(bds_wbuf, msg, pos);
    free(msg);
    send_buffer(bds_wbuf, bds_fd);
    // 等 “Yes”
    while (1) {
        if (read_to_buffer(bds_rbuf, bds_fd) < 0) {
            Error("cache: write_to_buffer failed");
            return;
        }
        char *payload; int plen = buffer_next(bds_rbuf, &payload);
        if (plen > 0) {
            if (strncmp(payload,"Yes",3)!=0)
                Error("cache: bad write ack");
            recycle_read(bds_rbuf, 4+plen);
            adjust_buffer(bds_rbuf);
            return;
        }
    }
}

// === Cache 接口实现 ===

void cache_init(void) {
    cache_size = 0;
    lru_head = lru_tail = NULL;
    cache_hits = cache_misses = 0;
}

void cache_destroy(void) {
    cache_flush();
    // nothing else to free
}

// 淘汰最旧条目，或分配新条
static cache_entry_t* alloc_entry(void) {
    if (cache_size < CACHE_CAPACITY) {
        cache_entry_t *e = &cache_arr[cache_size++];
        e->dirty = false;
        e->prev = e->next = NULL;
        return e;
    }
    // 淘汰头部
    cache_entry_t *victim = lru_head;
    detach(victim);
    if (victim->dirty) {
        push_block_to_server(victim->blockno, victim->data);
        victim->dirty = false;
    }
    return victim;
}

void cache_read(uint blockno, uchar *out_buf) {
    // find in cache
    for (int i = 0; i < cache_size; i++) {
        cache_entry_t *e = &cache_arr[i];
        if (e->blockno == blockno) {
            // hit
            Log("cache_read: cache hit!");
            cache_hits++;
            detach(e);
            attach_tail(e);
            memcpy(out_buf, e->data, BSZ);
            return;
        }
    }
    // miss
    Log("cache_read: cache miss!");
    cache_misses++;
    cache_entry_t *e = alloc_entry();
    e->blockno = blockno;
    fetch_block_from_server(blockno, e->data);
    attach_tail(e);
    memcpy(out_buf, e->data, BSZ);
}

void cache_write(uint blockno, const uchar *in_buf) {
    // find in cache
    for (int i = 0; i < cache_size; i++) {
        cache_entry_t *e = &cache_arr[i];
        if (e->blockno == blockno) {
            Log("cache_write: cache hit!");
            cache_hits++;
            detach(e);
            attach_tail(e);
            memcpy(e->data, in_buf, BSZ);
            e->dirty = true;
            return;
        }
    }
    // miss
    Log("cache_write: cache miss!");
    cache_misses++;
    cache_entry_t *e = alloc_entry();
    e->blockno = blockno;
    // load old data
    fetch_block_from_server(blockno, e->data);
    detach(e);  
    attach_tail(e);
    memcpy(e->data, in_buf, BSZ);
    e->dirty = true;
}

void cache_flush(void) {

    // wirte all dirty blocks back to server
    for (int i = 0; i < cache_size; i++) {
        cache_entry_t *e = &cache_arr[i];
        if (e->dirty) {
            push_block_to_server(e->blockno, e->data);
            e->dirty = false;
        }
    }
    Log("Cache stats: hits=%u misses=%u", cache_hits, cache_misses);
}
