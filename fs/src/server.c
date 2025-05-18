#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "log.h"
#include "tcp_utils.h"
#include "block.h"     // read_block, write_block
#include "common.h"
#include "fs.h"
#include "tcp_buffer.h"
#include "session.h"
#include "handle.h"

#define NCMD (sizeof(cmd_table) / sizeof(cmd_table[0]))


#define MAX_CONN 1024   // 最大并发连接数

static session_t *sessions[MAX_CONN];

FILE *log_file = NULL;

static struct {
    const char *name;
    int (*handler)(session_t *, char *);
} cmd_table[] = {
    {"f",     handle_f},
    {"mk",    handle_mk},
    {"mkdir", handle_mkdir},
    {"rm",    handle_rm},
    {"rmdir", handle_rmdir},
    {"cd",    handle_cd},
    {"ls",    handle_ls},
    {"cat",   handle_cat},
    {"w",     handle_w},
    {"i",     handle_i},
    {"d",     handle_d},
    {"login", handle_login},
    {"useradd", handle_useradd},
    {"userdel", handle_userdel},
    {"e",     handle_e},
};


void on_connection(int id) {
    sessions[id] = session_create(id);
    Log("Client %d connected", id);
    handle_mount(sessions[id]);
}

void cleanup(int id) {
    session_destroy(sessions[id]);
    sessions[id] = NULL;
    Log("Client %d disconnected", id);
}

int on_recv(int id, tcp_buffer *wb, char *msg, int len) {
    session_t *s = sessions[id];
    s->wb = wb;
    msg[len-2]=0;
    char *cmd  = strtok(msg, " ");
    char *args = strtok(NULL, "");

    bool fs_initialized = fs_is_initialized();
     // 1) 在全局 fs 未初始化前，只允许 root 登录并执行 f
     if (!fs_initialized) {
        if (strcmp(cmd, "login") != 0 && strcmp(cmd, "f") != 0) {
            RN(wb, "Error: filesystem not formatted. Please `login 1` then `f`.");
            return 0;
        }
        if (strcmp(cmd, "login") == 0 && args && atoi(args) != 1) {
            RN(wb, "Error: only root (UID=1) can login before format.");
            return 0;
        }
    }

    // 2) 登录后，只有 root 能跑 f
    if (strcmp(cmd, "f") == 0 && s->uid != 1) {
        RN(wb, "Error: only root can format.");
        return 0;
    }

    // 3) 未登录用户（除了 login）一律拦截
    if (s->uid < 0 && strcmp(cmd, "login") != 0) {
        RN(wb, "Error: please `login <uid>` first.");
        return 0;
    }

    if(s->uid != 1 && ((strcmp(cmd, "useradd") == 0) || (strcmp(cmd, "userdel") == 0))) {
        RN(wb, "Error: only root can add/delete users.");
        return 0;
    }

    // 找 handler
    for (int i = 0; i < NCMD; i++) {
        if (strcmp(cmd, cmd_table[i].name)==0) {
            return cmd_table[i].handler(s, args);
        }
    }
    RN(s->wb, "Unknown command");
    return 0;
}

// main: 先连接磁盘服务器，再启动 FS TCP 服务
int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr,
          "Usage: %s <bds_host> <bds_port> <fs_port> \n", argv[0]);
        exit(1);
    }
    const char *bds_host = argv[1];
    int bds_port        = atoi(argv[2]);
    int fs_port         = atoi(argv[3]);

    log_init("fs.log");

    // 1. 连接到磁盘服务器
    tcp_client bds_client = client_init(bds_host, bds_port);
    bds_fd  = client_fd(bds_client);      
    bds_rbuf = init_buffer();
    bds_wbuf = init_buffer();
    

    // 2. 读取几何信息
    get_disk_info(&ncyl, &nsec);


    // 4. 启动 FS 服务
    tcp_server server = server_init(fs_port, 1,
                                   on_connection, on_recv, cleanup);
    server_run(server);

    // 永不返回
    log_close();
    return 0;
}
