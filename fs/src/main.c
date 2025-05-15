
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "block.h"
#include "common.h"
#include "fs.h"
#include "log.h"


#define ReplyYes()       \
    do {                 \
        printf("Yes\n"); \
        Log("Success");  \
    } while (0)
#define ReplyNo(x)      \
    do {                \
        printf("No\n"); \
        Warn(x);        \
    } while (0)

// return a negative value to exit
// f <nothing>
int handle_f(char *args) {
    (void)args;
    if (cmd_f(ncyl, nsec) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to format");
    }
    return 0;
}

// mk <name> [<mode>]
int handle_mk(char *args) {
    if (!args) {
        ReplyNo("mk needs filename");
        return 0;
    }
    // 解析文件名
    char *name = strtok(args, " ");
    if (!name || !*name) {
        ReplyNo("mk needs filename");
        return 0;
    }
    // 解析可选的 mode（八进制），默认 0644
    char *mstr = strtok(NULL, " ");
    short mode = mstr ? (short)strtol(mstr, NULL, 8) : 0644;

    if (cmd_mk(name, mode) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to create file");
    }
    return 0;
}

// mkdir <name> [<mode>]
int handle_mkdir(char *args) {
    if (!args) {
        ReplyNo("mkdir needs dirname");
        return 0;
    }
    char *name = strtok(args, " ");
    if (!name || !*name) {
        ReplyNo("mkdir needs dirname");
        return 0;
    }
    char *mstr = strtok(NULL, " ");
    short mode = mstr ? (short)strtol(mstr, NULL, 8) : 0755;

    if (cmd_mkdir(name, mode) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to create directory");
    }
    return 0;
}

// rm <name>
int handle_rm(char *args) {
    if (!args) {
        ReplyNo("rm needs filename");
        return 0;
    }
    char *name = strtok(args, " ");
    if (!name || !*name) {
        ReplyNo("rm needs filename");
        return 0;
    }
    if (cmd_rm(name) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to remove file");
    }
    return 0;
}

int handle_rmdir(char *args) {
    char *name = args;
    if (cmd_rmdir(name) == E_SUCCESS)      
        ReplyYes();
    else           
        ReplyNo("Failed to remove directory");                        
    return 0;
}


// cd <dirname>
int handle_cd(char *args) {
    if (!args) {
        ReplyNo("cd needs dirname");
        return 0;
    }
    char *name = strtok(args, " ");
    if (!name || !*name) {
        ReplyNo("cd needs dirname");
        return 0;
    }
    if (cmd_cd(name) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to change directory");
    }
    return 0;
}

// ls  [no args]
int handle_ls(char *args) {
    (void)args;
    entry *entries = NULL;
    int n = 0;
    if (cmd_ls(&entries, &n) != E_SUCCESS) {
        ReplyNo("Failed to list files");
        return 0;
    }
    ReplyYes();
    // 这里只输出名字，若要 size/mtime，可按前面示例再 iget
    for (int i = 0; i < n; i++) {
        printf("%s\n", entries[i].name);
    }
    free(entries);
    return 0;
}

// cat <name>
int handle_cat(char *args) {
    if (!args) {
        ReplyNo("cat needs filename");
        return 0;
    }
    char *name = strtok(args, " ");
    if (!name || !*name) {
        ReplyNo("cat needs filename");
        return 0;
    }
    uchar *buf = NULL;
    uint len = 0;
    if (cmd_cat(name, &buf, &len) == E_SUCCESS) {
        ReplyYes();
        printf("%.*s\n", len, buf);
        free(buf);
    } else {
        ReplyNo("Failed to read file");
    }
    return 0;
}

// w <name> <len> <data>
int handle_w(char *args) {
    if (!args) {
        ReplyNo("w needs name and data");
        return 0;
    }
    char *name = strtok(args, " ");
    char *lstr = strtok(NULL, " ");
    char *data = strtok(NULL, "");
    if (!name || !lstr || !data) {
        ReplyNo("w needs name len data");
        return 0;
    }
    uint len = (uint)atoi(lstr);
    if (cmd_w(name, len, data) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to write file");
    }
    return 0;
}

// i <name> <pos> <len> <data>
int handle_i(char *args) {
    if (!args) {
        ReplyNo("i needs name pos len data");
        return 0;
    }
    char *name = strtok(args, " ");
    char *pstr = strtok(NULL, " ");
    char *lstr = strtok(NULL, " ");
    char *data = strtok(NULL, "");
    if (!name || !pstr || !lstr || !data) {
        ReplyNo("i needs name pos len data");
        return 0;
    }
    uint pos = (uint)atoi(pstr);
    uint len = (uint)atoi(lstr);
    if (cmd_i(name, pos, len, data) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to insert data");
    }
    return 0;
}

// d <name> <pos> <len>
int handle_d(char *args) {
    if (!args) {
        ReplyNo("d needs name pos len");
        return 0;
    }
    char *name = strtok(args, " ");
    char *pstr = strtok(NULL, " ");
    char *lstr = strtok(NULL, " ");
    if (!name || !pstr || !lstr) {
        ReplyNo("d needs name pos len");
        return 0;
    }
    uint pos = (uint)atoi(pstr);
    uint len = (uint)atoi(lstr);
    if (cmd_d(name, pos, len) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to delete data");
    }
    return 0;
}

// login <uid>
int handle_login(char *args) {
    if (!args) {
        ReplyNo("login needs uid");
        return 0;
    }
    int uid = atoi(args);
    if (cmd_login(uid) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to login");
    }
    return 0;
}

// e [no args]
int handle_e(char *args) {
    (void)args;
    printf("Bye!\n");
    Log("Exit");
    return -1;
}

static struct {
    const char *name;
    int (*handler)(char *);
} cmd_table[] = {{"f", handle_f},        {"mk", handle_mk},       {"mkdir", handle_mkdir}, {"rm", handle_rm},
                 {"cd", handle_cd},      {"rmdir", handle_rmdir}, {"ls", handle_ls},       {"cat", handle_cat},
                 {"w", handle_w},        {"i", handle_i},         {"d", handle_d},         {"e", handle_e},
                 {"login", handle_login}};

#define NCMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

FILE *log_file;

int main(int argc, char *argv[]) {
    log_init("fs.log");
    assert(BSIZE % sizeof(dinode) == 0);

    get_disk_info(&ncyl, &nsec);
    sbinit();

    char buf[4096];
    while (fgets(buf, sizeof(buf), stdin)) {
        buf[strcspn(buf, "\r\n")] = 0;       // 去掉换行
        Log("Use command: %s", buf);

        // 分割 cmd 和 args
        char *cmd  = strtok(buf, " \t");
        char *args = strtok(NULL, "");

        int ret = 1;
        if (cmd) {
            for (int i = 0; i < NCMD; i++) {
                if (strcmp(cmd, cmd_table[i].name) == 0) {
                    ret = cmd_table[i].handler(args);
                    break;
                }
            }
        }
        if (ret == 1) {
            Log("No such command");
            printf("No\n");
        }
        if (ret < 0) break;   // 'e' 命令退出
    }

    log_close();
    return 0;
}
