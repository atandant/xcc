/* Mini critter world (xcc integration example).
 *
 * Cornell-style multi-species simulation with no runtime library.  main
 * returns a deterministic byte checksum (inspect with `echo $?`).
 *
 * Exercises: typedef, unsigned char grid, long checksum, struct by-value
 * (sret), union rule flags, function-pointer brains, sizeof folds,
 * comma operator, continue, else if, 8x8 grid, three critters, 48 ticks.
 */

enum {
    WORLD_W = 8,
    WORLD_H = 8,
    MAX_CRITTERS = 3,
    MAX_TICKS = 48,
    WORLD_CELLS = 64
};

typedef unsigned char Cell;

enum CellKind {
    C_EMPTY = 0,
    C_WALL = 1,
    C_FOOD = 2
};

enum Species {
    HERB = 0,
    CARN = 1,
    SCAV = 2
};

enum Direction {
    UP = 0,
    RIGHT = 1,
    DOWN = 2,
    LEFT = 3
};

typedef struct Critter {
    int x;
    int y;
    int dir;
    int energy;
    int eaten;
    int age;
    int trace;
    int species;
} Critter;

struct Rule {
    int smell_food;
    int turn_when_lost;
    int move_when_clear;
};

union RuleFlags {
    struct Rule rule;
    int as_int;
};

typedef int (*PickDirFn)(Cell world[WORLD_H][WORLD_W], Critter *cr,
                         struct Rule *rule);
typedef void (*OnEatFn)(Critter *cr);

struct Brain {
    PickDirFn pick_dir;
    OnEatFn on_eat;
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
    if (x >= WORLD_W)
        return 0;
    if (y >= WORLD_H)
        return 0;
    return 1;
}

int passable(Cell world[WORLD_H][WORLD_W], int x, int y)
{
    if (inside(x, y) == 0)
        return 0;
    return world[y][x] != C_WALL;
}

int food_ahead(Cell world[WORLD_H][WORLD_W], Critter *cr)
{
    int nx;
    int ny;

    nx = cr->x + dx(cr->dir);
    ny = cr->y + dy(cr->dir);
    if (inside(nx, ny) == 0)
        return 0;
    return world[ny][nx] == C_FOOD;
}

