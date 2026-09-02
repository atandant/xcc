/* SPDX-License-Identifier: MIT */
#ifndef __XCC_UNISTD_H
#define __XCC_UNISTD_H

#include <stddef.h>
#include <sys/types.h>

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

int access(const char *path, int mode);
int chdir(const char *path);
int chown(const char *path, uid_t owner, gid_t group);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int close(int fd);
int dup(int fd);
int dup2(int oldfd, int newfd);
int fchdir(int fd);
int fchown(int fd, uid_t owner, gid_t group);
int fsync(int fd);
int ftruncate(int fd, off_t length);
char *getcwd(char *buf, size_t size);
int isatty(int fd);
int link(const char *oldpath, const char *newpath);
off_t lseek(int fd, off_t offset, int whence);
int pipe(int pipefd[2]);
ssize_t pread(int fd, void *buf, size_t count, off_t offset);
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);
ssize_t readlink(const char *path, char *buf, size_t bufsiz);
int rmdir(const char *path);
int symlink(const char *target, const char *linkpath);
int truncate(const char *path, off_t length);
int unlink(const char *path);

#endif
