#include "dirop.h"

int dir_add(inode *ip, const char *name, short type, uint inum)
{
    if(ip->type != T_DIR)
    {
        Error("dir_add: ip->type must be T_DIR");
        return E_ERROR;
    }
    
    entry tmp;
    uint off;
    for(off = 0; off < ip->size; off += sizeof(entry))
    {
        readi(ip, (uchar*)&tmp, off, sizeof(entry));
        if(tmp.inum == 0 && tmp.type != T_DIR && strncmp(tmp.name, "/", MAXNAME) == 0)
            break;
    }

    // 构造新的 dirent
    memset(&tmp, 0, sizeof(entry));
    tmp.inum = inum;
    tmp.type = type;
    strncpy(tmp.name, name, MAXNAME - 1);
    tmp.name[MAXNAME - 1] = '\0';
    // if(tmp.name[0] == '.' && tmp.name[1] == '\0')
    // {
    //     printf("dir_add: name is '.'\n");
    // }
    // if(tmp.name[0] == '.' && tmp.name[1] == '.' && tmp.name[2] == '\0')
    // {
    //     printf("dir_add: name is '..'\n");
    // }

    writei(ip, (uchar *)&tmp, off, sizeof(entry));
    return E_SUCCESS;
}

inode *dir_lookup(inode *ip, const char *name, short expected_type) {
    
    if(!ip || !name) {
        Error("dir_lookup: invalid arguments");
        return NULL;
    }

    if (ip->type != T_DIR) {
        Error("dir_lookup: not a directory");
        return NULL;
    }

    // !debug
    // entry tmp1;
    // for (uint off = 0; off < ip->size; off += sizeof(entry)) {
    //     readi(ip, (uchar*)&tmp1, off, sizeof(entry));
    //     // if (tmp1.inum == 0) continue;
    //     printf("dir_name: %s, inum: %d, type: %d\n", tmp1.name, tmp1.inum, tmp1.type);
    // }
    // printf("finish dir_lookup\n");

    entry tmp;
    for (uint off = 0; off < ip->size; off += sizeof(entry)) {
        readi(ip, (uchar*)&tmp, off, sizeof(entry));
        if (tmp.inum == 0 && strncmp(tmp.name, "..", MAXNAME) != 0 && tmp.type != T_DIR) {
            // printf("dir_lookup: name is '/'");
            continue;
        }
        if (strncmp(tmp.name, name, MAXNAME) == 0 && tmp.type == expected_type) {
            return iget(tmp.inum);
        }
    }

    return NULL;
}

int dir_remove(inode *ip, const char *name, short expected_type) {
    if(!ip || !name) {
        Error("dir_remove: invalid arguments");
        return E_ERROR;
    }
    
    if (ip->type != T_DIR) {
        Error("dir_remove: ip->type must be T_DIR");
        return E_ERROR;
    }

    entry tmp;
    for (uint off = 0; off < ip->size; off += sizeof(entry)) {
        readi(ip, (uchar*)&tmp, off, sizeof(entry));
        if (tmp.inum != 0 && strncmp(tmp.name, name, MAXNAME) == 0 && tmp.type == expected_type) {
            memset(&tmp, 0, sizeof(entry));
            writei(ip, (uchar *)&tmp, off, sizeof(entry));
            return E_SUCCESS;
        }
    }

    Error("dir_remove: entry not found");
    return E_ERROR;
}

bool dir_is_empty(inode *ip) {
    if (ip->type != T_DIR) {
        Error("dir_is_empty: ip->type must be T_DIR");
        return false;
    }

    entry tmp;
    for (uint off = 0; off < ip->size; off += sizeof(entry)) {
        readi(ip, (uchar*)&tmp, off, sizeof(entry));
        if (tmp.inum == 0) continue;

        // 跳过 "." 和 ".."
        if (strncmp(tmp.name, ".", MAXNAME) == 0) continue;
        if (strncmp(tmp.name, "..", MAXNAME) == 0) continue;

        return false;  // 发现实际内容
    }

    return true;
}
