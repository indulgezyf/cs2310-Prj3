#include "inode.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

pthread_mutex_t inode_cache_lock = PTHREAD_MUTEX_INITIALIZER;

#include "block.h"
#include "log.h"
#define NINODES 20

// 每个块里能存多少个 uint 指针
#define IPTRS   (BSIZE / sizeof(uint))
// 二级间接块能存多少个数据块号
#define DPTRS   (IPTRS * IPTRS)


// char inode_bitmap[BSIZE/8] = {0};
// char block_bitmap[BSIZE/8] = {0};

inode inodes_cache[NINODES];

void init_inode_cache() {
    for (int i = 0; i < NINODES; i++) {
        inodes_cache[i].inum = -1;
    }
}


inode *iget(uint inum) {
    // 1. 检查 inum 越界
    uint max_inodes = sb.ninodeBlocks * (BSIZE / sizeof(dinode));
    if (inum >= max_inodes) {
        Error("iget: invalid inum %u", inum);
        return NULL;
    }

    pthread_mutex_lock(&inode_cache_lock);

    // 2. 查缓存：已有则直接 ++count 返回
    inode *empty = NULL;
    for (int i = 0; i < NINODES; i++) {
        if (inodes_cache[i].inum == inum) {
            inodes_cache[i].reference_count++;
            pthread_mutex_unlock(&inode_cache_lock);
            return &inodes_cache[i];
        }
        if (empty == NULL && inodes_cache[i].inum == (uint)-1) {
            empty = &inodes_cache[i];
        }
    }
    if (empty == NULL) {
        Error("iget: no empty slot in cache");
        pthread_mutex_unlock(&inode_cache_lock);
        return NULL;
    }

    // 3. 计算所在块号和块内偏移
    uint inodes_per_block = BSIZE / sizeof(dinode);
    uint blk_idx = inum / inodes_per_block;      // 第几个块
    uint idx     = inum % inodes_per_block;      // 块内的第几个 inode
    uint blockno = sb.inodeStart + blk_idx;

    // 4. 从后端读取整块
    uchar blockbuf[BSIZE];
    read_block(blockno, blockbuf);
    // TODO: 可以检测 read_block 的返回值并做错误处理

    // 5. 把 dinode 数据拷贝到缓存对象
    dinode *dp = (dinode *)(blockbuf + idx * sizeof(dinode));
    // 假设 in-memory inode 和 on-disk dinode 布局一致，直接 memcpy
    memcpy(empty, dp, sizeof(dinode));
    // 记得把 inum 和引用数给设好
    empty->inum = inum;
    empty->reference_count = 1;

    pthread_mutex_unlock(&inode_cache_lock);
    return empty;
}


void iput(inode *ip) { 
    
    if(ip == NULL)
    {
        Error("iput: Invalid inode");
        return;
    }

    pthread_mutex_lock(&inode_cache_lock);
    if(ip->reference_count > 0) {
        ip->reference_count--;
    }

    if(ip->reference_count == 0) {
        // update inode to disk
        iupdate(ip);
        // clear the cache
        memset(ip, 0, sizeof(inode));
        ip->inum = -1;
    }
    pthread_mutex_unlock(&inode_cache_lock);
}

