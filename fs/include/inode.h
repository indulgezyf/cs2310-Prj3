#ifndef __INODE_H__
#define __INODE_H__

#include "common.h"

#define NDIRECT 10  // Direct blocks, you can change this value

#define MAXFILEB (NDIRECT + APB + APB * APB)

enum {
    T_DIR = 1,   // Directory
    T_FILE = 2,  // File
};

// You should add more fields
// the size of a dinode must divide BSIZE
// 4+2+2+4*9+52+32 = 128
typedef struct {
    uint inum;             // inode 编号，便于调试和一致性（可选）

    ushort type;           // T_FILE stands on file, T_DIR stands on directory
    ushort nlink;          // 链接数

    uint size;             // 文件字节数
    uint blocks;           // 实际占用块数（可能大于 size）

    uint uid;              // 所有者 UID
    uint gid;              // 所有者 GID
    uint mode;             // 权限和类型标志

    uint atime;            // 最后访问时间
    uint mtime;            // 最后修改时间
    uint ctime;            // 状态改变时间

    uint flags;            // 状态标志（只读/隐藏等）

    uint addrs[NDIRECT + 3]; // 直接块 + 一级间接块 + 二级间接块

    uint reserved[8];      // 保留字段（用于未来拓展）
} dinode;


// inode in memory
// more useful fields can be added, e.g. reference count
//sum = 4+2+2+4*9+52+4+28 = 128
typedef struct {
    uint inum;            // inode 编号 (4 bytes)
    ushort type;          // T_FILE stands on file, T_DIR stands on directory (2 bytes)
    ushort nlink;         // 链接数 (2 bytes)

    uint size;            // 文件大小 (4 bytes)
    uint blocks;          // 占用块数 (4 bytes)

    uint uid;             // 所有者 UID (4 bytes)
    uint gid;             // 所有者 GID (4 bytes)
    uint mode;            // 权限与文件类型标志 (4 bytes)

    uint atime;           // 最后访问时间 (Unix 时间戳, 4 bytes)
    uint mtime;           // 最后修改时间 (4 bytes)
    uint ctime;           // 状态改变时间 (4 bytes)

    uint flags;           // 特殊状态标志，比如只读、不可删除等 (4 bytes)

    uint addrs[NDIRECT + 3]; // 数据块指针 (4 × 13 = 52 bytes)

    uint reference_count; // 引用计数 (4 bytes)

    // 可选留一些reserved空间，比如扩展ACL、大文件支持
    uint reserved[7];     // 4 × 8 = 32bytes
} inode;

// You can change the size of MAXNAME
#define MAXNAME 26

//initialize inode cache
void init_inode_cache();

// Get an inode by number (returns allocated inode or NULL)
// Don't forget to use iput()
inode *iget(uint inum);

// Free an inode (or decrement reference count)
void iput(inode *ip);

// Allocate a new inode of specified type (returns allocated inode or NULL)
// Don't forget to use iput()
inode *ialloc(short type);

// Update disk inode with memory inode contents
void iupdate(inode *ip);

// Read from an inode (returns bytes read or -1 on error)
int readi(inode *ip, uchar *dst, uint off, uint n);

// Write to an inode (returns bytes written or -1 on error)
int writei(inode *ip, uchar *src, uint off, uint n);

// bmap map the logical block number bnum to the physical block number
uint bmap(inode *ip, uint bnum, int alloc);

// Remove an inode: free all data blocks and indirect blocks, then free the inode itself
void iremove(inode *ip);

#endif
