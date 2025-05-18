#include "fs.h"

static bool fs_initialized = false; // 是否已经初始化

// inode *cwd = NULL;
// int current_uid = -1; // -1 means no user logged in
int current_gid = 0; //TODO set gid in function login

void sbinit(int nblocks) {
    sb.magic            = MAGIC_NUM;  
    sb.size             = nblocks;
    sb.ndataBlocksBitmapBlocks = 1; // 1 块数据块 bitmap
    sb.ninodeBlocksBitmapBlocks = 1; // 1 块 inode bitmap

    sb.inodeBitmapStart = 1;      // block 1: inode bitmap
    sb.bmapstart        = sb.inodeBitmapStart + sb.ninodeBlocksBitmapBlocks;      // block 2: data block bitmap
    sb.inodeStart       = sb.bmapstart + sb.ndataBlocksBitmapBlocks;      // block 3~: inode blocks
    sb.ninodeBlocks     = 32;      // 使用 4 块 inode blocks
    sb.blockStart       = sb.inodeStart + sb.ninodeBlocks;  // 数据区起始块

    sb.ndataBlocks      = nblocks - sb.blockStart;
}


int cmd_f(session_t *s, int ncyl, int nsec) {
    uint nblocks = ncyl * nsec;

    // 1) 初始化内存中的超级块 sb
    sbinit(nblocks);

    // 准备一个 0 填充扇区
    uchar zero[BSIZE];
    memset(zero, 0, BSIZE);

    // 2) 写超级块到 block 0
    {
        uchar buf[BSIZE];
        memcpy(buf, &sb, sizeof(superblock));
        write_block(0, buf);
    }

    // 3) 清空 inode 位图区：sb.inodeBitmapStart 开始，共 sb.ninodeBlocksBitmapBlocks 块
    for (uint i = 0; i < sb.ninodeBlocksBitmapBlocks; i++) {
        write_block(sb.inodeBitmapStart + i, zero);
    }

    // 4) 清空数据块位图区：sb.bmapstart 开始，共 sb.ndataBlocksBitmapBlocks 块
    for (uint i = 0; i < sb.ndataBlocksBitmapBlocks; i++) {
        write_block(sb.bmapstart + i, zero);
    }

    // 5) 清空 inode 区：sb.inodeStart 开始，共 sb.ninodeBlocks 块
    for (uint i = 0; i < sb.ninodeBlocks; i++) {
        write_block(sb.inodeStart + i, zero);
    }

    // 6) 初始化内存中的 inode 缓存
    init_inode_cache();

    // 7) 分配根目录 inode
    inode *root = ialloc(T_DIR);
    if (!root) {
        Error("cmd_f: failed to allocate root inode");
        return E_ERROR;
    }
    // 根目录元数据
    root->mode  = 0755;    
    root->uid   = 1;
    root->gid   = 0;
    root->nlink = 1;      // 只为 "." 保留一个链接
    time_t now = time(NULL);
    root->atime = now;
    root->mtime = now;
    root->ctime = now;
    iupdate(root);

    // 8) 向根目录写入 "." 条目
    dir_add(root, ".", T_DIR, root->inum);
    iupdate(root);
    
    // mkdir home which store user's home dir
    inode *home = NULL;
    if (!home) {
        // 在根目录下创建 /home
        inode *root = iget(ROOT_INUM);
        inode *tmp = ialloc(T_DIR);
        if (!tmp) 
        { 
            iput(root);
            Error("user_add: allocate inode fails"); 
            return E_ERROR; 
        }
        tmp->mode = 0755; 
        tmp->uid = 1; // root user 
        tmp->gid = 0; 
        tmp->nlink = 2;
        uint now = (uint)time(NULL);
        tmp->atime = tmp->mtime = tmp->ctime = now;
        iupdate(tmp);
        dir_add(root, "home", T_DIR, tmp->inum);
        dir_add(tmp, ".", T_DIR, tmp->inum);
        dir_add(tmp, "..", T_DIR, root->inum);
        iput(tmp);
        iput(root);
    }

    // fs_mount
    s->cwd = root;

    //changg static variable fs_initialized
    fs_initialized = true;

    return E_SUCCESS;
}

// 每次挂载／初始化都需要做的
void fs_mount(session_t *s) {
    // 1) 先读整个扇区到临时缓冲
    uchar buf[BSIZE];
    read_block(0, buf);

    // 2) 只复制 superblock 结构体本身的大小
    memcpy(&sb, buf, sizeof(sb));

    if (sb.magic != MAGIC_NUM) {
        // fprintf(stderr, "This file system has not been formatted.\n", sb.magic);
        return;
    }
    init_inode_cache();
    // 让每个 session 的 cwd 指向根目录
    inode *root = iget(ROOT_INUM);
    s->cwd = root;
    fs_initialized = true;
    return;
}

