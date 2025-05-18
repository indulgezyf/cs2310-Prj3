#include "handle.h"

int handle_f(session_t *s, char *args) {
    tcp_buffer *wb = s->wb;
    if (cmd_f(s, ncyl, nsec) == E_SUCCESS)    RY(wb);
    else                                   RN(wb, "Failed to format");
    return 0;
}

void handle_mount(session_t *s) {
    fs_mount(s);
}

int handle_mk(session_t *s, char *args) {
    tcp_buffer *wb = s->wb;
    // 解析 args
    char *name = args;
    short mode = 0660; //TODO 修改权限在extension里
    // 权限检查：父目录写权限
    if (!user_check_perm(s, s->cwd, PERM_WRITE)) {
        RN(wb, "Permission denied");
        return 0;
    }
    if (cmd_mk(s, name, mode) == E_SUCCESS)   RY(wb);
    else                                   RN(wb, "Failed to create file");
    return 0;
}

int handle_mkdir(session_t *s, char *args) {
    tcp_buffer *wb = s->wb;
    char *name = args;
    short mode = 0770; //TODO 修改权限在extension里
    // 权限检查：父目录写权限
    if (!user_check_perm(s, s->cwd, PERM_WRITE)) {
        RN(wb, "Permission denied");
        return 0;
    }
    if (cmd_mkdir(s, name, mode) == E_SUCCESS) RY(wb);
    else                                    RN(wb, "Failed to create dir");
    return 0;
}

int handle_rm(session_t *s, char *args) {
    tcp_buffer *wb = s->wb;
    char *name = args;
    // 查出文件 inode
    inode *ip = dir_lookup(s->cwd, name, T_FILE);
    if (!ip) {
        RN(wb, "File not found");
        return 0;
    }
    // 权限检查：父目录写权限 & 文件写权限
    if (!user_check_perm(s, s->cwd, PERM_WRITE) ||
        !user_check_perm(s, ip, PERM_WRITE)) {
        iput(ip);
        RN(wb, "Permission denied");
        return 0;
    }
    iput(ip);
    if (cmd_rm(s, name) == E_SUCCESS)         RY(wb);
    else                                   RN(wb, "Failed to remove file");
    return 0;
}

int handle_rmdir(session_t *s, char *args) {
    tcp_buffer *wb = s->wb;
    char *name = args;
    inode *ip = dir_lookup(s->cwd, name, T_DIR);
    if (!ip) {
        RN(wb, "Dir not found");
        return 0;
    }
    // 权限检查：父目录写权限 & 目录写权限
    if (!user_check_perm(s, s->cwd, PERM_WRITE) ||
        !user_check_perm(s, ip, PERM_WRITE)) {
        iput(ip);
        RN(wb, "Permission denied");
        return 0;
    }
    iput(ip);

    if (cmd_rmdir(s, name) == E_SUCCESS)      RY(wb);
    else                                   RN(wb, "Failed to remove dir");
    return 0;
}

int handle_cd(session_t *s, char *args) {
    tcp_buffer *wb = s->wb;
    char *name = args;
    inode *ip = dir_lookup(s->cwd, name, T_DIR);
    if (!ip) {
        RN(wb, "Dir not found");
        return 0;
    }
    // 权限检查：目录可读
    if (!user_check_perm(s, ip, PERM_READ)) {
        iput(ip);
        RN(wb, "Permission denied");
        return 0;
    }
    iput(ip);
    if (cmd_cd(s, name) == E_SUCCESS)         RY(wb);
    else                                   RN(wb, "Failed to change dir");
    return 0;
}

