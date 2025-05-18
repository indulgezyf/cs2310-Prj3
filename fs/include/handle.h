// cmd_handlers.h
#ifndef CMD_HANDLERS_H
#define CMD_HANDLERS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "block.h"  // read_block, write_block
#include "common.h"
#include "fs.h"
#include "log.h"
#include "session.h"
#include "tcp_buffer.h"
#include "tcp_utils.h"
#include "user.h"
#include "dirop.h"  // inode, ROOT_INUM, T_DIR

#define RY(wb)                     \
  do {                             \
    reply_with_yes((wb), NULL, 0); \
  } while (0)
#define RN(wb, msg)                          \
  do {                                       \
    reply_with_no((wb), (msg), strlen(msg)); \
  } while (0)
#define Re(wb, msg) \
  do {                                       \
    reply((wb), (msg), strlen(msg));                 \
  } while (0)

int handle_f(session_t *s, char *args);
int handle_mk(session_t *s, char *args);
int handle_mkdir(session_t *s, char *args);
int handle_rm(session_t *s, char *args);
int handle_rmdir(session_t *s, char *args);
int handle_ls(session_t *s, char *args);
int handle_cd(session_t *s, char *args);
int handle_cat(session_t *s, char *args);
int handle_w(session_t *s, char *args);
int handle_i(session_t *s, char *args);
int handle_d(session_t *s, char *args);
int handle_e(session_t *s, char *args);
void handle_mount(session_t *s);
int handle_login(session_t *s, char *args);
int handle_useradd(session_t *s, char *args);
int handle_userdel(session_t *s, char *args);

#endif
