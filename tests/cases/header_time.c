/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <time.h>

int main(void)
{
    struct tm t;
    time_t epoch;
    clock_t ticks;

    ticks = clock();
    if (ticks == (clock_t)-1)
        return 1;
    epoch = time(0);
    if (epoch <= 0)
        return 2;
    t.tm_sec = 0;
    t.tm_min = 0;
    t.tm_hour = 0;
    t.tm_mday = 1;
    t.tm_mon = 0;
    t.tm_year = 100;
    t.tm_isdst = -1;
    if (mktime(&t) <= 0)
        return 3;
    if (difftime(epoch, epoch) != 0.0)
        return 4;
    if (!gmtime(&epoch) || !localtime(&epoch))
        return 5;
    return 0;
}