int handle_ls(session_t *s, char *args)
{
    tcp_buffer *wb = s->wb;
    entry *ents = NULL;
    int n = 0;
    char ls_buf[4096];
    int off = 0;
    ls_buf[0] = '\r'; 
    ls_buf[1] = '\n'; 
    off += 2;

     // 权限检查：当前目录可读
    if (!user_check_perm(s, s->cwd, PERM_READ)) {
        RN(wb, "Permission denied");
        return 0;
    }

    /* 1. 读取目录项 */
    if (cmd_ls(s, &ents, &n) != E_SUCCESS) {
        reply_with_no(wb, "Failed to list files", strlen("Failed to list files"));
        return 0;
    }



    /* 3. 遍历目录项并逐行追加到 wb */
    for (int i = 0; i < n; i++) {
        entry *e = &ents[i];
        inode *ip = iget(e->inum);
        if (!ip) continue;
        printf("find file %s, inum %d\n", e->name, ip->inum);

        /* ---------- mtime 是 32bit，无论你用 uint32_t 还是 unsigned int ---------- */
        char timestr[32] = "invalid-time";
        struct tm tmval;

        /* 把 32‑bit 时间戳提到 64‑bit，确保指针尺寸匹配 */
        time_t mt = (time_t)ip->mtime;          // 仅 4→8 字节宽度提升，值不变

        if (localtime_r(&mt, &tmval) != NULL) { // 传的指针是 &mt，绝对合法
            if (strftime(timestr, sizeof(timestr),
                        "%Y-%m-%d %H:%M:%S", &tmval) == 0) {
                strcpy(timestr, "time-err");    // 缓冲不够大才会返回 0
            }
        }
        /* 此时 timestr 就是格式化好的日期，如 "2025-05-15 11:20:30" */

        /* --- 组织一行输出 --- */
        // 构造权限字符串（同前）
        char perm_str[11];
        perm_str[0] = (ip->type == T_DIR) ? 'd' : '-';
        perm_str[1] = (ip->mode & S_IRUSR) ? 'r' : '-';
        perm_str[2] = (ip->mode & S_IWUSR) ? 'w' : '-';
        perm_str[3] = (ip->mode & S_IXUSR) ? 'x' : '-';
        perm_str[4] = (ip->mode & S_IRGRP) ? 'r' : '-';
        perm_str[5] = (ip->mode & S_IWGRP) ? 'w' : '-';
        perm_str[6] = (ip->mode & S_IXGRP) ? 'x' : '-';
        perm_str[7] = (ip->mode & S_IROTH) ? 'r' : '-';
        perm_str[8] = (ip->mode & S_IWOTH) ? 'w' : '-';
        perm_str[9] = (ip->mode & S_IXOTH) ? 'x' : '-';
        perm_str[10] = '\0';

        // 2) 链接数
        uint nlink = ip->nlink;

        // 3) 所有者和用户组。暂时直接用 UID/GID 数字输出，也可以做映射
        //    如果想用名字，需要维护 uid->name, gid->name 的映射表
        uint owner = ip->uid;
        uint group = ip->gid;

        // 4) 文件大小
        uint size = ip->size;

        // 5) 时间字符串 timestr，格式化成 "Mon DD HH:MM" （或年）
        //    你原本的 strftime 格式 "%b %e %H:%M" 就可以了

        // 6) 文件名 e->name

        char line[256];
        int L = snprintf(line, sizeof(line),
            "%s %2u %5u %5u %8u %s %s\r\n",
            perm_str,     // 权限，如 "drwxr-xr-x"
            nlink,        // 链接数
            owner,        // 所有者 UID
            group,        // 用户组 GID
            size,         // 文件大小
            timestr,      // 时间，如 "Mar 31 13:46"
            e->name       // 文件名
        );

        // printf("line: %s, length: %d\n", line, L);
        if (L > 0 && L < (int)sizeof(line)) {
            memcpy(ls_buf + off, line, L);
            off += L;
            if(off >= sizeof(ls_buf)) {
                // 超过缓冲区大小，直接返回
                Error("handle_ls: ls buffer overflow");
                break;
            }
        }

        /* 释放 inode 引用 */
        iput(ip);
    }
    off-=2; // 去掉最后的 \r\n
    reply_with_yes(wb, ls_buf, off);
    free(ents);
    return 0;
}



