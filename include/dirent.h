/* SPDX-License-Identifier: MIT */
#ifndef __XCC_DIRENT_H
#define __XCC_DIRENT_H

#include <sys/types.h>

#define DT_UNKNOWN 0
#define DT_FIFO    1
#define DT_CHR     2
#define DT_DIR     4
#define DT_BLK     6
#define DT_REG     8
#define DT_LNK     10
#define DT_SOCK    12

struct dirent {
    ino_t d_ino;
    off_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[256];
};

typedef struct __dirstream DIR;

int closedir(DIR *dirp);
int dirfd(DIR *dirp);
DIR *fdopendir(int fd);
DIR *opendir(const char *name);
struct dirent *readdir(DIR *dirp);
void rewinddir(DIR *dirp);
void seekdir(DIR *dirp, long location);
long telldir(DIR *dirp);

#endif
