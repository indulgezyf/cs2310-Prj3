// session.h
#ifndef SESSION_H
#define SESSION_H

#include <pthread.h>

#include "tcp_buffer.h"
#include "inode.h"
#define ROOT_INUM 0

typedef struct session {
  int conn_id;           // TCP 连接编号
  tcp_buffer *wb;        // 写缓冲
  inode *cwd;            // 当前工作目录 inode
  int uid;               // 已登录用户；-1 表示未登录
  int gid;               // TODO: 群组支持
  int umask;             // 掩码
  pthread_mutex_t lock;  // 若需要并发读写保护
} session_t;

session_t *session_create(int conn_id, tcp_buffer *wb);
void session_destroy(session_t *s);

#endif
