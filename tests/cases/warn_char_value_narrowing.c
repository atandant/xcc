/* SPDX-License-Identifier: MIT */
/* expect-warning: conversion from 'int' to 'char' may alter value */
/* xcc-args: -Wall */
int main(void) {
    int x = 300;
    char c = x;
    return (int)c;
}