bool fs_is_initialized(void) {
    return fs_initialized;
}

int cmd_mk(session_t *s, char *name, short mode) {
    /* Create a new regular file in current directory */
    if (dir_lookup(s->cwd, name, T_FILE)) {
        Error("cmd_mk: file '%s' already exists", name);
        return E_ERROR;
    }

    inode *ip = ialloc(T_FILE);
    if (!ip) 
    {
        Error("cmd_mk: allocate inode fails");
        return E_ERROR;
    }

    /* Initialize inode metadata */
    ip->mode = mode;
    ip->uid = s->uid;
    ip->gid = 0;
    ip->nlink = 1;
    ip->atime = ip->mtime = ip->ctime = (uint)time(NULL);
    iupdate(ip);

    /* Add entry into directory */
    if (dir_add(s->cwd, name, T_FILE, ip->inum) != E_SUCCESS) {
        iput(ip);
        Error("mk: add file %s to cwd fails", name);
        return E_ERROR;
    }

    iput(ip);
    return E_SUCCESS;
}

int cmd_mkdir(session_t *s, char *name, short mode) {
    /* Create a new directory in current directory */
    if (dir_lookup(s->cwd, name, T_DIR)) {
        Error("cmd_mkdir: directory '%s' already exists", name);
        return E_ERROR;
    }

    inode *dp = ialloc(T_DIR);
    if (!dp) 
    {
        Error("cmd_mkdir: alloacte dinode fails");
        return E_ERROR;
    }

    /* Initialize directory inode */
    dp->mode = mode;
    dp->uid = s->uid;
    dp->gid = 0;
    dp->nlink = 2;  // '.' and '..'
    dp->atime = dp->mtime = dp->ctime = (uint)time(NULL);

    /* Add '.' and '..' entries */
    dir_add(dp, ".", T_DIR, dp->inum);
    dir_add(dp, "..", T_DIR, s->cwd->inum);
    iupdate(dp);
    //!debug
    // if(dir_lookup(dp, ".", T_DIR) == NULL)
    // {
    //     printf("dir_add: '.' not found\n");
    // }
    // if(dir_lookup(dp, "..", T_DIR) == NULL)
    // {
    //     printf("dir_add: '..' not found\n");
    // }

    /* Link into parent directory */
    if (!s->cwd)
    {
        Log("cmd_mkdir: cwd is NULL, creating root directory");
    }
    //cwd is NULL which indicates the dir maked is root
    else if (dir_add(s->cwd, name, T_DIR, dp->inum) != E_SUCCESS) {
        Error("mkdir: add subdir %s to cwd fails", name);
        iput(dp);
        return E_ERROR;
    }

    iput(dp);
    return E_SUCCESS;
}

int cmd_rm(session_t *s, char *name) {
    /* Remove a regular file from current directory */
    inode *ip = dir_lookup(s->cwd, name, T_FILE);
    if (!ip) {
        Error("cmd_rm: file '%s' not found", name);
        return E_ERROR;
    }

    if (dir_remove(s->cwd, name, T_FILE) != E_SUCCESS) {
        Error("rm: rm file %s from cwd fails", name);
        iput(ip);
        return E_ERROR;
    }

    /* Decrement link count and possibly remove inode */
    ip->nlink--;
    if (ip->nlink == 0)
        iremove(ip);
    iput(ip);
    return E_SUCCESS;
}

// 把 path 拆成 dirpath（可能是 “” 或 “/a/b”）和 basename（不含斜杠的最后一段）
// 例如 "/foo/bar/baz" -> dirpath="/foo/bar", leaf="baz"
static void split_path(const char *path, char *dirpath, char *leaf) {
    const char *p = strrchr(path, '/');
    if (!p) {
        // 没有斜杠，全是当前目录下的名字
        strcpy(dirpath, "");
        strcpy(leaf, path);
    } 
    else if (p == path) {
        // 形如 "/name"
        strcpy(dirpath, "/");
        strcpy(leaf, path+1);
    } 
    else {
        size_t len = p - path;
        memcpy(dirpath, path, len);
        dirpath[len] = '\0';
        strcpy(leaf, p+1);
    }
}

