/* SPDX-License-Identifier: MIT */
/* expect: 0 */
enum State { OFF, ON };

int read_state();
int read_state(enum State state);

int main(void)
{
    return 0;
}
