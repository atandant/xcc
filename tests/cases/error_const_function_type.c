/* SPDX-License-Identifier: MIT */
/* expect-error: function type must not be const-qualified */
typedef int Function(void);
const Function function;

int main(void)
{
    return 0;
}
