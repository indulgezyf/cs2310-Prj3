// user.c
#include "user.h"

// /home 目录名
static const char *HOME_DIR = "home";

// 内部辅助：获取 /home 目录 inode，不存在就返回 NULL
static inode *get_home_root(void) {
    inode *root = iget(ROOT_INUM);
    inode *home = dir_lookup(root, HOME_DIR, T_DIR);
    iput(root);
    return home;
}

// 新建 /home/<uid> 目录
int user_add(session_t *s, int new_uid) {
    if(new_uid < 0) {
        Error("user_add: invalid user id");
        return E_INVALID_USER;
    }

    // 确保 /home 存在
    inode *home = get_home_root();
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
        dir_add(root, HOME_DIR, T_DIR, tmp->inum);
        dir_add(tmp, ".", T_DIR, tmp->inum);
        dir_add(tmp, "..", T_DIR, root->inum);
        iput(tmp);
        iput(root);
        home = get_home_root();
        if (!home) return E_ERROR;
    }

    // 检查是否已存在该用户目录
    char dirname[16];
    snprintf(dirname, sizeof(dirname), "%d", new_uid);
    if (dir_lookup(home, dirname, T_DIR)) {
        iput(home);
        return E_EXISTS;
    }

    // 创建用户家目录
    inode *ud = ialloc(T_DIR);
    if (!ud) { 
        iput(home); 
        Error("user_add: allocate inode for new user dir fails");
        return E_ERROR; }
    ud->mode = 0700;
    ud->uid  = new_uid;
    ud->gid  = 0;
    ud->nlink = 2;
    time_t now = time(NULL);
    ud->atime = ud->mtime = ud->ctime = now;
    iupdate(ud);
    // “.” 和 “..”
    dir_add(ud, ".", T_DIR, ud->inum);
    dir_add(ud, "..", T_DIR, home->inum);
    // 挂到 /home 下
    dir_add(home, dirname, T_DIR, ud->inum);

    iput(ud);
    iput(home);
    return E_SUCCESS;
}

int user_delete(session_t *s, int target_uid) {
    if (s->uid != 1) return E_PERMISSION_DENIED;
    if (target_uid < 0) {
        Error("user_delete: invalid user id");
        return E_INVALID_USER;
    }

    inode *home = get_home_root();
    if (!home) {
        Error("user_delete: /home not found");
        return E_ERROR;
    }

    char dirname[16];
    snprintf(dirname, sizeof(dirname), "%d", target_uid);
    inode *ud = dir_lookup(home, dirname, T_DIR);
    if (!ud) { 
        iput(home); 
        Error("user_delete: user dir %s not found", dirname);
        return E_USER_NOT_FOUND; 
    }

    // 只允许空目录
    // 这里复用 dir_is_empty
    if (!dir_is_empty(ud)) {
        iput(ud);
        iput(home);
        Error("user_delete: user dir %s not empty", dirname);
        return E_ERROR;
    }
    // 从 /home 目录删除
    dir_remove(home, dirname, T_DIR);
    // 删除 inode
    iput(ud);
    iput(home);
    return E_SUCCESS;
}

int user_login(session_t *s, int uid) {
    // 验证或自动创建家目录
    if(uid < 0) {
        Error("user_login: invalid user id");
        return E_INVALID_USER;
    }

    if(uid == 1)
    {
        // root 用户登录
        s->uid = 1;
        return E_SUCCESS;
    }

    inode *home = get_home_root();
    if (!home) return E_ERROR;

    char dirname[16];
    snprintf(dirname, sizeof(dirname), "%d", uid);
    inode *ud = dir_lookup(home, dirname, T_DIR);
    iput(home);

    if (!ud) {
        // 还没注册，自动创建
        int r = user_add(s, uid);
        if (r != E_SUCCESS) return r;
        home = get_home_root();
        ud = dir_lookup(home, dirname, T_DIR);
        iput(home);
        if (!ud) return E_ERROR;
    }

    // 记录登录状态，cwd 重置到根目录
    s->uid = uid;
    iput(s->cwd);
    s->cwd = iget(ROOT_INUM);
    iput(ud);
    return E_SUCCESS;
}

bool user_check_perm(session_t *s, inode *ip, int perm) {
    if (ip->uid == s->uid) return true;
    // 检查其他用户权限位
    if ((perm & PERM_READ) && (ip->mode & S_IROTH))  perm &= ~PERM_READ;
    if ((perm & PERM_WRITE) && (ip->mode & S_IWOTH)) perm &= ~PERM_WRITE;
    return perm == 0;
}
