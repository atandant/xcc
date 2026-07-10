/* SPDX-License-Identifier: MIT */
/* expect-warning: conversion from 'int *' to 'void *' without a cast */
/* xcc-args: -Wall */
int main(void) {
    int x = 1;
    int *p = &x;
    void *q = p;
    return q != 0;
}