// 分配一个新的 inode，type=T_FILE 或 T_DIR
inode *ialloc(short type) {
    uint inum = UINT_MAX;
    uint bits_per_block = BSIZE * 8;
    uchar bitmap[BSIZE];

    // 1. 找空闲位：只有一个位图块时 sb.ninodeBlocksBitmapBlocks == 1
    //    若有多块，需要循环 sb.ninodeBlocksBitmapBlocks
    for (uint b = 0; b < sb.ninodeBlocksBitmapBlocks; b++) {
        uint blockno = sb.inodeBitmapStart + b;
        read_block(blockno, bitmap);
        // TODO 这里需要修改read_block加上错误处理
        // if (read_block(blockno, bitmap) < 0) {
        //     Error("ialloc: read bitmap block %u failed", blockno);
        //     return NULL;
        // }
        // 每字节检查
        for (uint byte = 0; byte < BSIZE; byte++) {
            if (bitmap[byte] != 0xFF) {
                // 该字节中至少有一位空闲
                for (int bit = 0; bit < 8; bit++) {
                    if ((bitmap[byte] & (1 << bit)) == 0) {
                        // 找到空位
                        inum = b * bits_per_block + byte * 8 + bit;
                        // 越界检查
                        if (inum >= sb.size) {
                            Error("ialloc: inum %u out of range", inum);
                            return NULL;
                        }
                        // 标记为已用
                        bitmap[byte] |= (1 << bit);
                        // 写回这一整块
                        write_block(blockno, bitmap);
                        // TODO 这里需要修改wirte_block加上错误处理
                        // if (write_block(blockno, bitmap) < 0) {
                        //     Error("ialloc: write bitmap block %u failed", blockno);
                        //     return NULL;
                        // }
                        goto bitmap_done;
                    }
                }
            }
        }
    }
bitmap_done:
    if (inum == UINT_MAX) {
        Error("ialloc: no free inode");
        return NULL;
    }

    // 2. 构造 inode / dinode 在磁盘上的存放位置，并更新
    //    每个块可以存放 BSIZE/sizeof(dinode) 个 inode 条目
    uint inodes_per_block = BSIZE / sizeof(dinode);
    uint blk_idx  = inum / inodes_per_block;              // 在第几块
    uint offset_in_block = inum % inodes_per_block;       // 块内第几个

    uchar blockbuf[BSIZE];
    uint blockno = sb.inodeStart + blk_idx;
    read_block(blockno, blockbuf);
    // TODO 这里需要加上错误处理
    // if (read_block(blockno, blockbuf) < 0) {
    //     Error("ialloc: read inode block %u failed", blockno);
    //     return NULL;
    // }

    // pointer to where to write
    dinode *dip = (dinode *)(blockbuf + offset_in_block * sizeof(dinode));
    memset(dip, 0, sizeof(dinode));
    dip->inum  = inum;
    dip->type  = type;
    dip->nlink = 1;
    dip->size  = 0;
    dip->blocks= 0;
    dip->uid   = 0;
    dip->gid   = 0;
    dip->mode  = (type==T_DIR ? 0755 : 0644);
    time_t now = time(NULL);
    dip->atime = now;
    dip->ctime = now;
    dip->mtime = now;
    dip->flags = 0;
    // addrs 和 reserved 已经被 memset 清零

    // 写回 inode 块
    write_block(blockno, blockbuf);
    //TODO 修改write_block函数加上错误处理
    // if (write_block(blockno, blockbuf) < 0) {
    //     Error("ialloc: write inode block %u failed", blockno);
    //     return NULL;
    // }

    // 3. 交给缓存层加载并返回
    return iget(inum);
}

void iupdate(inode *ip) {
    uint inum = ip->inum;
    // 每块能放多少个 inode
    uint inodes_per_block = BSIZE / sizeof(inode);
    // inode 所在的块偏移（相对于 sb.inodeStart）
    uint blk_idx = inum / inodes_per_block;
    // inode 在块内的索引
    uint idx     = inum % inodes_per_block;

    // 1. 读出那一块
    uchar blockbuf[BSIZE];
    uint blockno = sb.inodeStart + blk_idx;
    read_block(blockno, blockbuf);
    // TODO 这里需要修改read_block加上错误处理
    // if (read_block(blockno, blockbuf) < 0) {
    //     Error("iupdate: read_block %u failed", blockno);
    //     return;
    // }

    // 2. 修改那条 inode 记录
    memcpy(blockbuf + idx * sizeof(inode), ip, sizeof(inode));

    // 3. 写回整块
    write_block(blockno, blockbuf);
    // TODO 这里需要修改write_block加上错误处理
    // if (write_block(blockno, blockbuf) < 0) {
    //     Error("iupdate: write_block %u failed", blockno);
    //     return;
    // }
}

// // alloc == 1 表示如果没有块则分配，alloc == 0 表示只查找
// // alloc=1 表示没有块时自动分配，alloc=0 则只查不分配
// // bmap map the logical block number bnum to the physical block number
// uint bmap(inode *ip, uint bnum, int alloc) {
//     // 1. 直接块
//     if (bnum < NDIRECT) {
//         if (ip->addrs[bnum] == 0 && alloc) {
//             ip->addrs[bnum] = balloc();
//             ip->blocks++;
//         }
//         return ip->addrs[bnum];
//     }
//     bnum -= NDIRECT;

