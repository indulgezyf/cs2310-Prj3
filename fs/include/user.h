// user.h
#ifndef USER_H
#define USER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "block.h"
#include "log.h"
#include <stdbool.h>
#include "session.h"
#include "common.h"   // for E_SUCCESS, etc
#include "dirop.h"       // for inode, ROOT_INUM, T_DIR

// 只允许 s->uid==1 的超级用户做这两个操作
int user_add(session_t *s, int new_uid);
int user_delete(session_t *s, int target_uid);

// 普通用户登录；如果 home 目录不存在会自动创建
int user_login(session_t *s, int uid);

// 权限检查，perm 可为以下任意组合
#define PERM_READ  0x1   // 判断 ip->mode 的 S_IROTH
#define PERM_WRITE 0x2   // 判断 ip->mode 的 S_IWOTH
// Owner permissions (high 3 bits)
#define S_IRUSR  0400   // owner has read permission
#define S_IWUSR  0200   // owner has write permission
#define S_IXUSR  0100   // owner has execute permission

// Group permissions
#define S_IRGRP  0040   // group has read permission
#define S_IWGRP  0020   // group has write permission
#define S_IXGRP  0010   // group has execute permission

// Other (world) permissions
#define S_IROTH  0004   // others have read permission
#define S_IWOTH  0002   // others have write permission
#define S_IXOTH  0001   // others have execute permission

bool user_check_perm(session_t *s, inode *ip, int perm);

#endif // USER_H
