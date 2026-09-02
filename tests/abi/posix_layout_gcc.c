/* SPDX-License-Identifier: MIT */
#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

int check_posix_types(size_t a, size_t b, size_t c, size_t d, size_t e,
                      size_t f, size_t g, size_t h, size_t i, size_t j,
                      size_t k)
{
    return a != sizeof(dev_t) || b != sizeof(ino_t) || c != sizeof(mode_t) ||
        d != sizeof(nlink_t) || e != sizeof(uid_t) || f != sizeof(gid_t) ||
        g != sizeof(pid_t) || h != sizeof(off_t) || i != sizeof(ssize_t) ||
        j != sizeof(blksize_t) || k != sizeof(blkcnt_t);
}

int check_stat_layout(size_t a, size_t b, size_t c, size_t d, size_t e,
                      size_t f, size_t g, size_t h, size_t i, size_t j,
                      size_t k, size_t l)
{
    return a != sizeof(struct stat) || b != offsetof(struct stat, st_dev) ||
        c != offsetof(struct stat, st_ino) ||
        d != offsetof(struct stat, st_nlink) ||
        e != offsetof(struct stat, st_mode) ||
        f != offsetof(struct stat, st_rdev) ||
        g != offsetof(struct stat, st_size) ||
        h != offsetof(struct stat, st_blocks) ||
        i != offsetof(struct stat, st_atim) ||
        j != offsetof(struct stat, st_mtim) ||
        k != offsetof(struct stat, st_ctim) ||
        l != offsetof(struct stat, __glibc_reserved);
}

int check_dirent_layout(size_t a, size_t b, size_t c, size_t d, size_t e)
{
    return a != sizeof(struct dirent) || b != offsetof(struct dirent, d_ino) ||
        c != offsetof(struct dirent, d_off) ||
        d != offsetof(struct dirent, d_type) ||
        e != offsetof(struct dirent, d_name);
}

int check_flock_layout(size_t a, size_t b, size_t c, size_t d, size_t e,
                       size_t f)
{
    return a != sizeof(struct flock) || b != offsetof(struct flock, l_type) ||
        c != offsetof(struct flock, l_whence) ||
        d != offsetof(struct flock, l_start) ||
        e != offsetof(struct flock, l_len) ||
        f != offsetof(struct flock, l_pid);
}
