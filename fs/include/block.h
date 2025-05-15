#ifndef __BLOCK_H__
#define __BLOCK_H__

#include "common.h"
#include "tcp_buffer.h"

typedef struct {
    uint magic;      // Magic number, used to identify the file system
    uint size;       // Size in blocks
    uint ndataBlocks;   // Number of data blocks
    uint ninodeBlocks;     // Number of inode blocks
    uint ndataBlocksBitmapBlocks; // Number of data block bitmap blocks
    uint ninodeBlocksBitmapBlocks; // Number of inode block bitmap blocks
    // !assume the number of data block bitmap and inode block bitmap is 1

    uint bmapstart;  // Block number of first free map block
    uint inodeStart;  // Block number of first inode block
    uint inodeBitmapStart;  // Block number of first inode bitmap block
    uint blockStart;  // Block number of first data block

} superblock;

// sb is defined in block.c
extern superblock sb;
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