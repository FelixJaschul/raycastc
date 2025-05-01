#include "all.h"

void save_map(void) {
    FILE* f = fopen("map.txt", "w");
    fprintf(f, "[WALLS]\n");

    for (i32 i = 0; i < state.wall_count; i++) {
        fprintf(f, "[WALL%d]\n%d %d %d %d\n",
                i + 1,
                state.walls[i].p0.x,
                state.walls[i].p0.y,
                state.walls[i].p1.x,
                state.walls[i].p1.y);
    }

    fclose(f);
}

void load_map(void) {
    state.wall_count = 0;
    char tag[64];

    FILE* f = fopen("map.txt", "r");

    while (fscanf(f, "%63s", tag) == 1) {
        if (strncmp(tag, "[WALL", 5) == 0) {
            i32 x0, y0, x1, y1;

            if (fscanf(f, "%d %d %d %d", &x0, &y0, &x1, &y1) == 4) {
                state.walls[state.wall_count++] = (wall_line){
                    .p0 = {x0, y0},
                    .p1 = {x1, y1}
                };
            }
        }
    }

    fclose(f);
}