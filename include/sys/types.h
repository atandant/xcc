/* SPDX-License-Identifier: MIT */
#ifndef __XCC_SYS_TYPES_H
#define __XCC_SYS_TYPES_H

/* Hosted x86-64 Linux types matching the glibc LP64 ABI. */
typedef unsigned long dev_t;
typedef unsigned long ino_t;
typedef unsigned int mode_t;
typedef unsigned long nlink_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef int pid_t;
typedef long off_t;
typedef long ssize_t;
typedef long blksize_t;
typedef long blkcnt_t;

#ifndef __XCC_TIME_T_DEFINED
#define __XCC_TIME_T_DEFINED
typedef long time_t;
#endif

#ifndef __XCC_CLOCK_T_DEFINED
#define __XCC_CLOCK_T_DEFINED
typedef long clock_t;
#endif

#endif
