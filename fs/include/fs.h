#ifndef __FS_H__
#define __FS_H__

#include "common.h"
#include "inode.h"
#include "session.h"
#include "dirop.h"
#include <stdbool.h>

typedef struct session session_t;

// used for cmd_ls
// typedef struct {
//     short type;
//     uint inum;
//     char name[MAXNAME];
//     // ...
//     // ...
//     // Other fields can be added as needed
// } entry;

extern inode* cwd; // current work dir
extern int current_uid; // Current user ID
extern int current_gid;

void sbinit(int nblocks);
void fs_mount(session_t *s);

int cmd_f(session_t *s, int ncyl, int nsec);

// int dir_add(inode *ip, const char *name, short type, uint inum);
// inode *dir_lookup(inode *ip, const char *name, short expected_type);
// int dir_remove(inode *ip, const char *name, short expected_type);
// bool dir_is_empty(inode *ip);


int cmd_mk(session_t *s, char *name, short mode);
int cmd_mkdir(session_t *s, char *name, short mode);
int cmd_rm(session_t *s, char *name);
int cmd_rmdir(session_t *s, char *name);

int cmd_cd(session_t *s, char *name);
int cmd_ls(session_t *s, entry **entries, int *n);

int cmd_cat(session_t *s, char *name, uchar **buf, uint *len);
int cmd_w(session_t *s, char *name, uint len, const char *data);
int cmd_i(session_t *s, char *name, uint pos, uint len, const char *data);
int cmd_d(session_t *s, char *name, uint pos, uint len);

int cmd_login(session_t *s, int auid);

#endif