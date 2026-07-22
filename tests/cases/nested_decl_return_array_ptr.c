/* SPDX-License-Identifier: MIT */
/* expect: 9 */
int (*same_row(int (*row)[3]))[3] { return row; }
int main(void)
{
    int row[3];
    row[2] = 9;
    return same_row(&row)[0][2];
}