int handle_cat(session_t *s, char *args) {
    tcp_buffer *wb = s->wb;
    char *name = args;
    inode *ip = dir_lookup(s->cwd, name, T_FILE);
    if (!ip) {
        RN(wb, "File not found");
        return 0;
    }
    // 权限检查：文件可读
    if (!user_check_perm(s, ip, PERM_READ)) {
        iput(ip);
        RN(wb, "Permission denied");
        return 0;
    }
    iput(ip);

    uchar *buf = NULL;
    uint len = 0;
    if (cmd_cat(s, name, &buf, &len) == E_SUCCESS) {
        reply_with_yes(wb, (char*)buf, len);
        free(buf);
    } else {
        RN(wb, "Failed to read file");
    }
    return 0;
}

int handle_w(session_t *s, char *args) {
    tcp_buffer *wb = s->wb;
    // args: "<name> <len> <data>"
    char *name = strtok(args, " ");
    uint len = atoi(strtok(NULL, " "));
    char *data = strtok(NULL, "");
    inode *ip = dir_lookup(s->cwd, name, T_FILE);
    if (!ip) { RN(wb, "File not found"); return 0; }
    // 权限检查：文件可写
    if (!user_check_perm(s, ip, PERM_WRITE)) {
        iput(ip);
        RN(wb, "Permission denied");
        return 0;
    }
    iput(ip);
    if (cmd_w(s, name, len, data) == E_SUCCESS) RY(wb);
    else                                     RN(wb, "Failed to write file");
    return 0;
}

int handle_i(session_t *s, char *args) {
    tcp_buffer *wb = s->wb;
    char *name = strtok(args, " ");
    uint pos = atoi(strtok(NULL, " "));
    uint len = atoi(strtok(NULL, " "));
    char *data = strtok(NULL, "");

    inode *ip = dir_lookup(s->cwd, name, T_FILE);
    if (!ip) { RN(wb, "File not found"); return 0; }
    // 权限检查：文件可写
    if (!user_check_perm(s, ip, PERM_WRITE)) {
        iput(ip);
        RN(wb, "Permission denied");
        return 0;
    }
    iput(ip);

    if (cmd_i(s, name, pos, len, data) == E_SUCCESS) RY(wb);
    else                                          RN(wb, "Failed to insert");
    return 0;
}

int handle_d(session_t *s, char *args) {
    tcp_buffer *wb = s->wb;
    char *name = strtok(args, " ");
    uint pos = atoi(strtok(NULL, " "));
    uint len = atoi(strtok(NULL, " "));

    inode *ip = dir_lookup(s->cwd, name, T_FILE);
    if (!ip) { RN(wb, "File not found"); return 0; }
    // 权限检查：文件可写
    if (!user_check_perm(s, ip, PERM_WRITE)) {
        iput(ip);
        RN(wb, "Permission denied");
        return 0;
    }
    iput(ip);

    if (cmd_d(s, name, pos, len) == E_SUCCESS)       RY(wb);
    else                                          RN(wb, "Failed to delete");
    return 0;
}

// int handle_login(session_t *s, char *args) {  
//     tcp_buffer *wb = s->wb;
//     int uid = atoi(args);
//     if (cmd_login(s, uid) == E_SUCCESS)  RY(wb);
//     else                              RN(wb, "Failed to login");
//     return 0;
// }

int handle_e(session_t *s, char *args) {
    tcp_buffer *wb = s->wb;
    reply_with_yes(wb, "Bye!\r\n", 6);
    return -1;
}

int handle_useradd(session_t *s, char *args) {
    tcp_buffer *wb = s->wb;
    if (!args) {
        RN(wb, "Usage: useradd <uid>");
        return 0;
    }
    int uid = atoi(args);
    int r = user_add(s, uid);
    if (r == E_SUCCESS)       RY(wb);
    else if (r == E_PERMISSION_DENIED) RN(wb, "Permission denied");
    else if (r == E_EXISTS)            RN(wb, "User already exists");
    else if (r == E_INVALID_USER)      RN(wb, "Invalid user ID");
    else                               RN(wb, "Failed to add user");
    return 0;
}