//     // 2. 第一个一级间接块
//     if (bnum < IPTRS) {
//         if (ip->addrs[NDIRECT] == 0 && alloc) {
//             ip->addrs[NDIRECT] = balloc();
//             ip->blocks++;
//             // 清空新分配的间接块
//             memset(sram + ip->addrs[NDIRECT] * BSIZE,
//                    0, BSIZE);
//         }
//         if (ip->addrs[NDIRECT] == 0)
//         {
//             Error("bmap: no 1st indirect block allocated");
//             return 0;
//         }
//         uint *ind1 = (uint *)(sram + ip->addrs[NDIRECT] * BSIZE);
//         if (ind1[bnum] == 0 && alloc) {
//             ind1[bnum] = balloc();
//             ip->blocks++;
//         }
//         if (ind1[bnum] == 0)
//         {
//             Error("bmap: no direct block of 1st indirect block allocated");
//             return 0;
//         }
//         return ind1[bnum];
//     }
//     bnum -= IPTRS;

//     // 3. 第二个一级间接块
//     if (bnum < IPTRS) {
//         if (ip->addrs[NDIRECT+1] == 0 && alloc) {
//             ip->addrs[NDIRECT+1] = balloc();
//             ip->blocks++;
//             memset(sram + ip->addrs[NDIRECT+1] * BSIZE,
//                    0, BSIZE);
//         }
//         if (ip->addrs[NDIRECT+1] == 0)
//         {
//             Error("bmap: no 2nd indirect block allocated");
//             return 0;
//         }
//         uint *ind2 = (uint *)(sram + ip->addrs[NDIRECT+1] * BSIZE);
//         if (ind2[bnum] == 0 && alloc) {
//             ind2[bnum] = balloc();
//             ip->blocks++;
//         }
//         if (ind2[bnum] == 0)
//         {
//             Error("bmap: no direct block of 2nd indirect block allocated");
//             return 0;
//         }
//         return ind2[bnum];
//     }
//     bnum -= IPTRS;

//     // 4. 二级间接块
//     if (bnum < DPTRS) {
//         if (ip->addrs[NDIRECT+2] == 0 && alloc) {
//             ip->addrs[NDIRECT+2] = balloc();
//             ip->blocks++;
//             memset(sram + ip->addrs[NDIRECT+2] * BSIZE,
//                    0, BSIZE);
//         }
//         if (ip->addrs[NDIRECT+2] == 0)
//         {
//             Error("bmap: no 3rd indirect block allocated");
//             return 0;
//         }
//         uint *dbl = (uint *)(sram + ip->addrs[NDIRECT+2] * BSIZE);
//         uint i1 = bnum / IPTRS;
//         uint i2 = bnum % IPTRS;
//         if (dbl[i1] == 0 && alloc) {
//             dbl[i1] = balloc();
//             ip->blocks++;
//             memset(sram + dbl[i1] * BSIZE, 0, BSIZE);
//         }
//         if (dbl[i1] == 0)
//         {
//             Error("bmap: no indirect block of 3rd indirect block allocated");
//             return 0;
//         }
//         uint *ind3 = (uint *)(sram + dbl[i1] * BSIZE);
//         if (ind3[i2] == 0 && alloc) {
//             ind3[i2] = balloc();
//             ip->blocks++;
//         }
//         if(ind3[i2] == 0)
//         {
//             Error("bmap: no direct block of 3rd indirect block allocated");
//             return 0;
//         }
//         return ind3[i2];
//     }

//     Error("bmap: file too large");
//     return 0;
// }

