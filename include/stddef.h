/* SPDX-License-Identifier: MIT */
#ifndef __XCC_STDDEF_H
#define __XCC_STDDEF_H

typedef long ptrdiff_t;
typedef unsigned long size_t;
typedef int wchar_t;

#define NULL ((void *)0)
#define offsetof(type, member) ((size_t)&(((type *)0)->member))

#endif
