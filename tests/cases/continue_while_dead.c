/* SPDX-License-Identifier: MIT */
/* expect: 3 */
int steps_with_dead(int ticks, int alive_start)
{
    int tick;
    int alive;
    int steps;

    alive = alive_start;
    steps = 0;
    tick = 0;
    while (tick < ticks) {
        if (alive <= 0) {
            tick = tick + 1;
            continue;
        }
        steps = steps + 1;
        alive = alive - 1;
        tick = tick + 1;
    }
    return steps;
}

int main(void)
{
    return steps_with_dead(6, 3);
}