// ----------------------------------------------------------------------------
// bmap: 将 inode 的逻辑块号映射到物理块号，alloc==1 时进行分配
// ----------------------------------------------------------------------------
uint bmap(inode *ip, uint bnum, int alloc) {
    uchar buf[BSIZE];
    // 1. 直接块
    if (bnum < NDIRECT) {
        if (ip->addrs[bnum] == 0 && alloc) {
            ip->addrs[bnum] = allocate_block();
            ip->blocks++;
        }
        return ip->addrs[bnum];
    }
    bnum -= NDIRECT;

    // 2. 一级间接块
    if (bnum < IPTRS) {
        if (ip->addrs[NDIRECT] == 0 && alloc) {
            ip->addrs[NDIRECT] = allocate_block();
            ip->blocks++;
            memset(buf, 0, BSIZE);
            write_block(ip->addrs[NDIRECT], buf);
        }
        if (ip->addrs[NDIRECT] == 0) return 0;
        read_block(ip->addrs[NDIRECT], buf);
        uint *ind = (uint*)buf;
        if (ind[bnum] == 0 && alloc) {
            ind[bnum] = allocate_block();
            ip->blocks++;
            write_block(ip->addrs[NDIRECT], buf);
        }
        return ind[bnum];
    }
    bnum -= IPTRS;

    // 3. 第二个一级间接块
    if (bnum < IPTRS) {
        if (ip->addrs[NDIRECT+1] == 0 && alloc) {
            ip->addrs[NDIRECT+1] = allocate_block();
            ip->blocks++;
            memset(buf, 0, BSIZE);
            write_block(ip->addrs[NDIRECT+1], buf);
        }
        if (ip->addrs[NDIRECT+1] == 0) return 0;
        read_block(ip->addrs[NDIRECT+1], buf);
        uint *ind = (uint*)buf;
        if (ind[bnum] == 0 && alloc) {
            ind[bnum] = allocate_block();
            ip->blocks++;
            write_block(ip->addrs[NDIRECT+1], buf);
        }
        return ind[bnum];
    }
    bnum -= IPTRS;

    // 4. 二级间接块
    if (bnum < DPTRS) {
        if (ip->addrs[NDIRECT+2] == 0 && alloc) {
            ip->addrs[NDIRECT+2] = allocate_block();
            ip->blocks++;
            memset(buf, 0, BSIZE);
            write_block(ip->addrs[NDIRECT+2], buf);
        }
        if (ip->addrs[NDIRECT+2] == 0) return 0;
        read_block(ip->addrs[NDIRECT+2], buf);
        uint *dbl = (uint*)buf;

        uint i1 = bnum / IPTRS;
        uint i2 = bnum % IPTRS;
        if (dbl[i1] == 0 && alloc) {
            dbl[i1] = allocate_block();
            ip->blocks++;
            uchar sub[BSIZE]; memset(sub, 0, BSIZE);
            write_block(dbl[i1], sub);
            write_block(ip->addrs[NDIRECT+2], buf);
        }
        if (dbl[i1] == 0) return 0;
        uchar sub[BSIZE];
        read_block(dbl[i1], sub);
        uint *ind3 = (uint*)sub;
        if (ind3[i2] == 0 && alloc) {
            ind3[i2] = allocate_block();
            ip->blocks++;
            write_block(dbl[i1], sub);
        }
        return ind3[i2];
    }

    Error("bmap: file too large");
    return 0;
}

// 注意：当前版本未对 read_block/write_block 返回值做处理，
// TODO: 为 allocate_block()、I/O 操作添加错误检测及恢复逻辑。


