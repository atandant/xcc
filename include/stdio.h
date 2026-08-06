/* SPDX-License-Identifier: MIT */
#ifndef __XCC_STDIO_H
#define __XCC_STDIO_H

#include <stddef.h>
#include <stdarg.h>

/* Hosted x86-64 Linux declarations matching the glibc ABI. */
struct _IO_FILE;
typedef struct _IO_FILE FILE;

union __xcc_mbstate_value {
    unsigned int __wch;
    char __wchb[4];
};

typedef struct __xcc_mbstate_t {
    int __count;
    union __xcc_mbstate_value __value;
} __xcc_mbstate_t;

typedef struct _G_fpos_t {
    long __pos;
    __xcc_mbstate_t __state;
} fpos_t;

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2
#define BUFSIZ 8192
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define FOPEN_MAX 16
#define FILENAME_MAX 4096
#define L_tmpnam 20
#define TMP_MAX 238328

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int remove(const char *filename);
int rename(const char *oldname, const char *newname);
FILE *tmpfile(void);
char *tmpnam(char *s);
int fclose(FILE *stream);
int fflush(FILE *stream);
FILE *fopen(const char *filename, const char *mode);
FILE *freopen(const char *filename, const char *mode, FILE *stream);
void setbuf(FILE *stream, char *buf);
int setvbuf(FILE *stream, char *buf, int mode, size_t size);

int fprintf(FILE *stream, const char *format, ...);
int printf(const char *format, ...);
int sprintf(char *s, const char *format, ...);
int vfprintf(FILE *stream, const char *format, va_list arg);
int vprintf(const char *format, va_list arg);
int vsprintf(char *s, const char *format, va_list arg);

int fscanf(FILE *stream, const char *format, ...);
int scanf(const char *format, ...);
int sscanf(const char *s, const char *format, ...);
int vfscanf(FILE *stream, const char *format, va_list arg);
int vscanf(const char *format, va_list arg);
int vsscanf(const char *s, const char *format, va_list arg);

int fgetc(FILE *stream);
int getc(FILE *stream);
int getchar(void);
char *fgets(char *s, int n, FILE *stream);
int fputc(int c, FILE *stream);
int putc(int c, FILE *stream);
int putchar(int c);
int fputs(const char *s, FILE *stream);
int puts(const char *s);
int ungetc(int c, FILE *stream);
char *gets(char *s);

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fgetpos(FILE *stream, fpos_t *pos);
int fseek(FILE *stream, long offset, int whence);
int fsetpos(FILE *stream, const fpos_t *pos);
long ftell(FILE *stream);
void rewind(FILE *stream);
void clearerr(FILE *stream);
int feof(FILE *stream);
int ferror(FILE *stream);
void perror(const char *s);

#endif
