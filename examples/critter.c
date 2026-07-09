/* Mini critter world.
 *
 * This is a tiny Cornell-style critter simulation adapted for xcc examples:
 * no runtime library, no printing, and a deterministic result returned from
 * main.  The critter has a simple rule:
 *
 *   if food is directly ahead, move;
 *   else smell for the nearest food with BFS and face that direction;
 *   else turn right;
 *   then move if the cell ahead is open.
 *
 * The final return value is a checksum of the raw critter data and remaining
 * world cells, reduced to one byte so it can be inspected with `echo $?`.
 */

enum Cell {
    EMPTY = 0,
    WALL = 1,
    FOOD = 2
};

enum Direction {
    UP = 0,
    RIGHT = 1,
    DOWN = 2,
    LEFT = 3
};

struct Critter {
    int x;
    int y;
    int dir;
    int energy;
    int eaten;
    int age;
    int trace;
};

struct Rule {
    int smell_food;
    int turn_when_lost;
    int move_when_clear;
};

int dx(int dir)
{
    if (dir == RIGHT)
        return 1;
    if (dir == LEFT)
        return -1;
    return 0;
}

int dy(int dir)
{
    if (dir == DOWN)
        return 1;
    if (dir == UP)
        return -1;
    return 0;
}

int turn_right(int dir)
{
    return (dir + 1) % 4;
}

int inside(int x, int y)
{
    if (x < 0)
        return 0;
    if (y < 0)
        return 0;
    if (x >= 5)
        return 0;
    if (y >= 5)
        return 0;
    return 1;
}

int passable(int world[5][5], int x, int y)
{
    if (inside(x, y) == 0)
        return 0;
    return world[y][x] != WALL;
}

int food_ahead(int world[5][5], struct Critter *cr)
{
    int nx;
    int ny;

    nx = cr->x + dx(cr->dir);
    ny = cr->y + dy(cr->dir);
    if (inside(nx, ny) == 0)
        return 0;
    return world[ny][nx] == FOOD;
}

int smell_nearest_food(int world[5][5], int sx, int sy)
{
    int seen[5][5];
    int qx[25];
    int qy[25];
    int qfirst[25];
    int head;
    int tail;
    int x;
    int y;
    int first;
    int dir;
    int nx;
    int ny;
    int i;
    int j;

    for (i = 0; i < 5; i = i + 1) {
        for (j = 0; j < 5; j = j + 1)
            seen[i][j] = 0;
    }

    head = 0;
    tail = 0;
    qx[tail] = sx;
    qy[tail] = sy;
    qfirst[tail] = -1;
    tail = tail + 1;
    seen[sy][sx] = 1;

    while (head < tail) {
        x = qx[head];
        y = qy[head];
        first = qfirst[head];
        head = head + 1;

        if (first != -1) {
            if (world[y][x] == FOOD)
                return first;
        }

        for (dir = 0; dir < 4; dir = dir + 1) {
            nx = x + dx(dir);
            ny = y + dy(dir);
            if (passable(world, nx, ny)) {
                if (seen[ny][nx] == 0) {
                    seen[ny][nx] = 1;
                    qx[tail] = nx;
                    qy[tail] = ny;
                    if (first == -1)
                        qfirst[tail] = dir;
                    else
                        qfirst[tail] = first;
                    tail = tail + 1;
                }
            }
        }
    }
    return -1;
}

int choose_direction(int world[5][5], struct Critter *cr, struct Rule *rule)
{
    int smell;

    if (food_ahead(world, cr))
        return cr->dir;

    if (rule->smell_food) {
        smell = smell_nearest_food(world, cr->x, cr->y);
        if (smell != -1)
            return smell;
    }

    if (rule->turn_when_lost)
        return turn_right(cr->dir);
    return cr->dir;
}

void move_forward(int world[5][5], struct Critter *cr)
{
    int nx;
    int ny;

    nx = cr->x + dx(cr->dir);
    ny = cr->y + dy(cr->dir);
    cr->energy = cr->energy - 1;

    if (passable(world, nx, ny)) {
        cr->x = nx;
        cr->y = ny;
        if (world[ny][nx] == FOOD) {
            world[ny][nx] = EMPTY;
            cr->eaten = cr->eaten + 1;
            cr->energy = cr->energy + 5;
        }
    }
}

void update_trace(struct Critter *cr)
{
    cr->trace = (cr->trace * 17 + cr->x * 3 + cr->y * 5 + cr->dir * 7 +
                 cr->energy + cr->eaten * 11 + cr->age) % 251;
}

void critter_step(int world[5][5], struct Critter *cr, struct Rule *rule)
{
    cr->age = cr->age + 1;
    cr->dir = choose_direction(world, cr, rule);
    if (rule->move_when_clear)
        move_forward(world, cr);
    update_trace(cr);
}

int final_checksum(int world[5][5], struct Critter *cr)
{
    int h;
    int x;
    int y;

    h = cr->trace;
    h = (h * 13 + cr->x) % 256;
    h = (h * 13 + cr->y) % 256;
    h = (h * 13 + cr->dir) % 256;
    h = (h * 13 + cr->energy) % 256;
    h = (h * 13 + cr->eaten) % 256;
    h = (h * 13 + cr->age) % 256;

    for (y = 0; y < 5; y = y + 1) {
        for (x = 0; x < 5; x = x + 1)
            h = (h * 13 + world[y][x] * (1 + x + y * 5)) % 256;
    }
    return h;
}

int run_critter(void)
{
    int world[5][5] = {
        {WALL, WALL, WALL, WALL, WALL},
        {WALL, EMPTY, FOOD, EMPTY, WALL},
        {WALL, EMPTY, WALL, FOOD, WALL},
        {WALL, FOOD, EMPTY, EMPTY, WALL},
        {WALL, WALL, WALL, WALL, WALL}
    };
    struct Critter cr;
    struct Rule rule;
    int tick;

    cr.x = 1;
    cr.y = 1;
    cr.dir = RIGHT;
    cr.energy = 8;
    cr.eaten = 0;
    cr.age = 0;
    cr.trace = 0;

    rule.smell_food = 1;
    rule.turn_when_lost = 1;
    rule.move_when_clear = 1;

    for (tick = 0; tick < 12; tick = tick + 1) {
        if (cr.energy > 0)
            critter_step(world, &cr, &rule);
    }

    return final_checksum(world, &cr);
}

int main(void)
{
    return run_critter();
}