// Remove an inode: free all data blocks and indirect blocks, then free the inode itself
void iremove(inode *ip) {
    if (!ip) {
        Error("iremove: invalid inode");
        return;
    }
    if (ip->nlink != 0 || ip->reference_count != 0) {
        return;
    }

    uchar buf[BSIZE];
    uint blkno;

    // 1. 释放直接块
    for (int i = 0; i < NDIRECT; i++) {
        blkno = ip->addrs[i];
        if (blkno) {
            free_block(blkno);
            ip->blocks--;
            ip->addrs[i] = 0;
        }
    }

    // 2. 释放两个一级间接块
    for (int level = 0; level < 2; level++) {
        int idx = NDIRECT + level;
        blkno = ip->addrs[idx];
        if (blkno) {
            read_block(blkno, buf);
            uint *ind = (uint*)buf;
            for (uint j = 0; j < IPTRS; j++) {
                if (ind[j]) {
                    free_block(ind[j]);
                    ip->blocks--;
                    ind[j] = 0;
                }
            }
            free_block(blkno);
            ip->blocks--;
            ip->addrs[idx] = 0;
        }
    }

    // 3. 释放二级间接块
    blkno = ip->addrs[NDIRECT+2];
    if (blkno) {
        read_block(blkno, buf);
        uint *dbl = (uint*)buf;
        for (uint i1 = 0; i1 < IPTRS; i1++) {
            if (dbl[i1]) {
                read_block(dbl[i1], buf);
                uint *ind = (uint*)buf;
                for (uint i2 = 0; i2 < IPTRS; i2++) {
                    if (ind[i2]) {
                        free_block(ind[i2]);
                        ip->blocks--;
                        ind[i2] = 0;
                    }
                }
                free_block(dbl[i1]);
                ip->blocks--;
                dbl[i1] = 0;
            }
        }
        free_block(blkno);
        ip->blocks--;
        ip->addrs[NDIRECT+2] = 0;
    }

        // 4. 释放 inode 位图（支持多块位图）
    {
        uint inum = ip->inum;
        uchar mapbuf[BSIZE];
        uint bits_per_block = BSIZE * 8;
        // 循环所有 inode 位图块
        for (uint bi = 0; bi < sb.ninodeBlocksBitmapBlocks; bi++) {
            uint map_block = sb.inodeBitmapStart + bi;
            read_block(map_block, mapbuf);
            // 每块内可能包含多个 inode 对应的位
            uint start_idx = bi * bits_per_block;
            uint end_idx = start_idx + bits_per_block;
            if (inum >= start_idx && inum < end_idx) {
                uint local = inum - start_idx;
                uint byte = local / 8;
                uint bit  = local % 8;
                mapbuf[byte] &= ~(1 << bit);
                write_block(map_block, mapbuf);
                break;
            }
        }
    }

    // TODO: 更新 superblock 及其他必要元数据
}



// int readi(inode *ip, uchar *dst, uint off, uint n) {
//     if (ip == NULL || dst == NULL) {
//         Error("readi: Invalid inode or buffer");
//         return -1;
//     }
//     if (n <= 0) {
//         Error("readi: Invalid read size %d", n);
//         return -1;
//     }
//     if (off > ip->size) {
//         // offset已经超出文件末尾
//         Error("readi: offset %d out of range %d", off, ip->size);
//         return -1;
//     }
//     if (off + n > ip->size) {
//         // 读到文件末尾为止
//         Warn("readi: read %d bytes, but only %d bytes left", n, ip->size - off);
//         n = ip->size - off;
//     }

//     uint tot, m;
//     uint blockno;
//     uchar *src;

//     for (tot = 0; tot < n; tot += m, off += m, dst += m) {
//         uint bnum = off / BSIZE;
//         uint boff = off % BSIZE;

//         blockno = bmap(ip, bnum, 0);  // alloc=0，只查找，不分配
//         if (blockno == 0) {
//             break;  // 遇到空块，停止读取
//         }

//         src = sram + blockno * BSIZE;
//         m = BSIZE - boff;
//         if (n - tot < m)
//             m = n - tot;

//         memcpy(dst, src + boff, m);
//     }

//     uint now = (uint)time(NULL);
//     ip->atime = now;
//     iupdate(ip);

//     return tot;
// }



// int writei(inode *ip, uchar *src, uint off, uint n) {
//     if (n <= 0) {
//         Error("writei: Invalid write size %d", n);
//         return -1;
//     }

//     uint blocks_per_indirect = BSIZE / sizeof(uint);
//     uint max_size = BSIZE * (10 + 2 * blocks_per_indirect + blocks_per_indirect * blocks_per_indirect);
//     if (off + n > max_size) {
//         Error("writei: file too large");
//         return -1;
//     }

//     uint tot, m;
//     uint blockno;
//     uchar *dst;
//     for (tot = 0; tot < n; tot += m, off += m, src += m) {
//         uint bnum = off / BSIZE;
//         uint boff = off % BSIZE;

//         blockno = bmap(ip, bnum, 1);  // 注意alloc=1，表示自动分配
//         if (blockno == 0)
//         {
//             Error("writei: failed to map block");
//             return -1;
//         }

//         dst = sram + blockno * BSIZE;
//         m = BSIZE - boff;
//         if (n - tot < m)
//             m = n - tot;
//         memcpy(dst + boff, src, m);
//     }

