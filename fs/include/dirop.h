#ifndef DIROP_H
#define DIROP_H

#include "common.h"  // Defines E_SUCCESS, E_ERROR, uint
#include "inode.h"
#include "session.h"  // Defines inode, entry, T_DIR, etc.
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "block.h"
#include "log.h"

typedef struct session session_t;

// used for cmd_ls
typedef struct {
  short type;
  uint inum;
  char name[MAXNAME];
  // ...
  // ...
  // Other fields can be added as needed
} entry;

// Add a directory entry to a directory inode
int dir_add(inode *ip, const char *name, short type, uint inum);

// Lookup an entry by name in a directory inode
inode *dir_lookup(inode *ip, const char *name, short expected_type);

// Remove an entry by name from a directory inode
int dir_remove(inode *ip, const char *name, short expected_type);

// Check if a directory inode is empty (excluding "." and "..")
bool dir_is_empty(inode *ip);

#endif  // DIROP_H
