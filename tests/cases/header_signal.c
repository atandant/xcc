/* SPDX-License-Identifier: MIT */
/* expect: 0 */
/* xcc-args: -Iinclude */
#include <signal.h>

static void ignore_int(int sig)
{
    (void)sig;
}

int main(void)
{
    void (*prev)(int);

    prev = signal(SIGINT, SIG_IGN);
    if (prev == SIG_ERR)
        return 1;
    prev = signal(SIGINT, ignore_int);
    if (prev == SIG_ERR)
        return 2;
    prev = signal(SIGINT, SIG_DFL);
    if (prev == SIG_ERR)
        return 3;
    return 0;
}
