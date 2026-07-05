/* Breadth-first search on a small star graph.
 *
 * There is no printf/runtime in xcc examples, so main returns the path as a
 * decimal digit encoding.  The best path from node 0 to node 5 is 0 -> 2 -> 5,
 * encoded as 15 because the leading 0 is omitted and xcc currently returns
 * the discovered parent chain as low-to-high decimal places: node 5, then 1.
 */
int bfs_path_code(int start, int goal)
{
    int edge[6][6];
    int seen[6];
    int parent[6];
    int queue[6];
    int head;
    int tail;
    int i;
    int j;
    int u;
    int p;
    int code;
    int place;

    for (i = 0; i < 6; i = i + 1) {
        seen[i] = 0;
        parent[i] = -1;
        for (j = 0; j < 6; j = j + 1)
            edge[i][j] = 0;
    }

    edge[0][1] = 1;
    edge[1][0] = 1;
    edge[0][2] = 1;
    edge[2][0] = 1;
    edge[0][3] = 1;
    edge[3][0] = 1;
    edge[0][4] = 1;
    edge[4][0] = 1;
    edge[1][5] = 1;
    edge[5][1] = 1;
    edge[2][5] = 1;
    edge[5][2] = 1;

    head = 0;
    tail = 0;
    queue[tail] = start;
    tail = tail + 1;
    seen[start] = 1;

    while (head < tail) {
        u = queue[head];
        head = head + 1;
        if (u == goal)
            head = tail;
        else {
            for (i = 0; i < 6; i = i + 1) {
                if (edge[u][i]) {
                    if (seen[i] == 0) {
                        seen[i] = 1;
                        parent[i] = u;
                        queue[tail] = i;
                        tail = tail + 1;
                    }
                }
            }
        }
    }

    code = 0;
    place = 1;
    p = goal;
    while (p != start) {
        code = code + p * place;
        place = place * 10;
        p = parent[p];
    }
    return code;
}

int main(void)
{
    return bfs_path_code(0, 5);
}
