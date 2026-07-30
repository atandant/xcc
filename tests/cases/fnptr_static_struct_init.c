/* SPDX-License-Identifier: MIT */
/* expect: 21 */
struct Dispatch {
    int (*operations[3])(int);
};

int increment(int value) { return value + 1; }
int double_value(int value) { return value * 2; }
static struct Dispatch dispatch = { { increment, double_value, 0 } };

int main(void)
{
    return dispatch.operations[0](5) + dispatch.operations[1](7) +
           (dispatch.operations[2] == 0);
}
