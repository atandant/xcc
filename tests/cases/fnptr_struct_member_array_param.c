/* SPDX-License-Identifier: MIT */
/* expect: 7 */
enum { WORLD_W = 8, WORLD_H = 8 };
typedef unsigned char Cell;

typedef int (*PickDirFn)(Cell world[WORLD_H][WORLD_W], int x);

struct Brain {
    PickDirFn pick_dir;
};

int pick(Cell world[WORLD_H][WORLD_W], int x)
{
    return world[1][x];
}

int main(void)
{
    Cell world[WORLD_H][WORLD_W];
    struct Brain brain;

    world[1][3] = 7;
    brain.pick_dir = pick;
    return brain.pick_dir(world, 3);
}
