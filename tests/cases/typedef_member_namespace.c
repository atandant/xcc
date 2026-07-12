/* SPDX-License-Identifier: MIT */
/* expect: 9 */
/* A member name may reuse a typedef name from the ordinary namespace. */
typedef int Value;

struct Box {
    int Value;
};

int main(void)
{
    struct Box box;
    box.Value = 9;
    return box.Value;
}
