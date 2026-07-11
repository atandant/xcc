/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int load(int *p)
{
    return *p;
}

int main(void)
{
    int x;
    int (*fn)(int *);

    x = 7;
    fn = load;
    return fn(&x);
}