// 根据一个目录路径（绝对或相对），返回对应的 inode*，失败返回 NULL
static inode *resolve_dir(session_t *s, const char *dirpath) {
    if (dirpath[0] == '\0') {
        // "" 表示当前 cwd
        return iget(s->cwd->inum);
    }
    // 复用刚才写的 cmd_cd 解析逻辑，但不改变全局 cwd
    inode *ip = (dirpath[0]=='/') ? iget(ROOT_INUM) : iget(s->cwd->inum);

    // 跳过开头的 '/'
    const char *rest = dirpath;
    if (*rest=='/') while (*rest=='/') rest++;

    char *save;
    char buf[strlen(rest)+1];
    strcpy(buf, rest);

    for (char *seg = strtok_r(buf, "/", &save); seg; seg = strtok_r(NULL, "/", &save)) {
        if (strcmp(seg, ".")==0) continue;
        if (strcmp(seg, "..")==0) {
            inode *p = dir_lookup(ip, "..", T_DIR);
            iput(ip);
            if (!p) return NULL;
            ip = p;
        } else {
            inode *p = dir_lookup(ip, seg, T_DIR);
            iput(ip);
            if (!p) return NULL;
            ip = p;
        }
    }
    return ip;
}


int cmd_cd(session_t *s, char *path) {
    if (!path) {
        Error("cmd_cd: NULL path");
        return E_ERROR;
    }

    // 1. 如果用户只输入 "/"，直接切换到根目录
    if (strcmp(path, "/") == 0) {
        iput(s->cwd);
        s->cwd = iget(ROOT_INUM);
        if (!s->cwd) return E_ERROR;
        return E_SUCCESS;
    }

    // 2. 拆分出父目录和目标名
    char dirpath[256], leaf[MAXNAME];
    split_path(path, dirpath, leaf);

    // 3. 解析父目录 inode*
    inode *parent = resolve_dir(s, dirpath);
    if (!parent) {
        Error("cmd_cd: cannot resolve directory '%s'", dirpath);
        return E_ERROR;
    }

    // 4. 在 parent 下查找 leaf
    inode *target = NULL;
    if (strcmp(leaf, ".") == 0) {
        target = parent;
    } else if (strcmp(leaf, "..") == 0) {
        target = dir_lookup(parent, "..", T_DIR);
        iput(parent);
        if (!target) {
            Error("cmd_cd: '..' not found under '%s'", dirpath);
            return E_ERROR;
        }
    } else {
        target = dir_lookup(parent, leaf, T_DIR);
        iput(parent);
        if (!target) {
            Error("cmd_cd: directory '%s' not found under '%s'", leaf, dirpath[0] ? dirpath : "/");
            return E_ERROR;
        }
    }

    // 5. 确保它确实是目录
    if (target->type != T_DIR) {
        Error("cmd_cd: '%s' is not a directory", leaf);
        iput(target);
        return E_ERROR;
    }

    // 6. 切换 cwd
    iput(s->cwd);
    s->cwd = target;
    return E_SUCCESS;
}


int cmd_rmdir(session_t *s, char *name) {
    /* Remove an empty directory from current directory */
    inode *ip = dir_lookup(s->cwd, name, T_DIR);
    if (!ip) {
        Error("cmd_rmdir: directory '%s' not found", name);
        return E_ERROR;
    }

    if (!dir_is_empty(ip)) {
        Error("cmd_rmdir: directory '%s' not empty", name);
        iput(ip);
        return E_ERROR;
    }

    if (dir_remove(s->cwd, name, T_DIR) != E_SUCCESS) {
        Error("rmdir: remove subdir %s from cwd fails", name);
        iput(ip);
        return E_ERROR;
    }

    /* Remove '.' and '..' links */
    ip->nlink -= 2;
    if (ip->nlink <= 0)
        iremove(ip);
    iput(ip);
    return E_SUCCESS;
}

