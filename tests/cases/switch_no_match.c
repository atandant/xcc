/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int main(void)
{
    int result;
    result = 7;
    switch (4) {
    case 1:
        result = 1;
    }
    return result;
}