int handle_userdel(session_t *s, char *args) {
    tcp_buffer *wb = s->wb;
    if (!args) {
        RN(wb, "Usage: userdel <uid>");
        return 0;
    }
    int uid = atoi(args);
    int r = user_delete(s, uid);
    if (r == E_SUCCESS)            RY(wb);
    else if (r == E_PERMISSION_DENIED) RN(wb, "Permission denied");
    else if (r == E_ERROR)            RN(wb, "Delete failed, may be not empty");
    else if (r == E_INVALID_USER)     RN(wb, "Invalid user ID");
    else                               RN(wb, "User not found");
    return 0;
}

int handle_login(session_t *s, char *args) {
    tcp_buffer *wb = s->wb;
    if (!args) {
        RN(wb, "Usage: login <uid>");
        return 0;
    }
    int uid = atoi(args);
    int r = user_login(s, uid);
    // printf("uid: %d, r: %d\n", uid, r);
    if (r == E_SUCCESS)       RY(wb);
    else if (r == E_ERROR)    RN(wb, "Login failed");
    else if (r == E_INVALID_USER) RN(wb, "Invalid user ID");
    else                      RN(wb, "Unknown error");
    return 0;
}

// // -- 以下 handler 调用 fs-layer cmd_* 函数 --
// // 返回 <0 表示断开，0 表示继续

// int handle_f(int id, tcp_buffer *wb, char *args) {
//     if (cmd_f(ncyl, nsec) == E_SUCCESS)    RY(wb);
//     else                                   RN(wb, "Failed to format");
//     return 0;
// }

// int handle_mk(int id, tcp_buffer *wb, char *args) {
//     // 解析 args
//     char *name = args;
//     short mode = 0666; //TODO 修改权限在extension里
//     if (cmd_mk(name, mode) == E_SUCCESS)   RY(wb);
//     else                                   RN(wb, "Failed to create file");
//     return 0;
// }

// int handle_mkdir(int id, tcp_buffer *wb, char *args) {
//     char *name = args;
//     short mode = 0777; //TODO 修改权限在extension里
//     if (cmd_mkdir(name, mode) == E_SUCCESS) RY(wb);
//     else                                    RN(wb, "Failed to create dir");
//     return 0;
// }

// int handle_rm(int id, tcp_buffer *wb, char *args) {
//     char *name = args;
//     if (cmd_rm(name) == E_SUCCESS)         RY(wb);
//     else                                   RN(wb, "Failed to remove file");
//     return 0;
// }

// int handle_rmdir(int id, tcp_buffer *wb, char *args) {
//     char *name = args;
//     if (cmd_rmdir(name) == E_SUCCESS)      RY(wb);
//     else                                   RN(wb, "Failed to remove dir");
//     return 0;
// }

// int handle_cd(int id, tcp_buffer *wb, char *args) {
//     char *name = args;
//     if (cmd_cd(name) == E_SUCCESS)         RY(wb);
//     else                                   RN(wb, "Failed to change dir");
//     return 0;
// }

// int handle_ls(int id, tcp_buffer *wb, char *args)
// {
//     entry *ents = NULL;
//     int n = 0;
//     char ls_buf[4096];
//     int off = 0;
//     ls_buf[0] = '\r'; 
//     ls_buf[1] = '\n'; 
//     off += 2;

//     /* 1. 读取目录项 */
//     if (cmd_ls(&ents, &n) != E_SUCCESS) {
//         reply_with_no(wb, "Failed to list files", strlen("Failed to list files"));
//         return 0;
//     }



//     /* 3. 遍历目录项并逐行追加到 wb */
//     for (int i = 0; i < n; i++) {
//         entry *e = &ents[i];
//         inode *ip = iget(e->inum);
//         if (!ip) continue;
//         printf("find file %s, inum %d\n", e->name, ip->inum);