int smell_nearest_food(Cell world[WORLD_H][WORLD_W], int sx, int sy)
{
    Cell seen[WORLD_H][WORLD_W];
    int qx[WORLD_CELLS];
    int qy[WORLD_CELLS];
    int qfirst[WORLD_CELLS];
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

    for (i = 0; i < WORLD_H; i = i + 1) {
        for (j = 0; j < WORLD_W; j = j + 1)
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
            if (world[y][x] == C_FOOD)
                return first;
        }

        for (dir = 0; dir < 4; dir = dir + 1) {
            nx = x + dx(dir);
            ny = y + dy(dir);
            if (passable(world, nx, ny)) {
                if (seen[ny][nx] == 0) {
                    seen[ny][nx] = 1;
                    (qx[tail] = nx, qy[tail] = ny);
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

int choose_direction_default(Cell world[WORLD_H][WORLD_W], Critter *cr,
                             struct Rule *rule)
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

int choose_direction_carn(Cell world[WORLD_H][WORLD_W], Critter *cr,
                          struct Rule *rule)
{
    int smell;

    if (food_ahead(world, cr))
        return cr->dir;

    if (cr->energy < 4) {
        if (rule->turn_when_lost)
            return turn_right(cr->dir);
        return cr->dir;
    }

    if (rule->smell_food) {
        smell = smell_nearest_food(world, cr->x, cr->y);
        if (smell != -1)
            return smell;
    }

    if (rule->turn_when_lost)
        return turn_right(turn_right(cr->dir));
    return cr->dir;
}

int choose_direction_scav(Cell world[WORLD_H][WORLD_W], Critter *cr,
                           struct Rule *rule)
{
    int smell;
    int hunt;

    if (food_ahead(world, cr))
        return cr->dir;

    hunt = (cr->age % 3) == 0;
    if (hunt == 0) {
        if (rule->turn_when_lost)
            return turn_right(cr->dir);
        return cr->dir;
    }

    if (rule->smell_food) {
        smell = smell_nearest_food(world, cr->x, cr->y);
        if (smell != -1)
            return smell;
    }

    if (rule->turn_when_lost)
        return turn_right(cr->dir);
    return cr->dir;
}

void on_eat_herb(Critter *cr)
{
    cr->energy = cr->energy + 5;
}

void on_eat_carn(Critter *cr)
{
    cr->energy = cr->energy + 8;
    cr->trace = (cr->trace + 3) % 251;
}

void on_eat_scav(Critter *cr)
{
    cr->energy = cr->energy + 3;
    cr->eaten = cr->eaten + 1;
}

void init_brains(struct Brain *brains)
{
    brains[HERB].pick_dir = choose_direction_default;
    brains[HERB].on_eat = on_eat_herb;
    brains[CARN].pick_dir = choose_direction_carn;
    brains[CARN].on_eat = on_eat_carn;
    brains[SCAV].pick_dir = choose_direction_scav;
    brains[SCAV].on_eat = on_eat_scav;
}

void rule_for_species(int species, union RuleFlags *flags)
{
    flags->as_int = 0;
    if (species == HERB) {
        flags->rule.smell_food = 1;
        flags->rule.turn_when_lost = 1;
        flags->rule.move_when_clear = 1;
    } else if (species == CARN) {
        flags->rule.smell_food = 1;
        flags->rule.turn_when_lost = 1;
        flags->rule.move_when_clear = 1;
    } else {
        flags->rule.smell_food = 1;
        flags->rule.turn_when_lost = 0;
        flags->rule.move_when_clear = 1;
    }
}

Critter snapshot_critter(Critter cr)
{
    cr.trace = (cr.trace * 3 + cr.x + cr.y) % 251;
    return cr;
}

void move_forward(Cell world[WORLD_H][WORLD_W], Critter *cr,
                  struct Brain *brain)
{
    int nx;
    int ny;

    nx = cr->x + dx(cr->dir);
    ny = cr->y + dy(cr->dir);
    if (cr->species == CARN)
        cr->energy = cr->energy - 2;
    else
        cr->energy = cr->energy - 1;

    if (passable(world, nx, ny)) {
        cr->x = nx;
        cr->y = ny;
        if (world[ny][nx] == C_FOOD) {
            world[ny][nx] = C_EMPTY;
            cr->eaten = cr->eaten + 1;
            brain->on_eat(cr);
        }
    }
}

void update_trace(Critter *cr)
{
    cr->trace = (cr->trace * 17 + cr->x * 3 + cr->y * 5 + cr->dir * 7 +
                 cr->energy + cr->eaten * 11 + cr->age + cr->species * 13) % 251;
}

int critter_alive(Critter *cr)
{
    return cr->energy > 0;
}

void critter_step(Cell world[WORLD_H][WORLD_W], Critter *cr,
                  struct Rule *rule, struct Brain *brain)
{
    Critter snap;

    cr->age = cr->age + 1;
    cr->dir = brain->pick_dir(world, cr, rule);
    if (rule->move_when_clear)
        move_forward(world, cr, brain);
    update_trace(cr);
    snap = snapshot_critter(*cr);
    cr->trace = snap.trace;
}

void resolve_collisions(Critter *critters, int n)
{
    int i;
    int j;

    for (i = 0; i < n; i = i + 1) {
        if (!critter_alive(&critters[i]))
            continue;
        for (j = i + 1; j < n; j = j + 1) {
            if (!critter_alive(&critters[j]))
                continue;
            if (critters[i].x == critters[j].x &&
                critters[i].y == critters[j].y) {
                if (critters[i].species == CARN &&
                    critters[j].species != CARN)
                    critters[j].energy = 0;
                else if (critters[j].species == CARN &&
                         critters[i].species != CARN)
                    critters[i].energy = 0;
                else {
                    critters[j].x =
                        critters[j].x - dx(critters[j].dir);
                    critters[j].y =
                        critters[j].y - dy(critters[j].dir);
                }
            }
        }
    }
}

long fold_byte(long h, long v)
{
    return (h * 13 + v) % 256;
}

long final_checksum(Cell world[WORLD_H][WORLD_W], int tick,
                    Critter *critters, int n)
{
    long h;
    int x;
    int y;
    int i;

    h = (long)sizeof(Critter);
    h = fold_byte(h, (long)sizeof(Cell));
    h = fold_byte(h, (long)sizeof(struct Rule));
    h = fold_byte(h, (long)tick);

    for (i = 0; i < n; i = i + 1) {
        h = fold_byte(h, (long)critters[i].trace);
        h = fold_byte(h, (long)critters[i].x);
        h = fold_byte(h, (long)critters[i].y);
        h = fold_byte(h, (long)critters[i].dir);
        h = fold_byte(h, (long)critters[i].energy);
        h = fold_byte(h, (long)critters[i].eaten);
        h = fold_byte(h, (long)critters[i].age);
        h = fold_byte(h, (long)critters[i].species);
    }

    for (y = 0; y < WORLD_H; y = y + 1) {
        for (x = 0; x < WORLD_W; x = x + 1)
            h = fold_byte(h, (long)world[y][x] *
                              (long)(1 + x + y * WORLD_W));
    }
    return h;
}

void init_world(Cell world[WORLD_H][WORLD_W])
{
    int x;
    int y;

    for (y = 0; y < WORLD_H; y = y + 1) {
        for (x = 0; x < WORLD_W; x = x + 1) {
            if (x == 0 || y == 0 || x == WORLD_W - 1 || y == WORLD_H - 1)
                world[y][x] = C_WALL;
            else
                world[y][x] = C_EMPTY;
        }
    }

    world[1][2] = C_FOOD;
    world[1][5] = C_FOOD;
    world[2][6] = C_FOOD;
    world[3][2] = C_WALL;
    world[3][4] = C_FOOD;
    world[4][1] = C_FOOD;
    world[5][6] = C_FOOD;
    world[6][3] = C_FOOD;
}

void init_critter(Critter *cr, int x, int y, int dir, int energy, int species)
{
    cr->x = x;
    cr->y = y;
    cr->dir = dir;
    cr->energy = energy;
    cr->eaten = 0;
    cr->age = 0;
    cr->trace = 0;
    cr->species = species;
}

int run_critter(void)
{
    Cell world[WORLD_H][WORLD_W];
    Critter critters[MAX_CRITTERS];
    struct Brain brains[MAX_CRITTERS];
    union RuleFlags flags;
    int tick;
    int i;

    init_brains(brains);
    init_world(world);

    init_critter(&critters[0], 1, 1, RIGHT, 10, HERB);
    init_critter(&critters[1], 1, 6, DOWN, 12, CARN);
    init_critter(&critters[2], 6, 1, UP, 8, SCAV);

    for (tick = 0; tick < MAX_TICKS; tick = tick + 1) {
        for (i = 0; i < MAX_CRITTERS; i = i + 1) {
            if (!critter_alive(&critters[i]))
                continue;
            rule_for_species(critters[i].species, &flags);
            critter_step(world, &critters[i], &flags.rule,
                         &brains[critters[i].species]);
        }
        resolve_collisions(critters, MAX_CRITTERS);
    }

    return (int)final_checksum(world, tick, critters, MAX_CRITTERS);
}

int main(void)
{
    return run_critter();
}
