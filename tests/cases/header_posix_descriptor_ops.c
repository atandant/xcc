/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    int fds[2];
    int copy;
    char buf[5];
    FILE *stream;

    if (pipe(fds) != 0)
        return 1;
    if (fcntl(fds[0], F_SETFD, FD_CLOEXEC) != 0)
        return 2;
    copy = dup(fds[1]);
    if (copy < 0 || write(copy, "pipe", 5) != 5 || close(copy) != 0)
        return 3;
    if (read(fds[0], buf, sizeof(buf)) != 5 || close(fds[0]) != 0)
        return 4;
    stream = fdopen(fds[1], "w");
    if (stream == 0 || fileno(stream) != fds[1] || fclose(stream) != 0)
        return 5;
    return memcmp(buf, "pipe", 5) != 0;
}
