/* SPDX-License-Identifier: MIT */
#ifndef __XCC_ASSERT_H
#define __XCC_ASSERT_H

#include <stdlib.h>

#undef assert
#ifdef NDEBUG
#define assert(ignore) ((void)0)
#else
#define assert(expr) ((void)((expr) || (abort(), 0)))
#endif

#endif