//         /* ---------- mtime 是 32bit，无论你用 uint32_t 还是 unsigned int ---------- */
//         char timestr[32] = "invalid-time";
//         struct tm tmval;

//         /* 把 32‑bit 时间戳提到 64‑bit，确保指针尺寸匹配 */
//         time_t mt = (time_t)ip->mtime;          // 仅 4→8 字节宽度提升，值不变

//         if (localtime_r(&mt, &tmval) != NULL) { // 传的指针是 &mt，绝对合法
//             if (strftime(timestr, sizeof(timestr),
//                         "%Y-%m-%d %H:%M:%S", &tmval) == 0) {
//                 strcpy(timestr, "time-err");    // 缓冲不够大才会返回 0
//             }
//         }
//         /* 此时 timestr 就是格式化好的日期，如 "2025-05-15 11:20:30" */

//         /* --- 组织一行输出 --- */
//         char line[160];
//         /* %-16.16s：左对齐，最长 16 字符，可根据 MAXNAME 调整 */
//         int L = snprintf(line, sizeof(line),
//                          "%-16.16s %10u  %s\r\n",
//                          e->name,
//                          ip->size,
//                          timestr);

//         // printf("line: %s, length: %d\n", line, L);
//         if (L > 0 && L < (int)sizeof(line)) {
//             memcpy(ls_buf + off, line, L);
//             off += L;
//             if(off >= sizeof(ls_buf)) {
//                 // 超过缓冲区大小，直接返回
//                 Error("handle_ls: ls buffer overflow");
//                 break;
//             }
//         }

//         /* 释放 inode 引用 */
//         iput(ip);
//     }
//     off-=2; // 去掉最后的 \r\n
//     reply_with_yes(wb, ls_buf, off);
//     free(ents);
//     return 0;
// }



// int handle_cat(int id, tcp_buffer *wb, char *args) {
//     char *name = args;
//     uchar *buf = NULL;
//     uint len = 0;
//     if (cmd_cat(name, &buf, &len) == E_SUCCESS) {
//         reply_with_yes(wb, (char*)buf, len);
//         free(buf);
//     } else {
//         RN(wb, "Failed to read file");
//     }
//     return 0;
// }

// int handle_w(int id, tcp_buffer *wb, char *args) {
//     // args: "<name> <len> <data>"
//     char *name = strtok(args, " ");
//     uint len = atoi(strtok(NULL, " "));
//     char *data = strtok(NULL, "");
//     if (cmd_w(name, len, data) == E_SUCCESS) RY(wb);
//     else                                     RN(wb, "Failed to write file");
//     return 0;
// }

// int handle_i(int id, tcp_buffer *wb, char *args) {
//     char *name = strtok(args, " ");
//     uint pos = atoi(strtok(NULL, " "));
//     uint len = atoi(strtok(NULL, " "));
//     char *data = strtok(NULL, "");
//     if (cmd_i(name, pos, len, data) == E_SUCCESS) RY(wb);
//     else                                          RN(wb, "Failed to insert");
//     return 0;
// }

// int handle_d(int id, tcp_buffer *wb, char *args) {
//     char *name = strtok(args, " ");
//     uint pos = atoi(strtok(NULL, " "));
//     uint len = atoi(strtok(NULL, " "));
//     if (cmd_d(name, pos, len) == E_SUCCESS)       RY(wb);
//     else                                          RN(wb, "Failed to delete");
//     return 0;
// }

// int handle_login(int id, tcp_buffer *wb, char *args) {
//     int uid = atoi(args);
//     if (cmd_login(uid) == E_SUCCESS)  RY(wb);
//     else                              RN(wb, "Failed to login");
//     return 0;
// }

// int handle_e(int id, tcp_buffer *wb, char *args) {
//     reply_with_yes(wb, "Bye!\r\n", 6);
//     return -1;
// }