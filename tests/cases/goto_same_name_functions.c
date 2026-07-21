/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int first(void)
{
    goto done;
done:
    return 3;
}

int second(void)
{
    goto done;
done:
    return 4;
}

int main(void)
{
    return first() + second();
}
