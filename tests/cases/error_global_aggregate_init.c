/* SPDX-License-Identifier: MIT */
/* expect-error: file-scope initializer for 'pair' is not yet supported */
struct Pair { int x; int y; };
struct Pair pair = { 1, 2 };
