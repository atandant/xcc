/* SPDX-License-Identifier: MIT */
/* expect-error: duplicate label 'done' */
int main(void)
{
done:
    ;
    {
done:
        return 0;
    }
}
