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

// uint allocate_block() {
//     uint bno = 0;
//     unsigned long offset = sb.bmapstart * BSIZE;
//     uint nblock = 0;
//     //if bf.size is not a multiple of 8
//     while (sram[offset] == 0xFF) {
//         offset++;
//         nblock+=8;
//     }
//     uint i = 0;
//     while (i < 8) {
//         if ((sram[offset] & (1 << i)) == 0) {
//             sram[offset] |= (1 << i);
//             bno = nblock + i;
//             break;
//         }
//         i++;
//     }
//     if(bno >= sb.size) {
//         Error("allocate_block: No free block");
//         return INVALID_BLOCK;
//     }
//     return bno;
// }

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

// void get_disk_info(int *ncyl, int *nsec) {
//     char *I_commad = "I\n";
//     buffer_append(bds_buf, I_commad, strlen(I_commad)+1);
//     send_buffer(bds_buf, bds_fd);
//     int n = read_to_buffer(bds_buf, bds_fd);
//     if(n < 0) {
//         Error("get_disk_info: Failed to read disk info");
//         return;
//     }
//     char *data = bds_buf->buf + bds_buf->read_index;
//     printf("data: %s\n", data);
//     if(sscanf(data, "%d %d", ncyl, nsec) != 2) {
//         Error("get_disk_info: Failed to parse disk info");
//         return;
//     }
//     Log("Disk info: %d cylinders, %d sectors", *ncyl, *nsec);

// }

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
    if (blockno < 0 || blockno >= (int)sb.size) {
        Error("read_block: blockno %d out of range", blockno);
        return;
    }

    // 1) 发送 "R cyl sec\n"
    char cmd[32];
    int L = snprintf(cmd, sizeof(cmd), "R %d %d\n",
                     blockno / nsec, blockno % nsec);
    if (L <= 0 || L >= (int)sizeof(cmd)) {
        Error("read_block: snprintf failed");
        return;
    }
    buffer_append(bds_wbuf, cmd, L);
    send_buffer(bds_wbuf, bds_fd);
    // send_buffer 已经把写缓冲清空

    // 2) 接收并拆包
    while (1) {
        int r = read_to_buffer(bds_rbuf, bds_fd);
        if (r < 0) {
            Error("read_block: read_to_buffer failed");
            return;
        }
        char *payload;
        int plen = buffer_next(bds_rbuf, &payload);
        if (plen > 0) {
            // payload 应以 "Yes " 开头，后面跟 512 字节
            if (plen < 4 + BSIZE || strncmp(payload, "Yes ", 4) != 0) {
                Error("read_block: unexpected reply");
                return;
            }
            memcpy(out_buf, payload + 4, BSIZE);
            recycle_read(bds_rbuf, 4 + plen);
            adjust_buffer(bds_rbuf);
            Log("Read block %d via disk server", blockno);
            return;
        }
        if (plen < 0) {
            Error("read_block: protocol error");
            return;
        }
        // plen == 0: 继续循环 recv
    }
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

    /* 1) 先把整条命令拼进一个临时缓冲 */
    char msg[64 + BSIZE + 2];      // 够放头+512字节+换行
    int pos = snprintf(msg, sizeof(msg),"W %d %d %d ", blockno / nsec, blockno % nsec, BSIZE);
    memcpy(msg + pos, in_buf, BSIZE);
    pos += BSIZE;
    msg[pos++] = '\n';
    /* 2) 只调用一次 buffer_append —— 这样整个指令带一个长度前缀 */
    buffer_append(bds_wbuf, msg, pos);
    send_buffer(bds_wbuf, bds_fd);         // 写缓冲立即被清空

    // 2) 接收并拆包确认
    while (1) {
        int r = read_to_buffer(bds_rbuf, bds_fd);
        if (r < 0) {
            Error("write_block: read_to_buffer failed");
            return;
        }
        char *payload;
        int plen = buffer_next(bds_rbuf, &payload);
        if (plen > 0) {
            // payload 应是 "Yes"
            // printf("payload: %s\n", payload);
            if (plen >= 3 && strncmp(payload, "Yes", 3) == 0) {
                Log("Write block %d via disk server", blockno);
            } else {
                Error("write_block: unexpected reply");
            }
            recycle_read(bds_rbuf, 4 + plen);
            adjust_buffer(bds_rbuf);
            return;
        }
        if (plen < 0) {
            Error("write_block: protocol error");
            return;
        }
        // plen == 0: 继续循环
    }
}

// void read_block(int blockno, uchar *buf) {
//     //!assume the size of the disk is 2048 blocks in sram
//     // sb.size = 2048;  // Simulated size of the disk
//     if (blockno < 0 || blockno >= sb.size) {
//         Error("read_block: Invalid block number");
//         return;
//     }
//     long offset = blockno * BSIZE;
//     memcpy(buf, sram + offset, BSIZE);
//     Log("Read block %d", blockno);
    
// }

// void write_block(int blockno, uchar *buf) {
//     //!assume the size of the disk is 2048 blocks in sram
//     // sb.size = 2048;  // Simulated size of the disk
//     if (blockno < 0 || blockno >= sb.size) {
//         Error("write_block: Invalid block number");
//         return;
//     }
//     long offset = blockno * BSIZE;
//     memcpy(sram + offset, buf, BSIZE);
//     Log("Write block %d", blockno);
// }
