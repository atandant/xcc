/* SPDX-License-Identifier: MIT */
/* expect: 6 */
enum { WORLD_W = 3, WORLD_H = 2 };
typedef int Row[WORLD_W];

typedef int (*PickFn)(Row grid[WORLD_H], int x);

int pick(Row grid[WORLD_H], int x)
{
    return grid[1][x];
}

struct Brain {
    PickFn pick_dir;
};

int main(void)
{
    Row grid[WORLD_H];
    struct Brain brain;

    grid[0][0] = 1;
    grid[0][1] = 2;
    grid[0][2] = 3;
    grid[1][0] = 4;
    grid[1][1] = 5;
    grid[1][2] = 6;
    brain.pick_dir = pick;
    return brain.pick_dir(grid, 2);
}