//     if (off > ip->size) {
//         ip->size = off;
//     }

//     uint now = (uint)time(NULL);
//     ip->mtime = now;
//     ip->ctime = now;

//     iupdate(ip);
//     return tot;
// }


// balloc return a free physical block number of file system, if no free block, return 0
// uint balloc() {
//     // 简化版分配块
//     for (uint b = 0; b < sb.ndataBlocks; b++) {
//         uint byte_index = b / 8;
//         uint bit_index = b % 8;
//         if ((sram[sb.bmapstart * BSIZE + byte_index] & (1 << bit_index)) == 0) {
//             // 找到空闲块
//             sram[sb.bmapstart * BSIZE + byte_index] |= (1 << bit_index);
//             return b + sb.blockStart;
//         }
//     }
//     Error("balloc: no free blocks");
//     return 0;
// }

int readi(inode *ip, uchar *dst, uint off, uint n) {
    if (ip == NULL || dst == NULL) {
        Error("readi: Invalid inode or buffer");
        return -1;
    }
    if (n == 0) {
        return 0;
    }
    if (off > ip->size) {
        Error("readi: offset %u out of range %u", off, ip->size);
        return -1;
    }
    if (off + n > ip->size) {
        Warn("readi: requested %u bytes, but only %u available",
             n, ip->size - off);
        n = ip->size - off;
    }

    uint tot = 0;
    while (tot < n) {
        uint bnum = (off + tot) / BSIZE;
        uint boff = (off + tot) % BSIZE;
        uint to_copy = BSIZE - boff;
        if (to_copy > n - tot) to_copy = n - tot;

        uint blk = bmap(ip, bnum, 0);  // alloc=0
        if (blk == 0) {
            // 稀疏区未分配 → 读取到此结束
            break;
        }

        uchar buf[BSIZE];
        read_block(blk, buf);
        // TODO 这里需要修改read_block加上错误处理
        // if (read_block(blk, buf) < 0) {
        //     Error("readi: read_block %u failed", blk);
        //     return tot;
        // }

        memcpy(dst + tot, buf + boff, to_copy);
        tot += to_copy;
    }

    // 更新访问时间并写回 inode
    ip->atime = (uint)time(NULL);
    iupdate(ip);

    return tot;
}


int writei(inode *ip, uchar *src, uint off, uint n) {
    if (ip == NULL || src == NULL) {
        Error("writei: Invalid arguments");
        return -1;
    }
    if (n == 0) {
        return 0;
    }

    // 计算支持的最大文件大小（同原逻辑）
    uint blocks_per_indirect = BSIZE / sizeof(uint);
    uint max_size = BSIZE * (NDIRECT
                             + 2 * blocks_per_indirect
                             + blocks_per_indirect * blocks_per_indirect);
    if (off + n > max_size) {
        Error("writei: file too large");
        return -1;
    }

    uint tot = 0;
    while (tot < n) {
        uint bnum = (off + tot) / BSIZE;
        uint boff = (off + tot) % BSIZE;
        uint to_copy = BSIZE - boff;
        if (to_copy > n - tot) to_copy = n - tot;

        // 分配或查找物理块
        uint blk = bmap(ip, bnum, 1);  // alloc=1
        if (blk == 0) {
            Error("writei: bmap failed for block %u", bnum);
            return tot;
        }

        // 先读出整个块，再补写局部数据
        uchar buf[BSIZE];
        read_block(blk, buf);
        // TODO 这里需要修改read_block加上错误处理
        // if (read_block(blk, buf) < 0) {
        //     Error("writei: read_block %u failed", blk);
        //     return tot;
        // }

        memcpy(buf + boff, src + tot, to_copy);

        write_block(blk, buf);
        //TODO 修改write_block函数加上错误处理
        // if (write_block(blk, buf) < 0) {
        //     Error("writei: write_block %u failed", blk);
        //     return tot;
        // }

        tot += to_copy;
    }

    // 更新文件大小
    if (off + n > ip->size) {
        ip->size = off + n;
    }

    // 更新修改/创建时间并写回 inode
    uint now = (uint)time(NULL);
    ip->mtime = now;
    ip->ctime = now;
    iupdate(ip);

    return tot;
}

