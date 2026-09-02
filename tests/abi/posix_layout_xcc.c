/* SPDX-License-Identifier: MIT */
/* expect: 0 */
/* abi-role: caller */
/* abi-peer: posix_layout_gcc.c */
#include <stddef.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

int check_posix_types(size_t, size_t, size_t, size_t, size_t, size_t,
                      size_t, size_t, size_t, size_t, size_t);
int check_stat_layout(size_t, size_t, size_t, size_t, size_t, size_t,
                      size_t, size_t, size_t, size_t, size_t, size_t);
int check_dirent_layout(size_t, size_t, size_t, size_t, size_t);
int check_flock_layout(size_t, size_t, size_t, size_t, size_t, size_t);

int main(void)
{
    int bad = 0;

    bad += check_posix_types(sizeof(dev_t), sizeof(ino_t), sizeof(mode_t),
        sizeof(nlink_t), sizeof(uid_t), sizeof(gid_t), sizeof(pid_t),
        sizeof(off_t), sizeof(ssize_t), sizeof(blksize_t), sizeof(blkcnt_t));
    bad += check_stat_layout(sizeof(struct stat),
        offsetof(struct stat, st_dev), offsetof(struct stat, st_ino),
        offsetof(struct stat, st_nlink), offsetof(struct stat, st_mode),
        offsetof(struct stat, st_rdev), offsetof(struct stat, st_size),
        offsetof(struct stat, st_blocks), offsetof(struct stat, st_atim),
        offsetof(struct stat, st_mtim), offsetof(struct stat, st_ctim),
        offsetof(struct stat, __glibc_reserved));
    bad += check_dirent_layout(sizeof(struct dirent),
        offsetof(struct dirent, d_ino), offsetof(struct dirent, d_off),
        offsetof(struct dirent, d_type), offsetof(struct dirent, d_name));
    bad += check_flock_layout(sizeof(struct flock),
        offsetof(struct flock, l_type), offsetof(struct flock, l_whence),
        offsetof(struct flock, l_start), offsetof(struct flock, l_len),
        offsetof(struct flock, l_pid));
    return bad;
}
