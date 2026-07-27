/* SPDX-License-Identifier: MIT */
/* expect: 42 */
int update(int *values)
{
    values[1] = (values[0] * 17 + 9) % 251;
    return values[1];
}
int main(void)
{
    int values[2];
    values[0] = 61;
    return update(values);
}
