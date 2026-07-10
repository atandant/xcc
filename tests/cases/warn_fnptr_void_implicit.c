/* SPDX-License-Identifier: MIT */
/* expect-warning: conversion between 'int (*)(int)' and 'void *' without a cast */
/* xcc-args: -Wall */
int f(int x) { return x; }
int main(void) {
    int (*fp)(int) = f;
    void *p = fp;
    return p != 0;
}
