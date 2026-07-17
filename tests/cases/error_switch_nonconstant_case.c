/* SPDX-License-Identifier: MIT */
/* expect-error: case label is not an integer constant expression */
int main(void)
{
    int value;
    value = 1;
    switch (value) {
    case value:
        return 1;
    }
    return 0;
}
