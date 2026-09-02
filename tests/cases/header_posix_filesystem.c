/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void)
{
    const char *dir = "xcc-posix-dir.tmp";
    const char *path = "xcc-posix-dir.tmp/item";
    struct stat st;
    struct dirent *entry;
    DIR *stream;
    int found = 0;
    int fd;

    unlink(path);
    rmdir(dir);
    if (mkdir(dir, 0700) != 0)
        return 1;
    fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0 || write(fd, "abc", 3) != 3 || close(fd) != 0)
        return 2;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size != 3)
        return 3;
    if (chmod(path, 0640) != 0 || lstat(dir, &st) != 0 || !S_ISDIR(st.st_mode))
        return 4;
    stream = opendir(dir);
    if (stream == 0 || dirfd(stream) < 0)
        return 5;
    while ((entry = readdir(stream)) != 0)
        if (strcmp(entry->d_name, "item") == 0)
            found = 1;
    rewinddir(stream);
    if (closedir(stream) != 0)
        return 6;
    if (unlink(path) != 0 || rmdir(dir) != 0)
        return 7;
    return !found;
}
