#ifndef SUPERBLOCK_H
#define SUPERBLOCK_H

#include "common.h"

typedef struct {
  uint magic;         // Magic number, used to identify the file system
  uint size;          // Size in blocks
  uint ndataBlocks;   // Number of data blocks
  uint ninodeBlocks;  // Number of inode blocks
  uint ndataBlocksBitmapBlocks;   // Number of data block bitmap blocks
  uint ninodeBlocksBitmapBlocks;  // Number of inode block bitmap blocks
  // !assume the number of data block bitmap and inode block bitmap is 1

  uint bmapstart;         // Block number of first free map block
  uint inodeStart;        // Block number of first inode block
  uint inodeBitmapStart;  // Block number of first inode bitmap block
  uint blockStart;        // Block number of first data block

} superblock;

// sb is defined in block.c
extern superblock sb;

#endif  // SB_H
