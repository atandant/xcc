/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <stdio.h>

int main(void)
{
    if (EOF != -1 || BUFSIZ != 8192 || FOPEN_MAX != 16)
        return 1;
    if (FILENAME_MAX != 4096 || L_tmpnam != 20 || TMP_MAX != 238328)
        return 2;
    if (SEEK_SET != 0 || SEEK_CUR != 1 || SEEK_END != 2)
        return 3;
    if (_IOFBF != 0 || _IOLBF != 1 || _IONBF != 2)
        return 4;
    if (sizeof(fpos_t) != 16)
        return 5;
    return NULL != 0;
}