// List directory entries; caller frees *entries
int cmd_ls(session_t *s, entry **entries, int *n) {
    if (!entries || !n) return E_ERROR;
    if (s->cwd->type != T_DIR) return E_ERROR;

    // Count entries
    int count = 0;
    entry tmp;
    for (uint off = 0; off < s->cwd->size; off += sizeof(entry)) {
        readi(s->cwd, (uchar*)&tmp, off, sizeof(entry));
        if (tmp.inum == 0 && strncmp(tmp.name, "..", MAXNAME) != 0 && strncmp(tmp.name, ".", MAXNAME) != 0 && strncmp(tmp.name, "/", MAXNAME) != 0) continue;
        if (strncmp(tmp.name, ".", MAXNAME) == 0) continue;
        if (strncmp(tmp.name, "..", MAXNAME) == 0) continue;
        count++;
    }
    *n = count;

    // Allocate array
    entry *arr = malloc(count * sizeof(entry));
    if (!arr) return E_ERROR;

    // printf("cwd->inum: %d, cwd->size: %d\n", cwd->inum, cwd->size);
    // Populate
    int idx = 0;
    for (uint off = 0; off < s->cwd->size; off += sizeof(entry)) {
        readi(s->cwd, (uchar*)&tmp, off, sizeof(entry));
        if (tmp.inum == 0 && strncmp(tmp.name, "..", MAXNAME) != 0 && strncmp(tmp.name, ".", MAXNAME) != 0 && strncmp(tmp.name, "/", MAXNAME) != 0) continue;
        if (strncmp(tmp.name, ".", MAXNAME) == 0) continue;
        if (strncmp(tmp.name, "..", MAXNAME) == 0) continue;
        arr[idx++] = tmp;
        // printf("dir_name: %s, inum: %d, type: %d\n", tmp.name, tmp.inum, tmp.type);
    }
    *entries = arr;
    return E_SUCCESS;
}

// Concatenate file to buffer; caller frees *buf
int cmd_cat(session_t *s, char *name, uchar **buf, uint *len) {
    if (!name || !buf || !len) return E_ERROR;
    inode *ip = dir_lookup(s->cwd, name, T_FILE);
    if (!ip) return E_ERROR;

    uint sz = ip->size;
    uchar *data = malloc(sz);
    if (!data) { iput(ip); return E_ERROR; }

    int r = readi(ip, data, 0, sz);
    iput(ip);
    if (r < 0) { free(data); return E_ERROR; }

    *buf = data;
    *len = r;
    return E_SUCCESS;
}

// Write data starting at offset 0, truncating file
int cmd_w(session_t *s, char *name, uint len, const char *data) {
    if (!name || (!data && len>0)) return E_ERROR;
    inode *ip = dir_lookup(s->cwd, name, T_FILE);
    if (!ip) {
        // create file if not exist
        if (cmd_mk(s, name, 0644) != E_SUCCESS) return E_ERROR;
        ip = dir_lookup(s->cwd, name, T_FILE);
        if (!ip) return E_ERROR;
    }
    // Truncate
    ip->size = 0;
    ip->blocks = 0;
    iupdate(ip);

    int w = writei(ip, (uchar*)data, 0, len);
    iput(ip);
    return (w<0)? E_ERROR : E_SUCCESS;
}

// Insert data at pos, shifting existing
int cmd_i(session_t *s, char *name, uint pos, uint len, const char *data) {
    if (!name || (!data && len>0)) return E_ERROR;
    inode *ip = dir_lookup(s->cwd, name, T_FILE);
    if (!ip) return E_ERROR;

    // Read tail from pos to end
    uint orig_size = ip->size;
    if (pos > orig_size) pos = orig_size;
    uint tail_len = orig_size - pos;
    uchar *tail = malloc(tail_len);
    if (!tail) { iput(ip); return E_ERROR; }
    readi(ip, tail, pos, tail_len);

    // Write new data at pos
    writei(ip, (uchar*)data, pos, len);
    // Write back tail after inserted data
    writei(ip, tail, pos + len, tail_len);
    free(tail);
    iput(ip);
    return E_SUCCESS;
}

// Delete len bytes starting at pos, shifting remaining backward
int cmd_d(session_t *s, char *name, uint pos, uint len) {
    if (!name) return E_ERROR;
    inode *ip = dir_lookup(s->cwd, name, T_FILE);
    if (!ip) return E_ERROR;

    uint orig_size = ip->size;
    if (pos > orig_size) { iput(ip); return E_ERROR; }
    uint tail_pos = pos + len;
    if (tail_pos > orig_size) tail_pos = orig_size;

    uint tail_len = orig_size - tail_pos;
    uchar *tail = malloc(tail_len);
    if (!tail) { iput(ip); return E_ERROR; }
    readi(ip, tail, tail_pos, tail_len);

    // Write tail back at pos
    writei(ip, tail, pos, tail_len);
    free(tail);

    // Truncate file: new size = orig_size - (tail_pos - pos)
    uint new_size = pos + tail_len;
    ip->size = new_size;
    // Optionally free blocks beyond new_size, but skipped here
    iupdate(ip);
    iput(ip);
    return E_SUCCESS;
}

// int cmd_login(session_t *s, int auid) {
    
//     if (auid < 0) {
//         Error("Invalid user ID");
//         return E_NOT_LOGGED_IN;
//     }
//     s->uid = auid;
//     return E_SUCCESS;
// }
