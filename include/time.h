/* SPDX-License-Identifier: MIT */
#ifndef __XCC_TIME_H
#define __XCC_TIME_H

#include <stddef.h>

#ifndef __XCC_CLOCK_T_DEFINED
#define __XCC_CLOCK_T_DEFINED
typedef long clock_t;
#endif

#ifndef __XCC_TIME_T_DEFINED
#define __XCC_TIME_T_DEFINED
typedef long time_t;
#endif

#define CLOCKS_PER_SEC 1000000L

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
    /* glibc layout extension; required for libc mktime/localtime compatibility. */
    long int __tm_gmtoff;
    const char *__tm_zone;
};

clock_t clock(void);
double difftime(time_t time1, time_t time0);
time_t mktime(struct tm *timeptr);
time_t time(time_t *timer);
char *asctime(const struct tm *timeptr);
char *ctime(const time_t *timer);
struct tm *gmtime(const time_t *timer);
struct tm *localtime(const time_t *timer);
size_t strftime(char *s, size_t maxsize, const char *format,
                const struct tm *timeptr);

#endif
