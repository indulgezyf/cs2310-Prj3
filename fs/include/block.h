#ifndef __BLOCK_H__
#define __BLOCK_H__

#include "common.h"
#include "tcp_buffer.h"
#include "superblock.h"
#include "cache.h"

#define MAGIC_NUM 0x20041216  // Magic number for the file system

extern uchar* sram;  // Simulated disk memory
extern int ncyl; // Number of cylinders
extern int nsec; // Number of sectors
extern int bds_fd; // File descriptor for the block device
extern tcp_buffer *bds_rbuf; // Buffer for receiving data
extern tcp_buffer *bds_wbuf; // Buffer for sending data

void zero_block(uint bno);
uint allocate_block();
void free_block(uint bno);

void get_disk_info(int *ncyl, int *nsec);
void read_block(int blockno, uchar *buf);
void write_block(int blockno, uchar *buf);

#endif