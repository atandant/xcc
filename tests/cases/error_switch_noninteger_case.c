/* SPDX-License-Identifier: MIT */
/* expect-error: case label does not have integer type */
int main(void)
{
    switch (0) {
    case (void *)0:
        return 1;
    }
    return 0;
}
