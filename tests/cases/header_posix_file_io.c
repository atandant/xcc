/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

int main(void)
{
    const char *path = "xcc-posix-file-io.tmp";
    const char text[] = "full zlib";
    char buf[sizeof(text)];
    int fd;

    fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (fd < 0)
        return 1;
    if (write(fd, text, sizeof(text)) != (ssize_t)sizeof(text))
        return 2;
    if (lseek(fd, 0, SEEK_SET) != (off_t)0)
        return 3;
    if (read(fd, buf, sizeof(buf)) != (ssize_t)sizeof(buf))
        return 4;
    if (close(fd) != 0)
        return 5;
    if (remove(path) != 0)
        return 6;
    return memcmp(buf, text, sizeof(text)) != 0;
}
