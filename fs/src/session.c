// session.c
#include "session.h"
#include <stdlib.h>

session_t *session_create(int conn_id) {
    session_t *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->conn_id = conn_id;
    s->wb      = NULL;
    s->cwd     = NULL;  // 根目录 inode
    s->uid     = -1;               // 未登录
    s->gid     = 0;
    s->umask   = 022;
    pthread_mutex_init(&s->lock, NULL);
    return s;
}

void session_destroy(session_t *s) {
    if (!s) return;
    if (s->cwd) iput(s->cwd);
    pthread_mutex_destroy(&s->lock);
    free(s);
}
