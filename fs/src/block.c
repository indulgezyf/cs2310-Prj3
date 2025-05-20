#include "block.h"

#include <string.h>

#include "common.h"
#include "log.h"
#include "tcp_buffer.h"


superblock sb ;
uchar *sram = NULL; // Simulated disk memory

//*use get_disk_info to get the number of cylinders and sectors
int ncyl; // Number of cylinders
int nsec; // Number of sectors

int bds_fd;
tcp_buffer  *bds_rbuf;   // 只用于接收
tcp_buffer  *bds_wbuf;   // 只用于发送

void zero_block(uint bno) {
    uchar buf[BSIZE];
    memset(buf, 0, BSIZE);
    write_block(bno, buf);
}

// ----------------------------------------------------------------------------
// allocate_block: 从数据块位图中分配一个空闲物理块，返回物理块号
// ----------------------------------------------------------------------------
uint allocate_block() {
    uchar buf[BSIZE];
    // uint bits_per_block = BSIZE * 8;
    // 只有一个数据块位图块时 sb.ndataBlocksBitmapBlocks == 1
    for (uint bbi = 0; bbi < sb.ndataBlocksBitmapBlocks; bbi++) {
        uint blockno = sb.bmapstart + bbi;
        // 读位图块
        read_block(blockno, buf);
        // 扫描每个字节
        for (uint byte = 0; byte < BSIZE; byte++) {
            if (buf[byte] != 0xFF) {
                // 有空闲位
                for (int bit = 0; bit < 8; bit++) {
                    uint idx = byte * 8 + bit;
                    if (idx >= sb.ndataBlocks) break;
                    if ((buf[byte] & (1 << bit)) == 0) {
                        // 标记为已用
                        buf[byte] |= (1 << bit);
                        write_block(blockno, buf);
                        // 返回物理块号
                        return sb.blockStart + idx;
                    }
                }
            }
        }
    }
    Error("allocate_block: no free blocks");
    return 0;
}

void free_block(uint bno) {
    if(bno >= sb.size) {
        Error("free_block: Invalid block number");
        return;
    }
    uint bblock = BBLOCK(bno);
    uint offset = bno % BPB;
    sram[bblock * BSIZE + offset / 8] &= ~(1 << (offset % 8));
    Log("Free block %d", bno);
}

void get_disk_info(int *ncyl, int *nsec) {
    // 1) 发送 “I\n”
    const char *cmd = "I\n";
    buffer_append(bds_wbuf, cmd, strlen(cmd));
    send_buffer(bds_wbuf, bds_fd);

    // 2) 循环接收直到一整条报文到齐
    while (1) {
        int r = read_to_buffer(bds_rbuf, bds_fd);
        if (r < 0) {
            Error("get_disk_info: read error");
            return;
        }
        char *payload;
        int plen = buffer_next(bds_rbuf, &payload);
        if (plen > 0) {
            // 3) 直接解析两个数字
            //    payload 形如 "200 50\n"（plen 字节，不含长度前缀）
            if (sscanf(payload, "%d %d", ncyl, nsec) != 2) {
                Error("get_disk_info: parse failed");
                return;
            }

            // 4) 丢掉这条报文
            recycle_read(bds_rbuf, 4 + plen);
            adjust_buffer(bds_rbuf);

            Log("Disk info: %d cylinders, %d sectors", *ncyl, *nsec);
            return;
        }
        if (plen < 0) {
            Error("get_disk_info: protocol error");
            return;
        }
        // plen == 0: 继续 read_to_buffer
    }
}


/**
 * 通过 R c s\n 从磁盘服务器读 block
 */
void read_block(int blockno, uchar *out_buf) {
    // 验证 blockno 合法性
    if (blockno < 0 || (blockno != 0 &&  blockno >= (int)sb.size)) {
        Error("read_block: blockno %d out of range", blockno);
        return;
    }

    cache_read(blockno, out_buf);
    Log("read_block: blockno %d", blockno);
}

/**
 * 通过 W c s l data\n 写 block
 */
void write_block(int blockno, uchar *in_buf) {
    // 验证 blockno 合法性
    if (blockno < 0 || blockno >= (int)sb.size) {
        Error("write_block: blockno %d out of range", blockno);
        return;
    }

    cache_write(blockno, in_buf);
    Log("write_block: blockno %d", blockno);
}