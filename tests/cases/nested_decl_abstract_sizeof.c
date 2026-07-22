/* SPDX-License-Identifier: MIT */
/* expect: 32 */
int main(void)
{
    return sizeof(int (*[3])(int)) + sizeof(int (*(*)(int))(char));
}
