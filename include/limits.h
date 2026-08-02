/* SPDX-License-Identifier: MIT */
#ifndef __XCC_LIMITS_H
#define __XCC_LIMITS_H

/* x86-64 Linux LP64 hosted environment (C89). */

#define CHAR_BIT    8

#define SCHAR_MIN   (-128)
#define SCHAR_MAX   127
#define UCHAR_MAX   255

#define SHRT_MIN    (-32768)
#define SHRT_MAX    32767
#define USHRT_MAX   65535

#define INT_MIN     (-2147483647 - 1)
#define INT_MAX     2147483647
#define UINT_MAX    4294967295U

#define LONG_MIN    (-9223372036854775807L - 1)
#define LONG_MAX    9223372036854775807L
#define ULONG_MAX   18446744073709551615UL

#define MB_LEN_MAX  16

#endif
