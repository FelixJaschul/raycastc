#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <SDL2/SDL.h>

#define ASSERT(_e, ...) do { if (!(_e)) { fprintf(stderr, __VA_ARGS__); exit(1); } } while (0)

typedef float           f32;
typedef int             i32;
typedef uint8_t         u8;
typedef uint32_t        u32;
typedef size_t          usize;
typedef ssize_t         isize;
typedef bool            b;

typedef struct          { f32 x, y; } v2;
typedef struct          { i32 x, y; } v2i;

#define SCREEN_WIDTH    384
#define SCREEN_HEIGHT   216
#define TILE_SIZE       20
#define GRID_SIZE       32
#define GRID_DIST_LEFT  331
#define GRID_DIST_TOP   29
#define EDITOR_WIDTH    (GRID_SIZE * TILE_SIZE)
#define EDITOR_HEIGHT   (GRID_SIZE * TILE_SIZE)

// General Macros
#define dot(v0,v1)      ((v0).x*(v1).x+(v0).y*(v1).y)
#define length(v)       sqrtf(dot(v,v))
#define normalize(v)    ({v2 _v=(v); \
                        f32 l=length(_v); \
                        (v2){_v.x/l,_v.y/l}; })
#define sign(a)         ((a)<0?-1:((a)>0?1:0))
#define min(a,b)        ((a)<(b)?(a):(b))
#define max(a,b)        ((a)>(b)?(a):(b))

// Macros for The game renderer
#define xcam(a)         (2 * ((a) / (f32)SCREEN_WIDTH) - 1)
#define ray(a, b, c)    ((v2){ (a).x + (b).x * (c), \
                        (a).y + (b).y * (c) })
#define mapcell(a)      ((v2i){ (i32)(a).x, \
                        (i32)(a).y })
#define delta(a)        ((v2){ fabsf(1.0f / (a).x), \
                        fabsf(1.0f / (a).y) })
#define step(a)         ((v2i){ sign((a).x), \
                        sign((a).y) })
#define sd(a, b, c, d)  ((v2){ \
                        (d).x < 0 ? ((a).x - (b).x) * (c).x \
                        : ((b).x + 1.0f - (a).x) * (c).x, \
                        (d).y < 0 ? ((a).y - (b).y) * (c).y \
                        : ((b).y + 1.0f - (a).y) * (c).y })
#define wd(a, b, c)     ((c) ? ((a).y - (b).y) : ((a).x - (b).x))
#define wh(a)           ((i32)(SCREEN_HEIGHT / (a)))
#define wr(a)           ((i32[2]){ max(0, \
                        SCREEN_HEIGHT/2 - (a)/2), \
                        min(SCREEN_HEIGHT, \
                        SCREEN_HEIGHT/2 + (a)/2) })
#define pos(a, b, c)    ((v2){ (c).x * a - (c).y * b, \
                        (c).x * b + (c).y * a })

u8 map[64] = {
    1,1,1,1,1,1,1,1,
    1,0,0,0,0,0,0,1,
    1,0,3,0,2,2,0,1,
    1,0,3,0,0,0,0,1,
    1,0,3,0,4,4,0,1,
    1,0,3,0,4,0,0,1,
    1,0,0,0,0,0,0,1,
    1,1,1,1,1,1,1,1,
};

struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    u32 pixels[SCREEN_WIDTH * SCREEN_HEIGHT];
    v2 pos, dir, plane;
    v2i walls[1024];
    i32 mode; // 0 = game, 1 = editor
    i32 wall_count;
    b quit;
} state;

void verline(i32 x, i32 y0, i32 y1, u32 color) {
    for (i32 y = y0; y < y1; y++)
        state.pixels[y * SCREEN_WIDTH + x] = color;
}

void save_map() {
    FILE *file = fopen("map.txt", "w");
    for (i32 i = 0; i < state.wall_count; i++)
        fprintf(file, "%d %d\n", \
                state.walls[i].x, state.walls[i].y);
    fclose(file);
}

void load_map() {
    FILE *file = fopen("map.txt", "r");
    state.wall_count = 0;
    while (fscanf(file, "%d %d\n", \
        &state.walls[state.wall_count].x, \
        &state.walls[state.wall_count].y) == 2)
        state.wall_count++;
    fclose(file);
}

void render_game() {
    for (i32 x = 0; x < SCREEN_WIDTH; x++) {
        f32 xcam =
            xcam(x);
        v2 ray =
            ray(state.dir, state.plane, xcam),
            pos = state.pos;
        v2i map =
            mapcell(pos),
            step = step(ray);
        v2 delta =
            delta(ray),
            side = sd(pos, map, delta, ray);

        i32 side_hit = 0, val = 0;
        while (!val) {
            if (side.x < side.y) {
                side.x += delta.x;
                map.x += step.x;
                side_hit = 0;
            }
            else {
                side.y += delta.y;
                map.y += step.y;
                side_hit = 1;
            }
            val = state.map[map.y * 8 + map.x];
        }

        u32 color = 0;
        switch (val) {
            case 1: color = 0xFF0000FF; break;
            case 2: color = 0xFF00FF00; break;
            case 3: color = 0xFFFF0000; break;
            case 4: color = 0xFFFF00FF; break;
        }

        if (side_hit) {
            u32 r = ((color >> 16) & 0xFF) * 0.6;
            u32 g = ((color >> 8) & 0xFF) * 0.6;
            u32 b = (color & 0xFF) * 0.6;
            color = (0xFF << 24) | (r << 16) | (g << 8) | b;
        }

        f32 dist = wd(side, delta, side_hit);
        i32 h = wh(dist);
        i32 *yr = wr(h);

        verline(x, 0, yr[0], 0xFF202020);
        verline(x, yr[0], yr[1], color);
        verline(x, yr[1], SCREEN_HEIGHT, 0xFF505050);
    }
}

void render_editor() {
    SDL_SetRenderDrawColor(state.renderer,
        20, 20, 20, 255);
    SDL_RenderClear(state.renderer);

    SDL_SetRenderDrawColor(state.renderer,
        50, 50, 50, 255);
    for (i32 y = 0; y <= GRID_SIZE; y++)
        for (i32 x = 0; x <= GRID_SIZE; x++) {
            SDL_Rect dot = { x * TILE_SIZE + GRID_DIST_LEFT,
                y * TILE_SIZE + GRID_DIST_TOP, 3, 3 };
            SDL_RenderFillRect(state.renderer, &dot);
        }

    SDL_SetRenderDrawColor(state.renderer,
        200, 200, 200, 255);
    for (i32 i = 0; i + 1 < state.wall_count; i += 2) {
        v2i p0 = state.walls[i];
        v2i p1 = state.walls[i + 1];
        if (i % 2 == 0)
            SDL_RenderDrawLine(state.renderer,
                p0.x + GRID_DIST_LEFT, \
                p0.y + GRID_DIST_TOP,
                p1.x + GRID_DIST_LEFT, \
                p1.y + GRID_DIST_TOP);
    }

    SDL_RenderPresent(state.renderer);
}

i32 main() {
    SDL_Init(SDL_INIT_VIDEO);
    state.window = SDL_CreateWindow(
        "WINDOW",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_RESIZABLE
    );
    state.renderer = SDL_CreateRenderer(
        state.window, -1,
        SDL_RENDERER_PRESENTVSYNC
    );
    state.texture = SDL_CreateTexture(
        state.renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_WIDTH, SCREEN_HEIGHT
    );
    state.pos = (v2){ 2, 2 };
    state.dir = normalize(((v2){ -1, 0.1f }));
    state.plane = (v2){ 0, 0.66f };
    state.mode = 0;

    load_map();

    while (!state.quit) {
        if (state.mode == 0) {
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) state.quit = true;
                if (ev.type == SDL_KEYDOWN && \
                    ev.key.keysym.sym == SDLK_TAB)
                    state.mode = 1; // Switch Game Mode
            }
            // Move
            const u8 *k = SDL_GetKeyboardState(NULL);
            const f32 rot = 0.048f, mov = 0.048f;

            if (k[SDL_SCANCODE_LEFT]) {
                state.dir = pos(cosf(rot), sinf(rot), state.dir);
                state.plane = pos(cosf(rot), sinf(rot), state.plane);
            }
            if (k[SDL_SCANCODE_RIGHT]) {
                state.dir = pos(cosf(-rot), sinf(-rot), state.dir);
                state.plane = pos(cosf(-rot), sinf(-rot), state.plane);
            }
            if (k[SDL_SCANCODE_UP]) {
                state.pos.x += state.dir.x * mov;
                state.pos.y += state.dir.y * mov;
            }
            if (k[SDL_SCANCODE_DOWN]) {
                state.pos.x -= state.dir.x * mov;
                state.pos.y -= state.dir.y * mov;
            }
            // Render
            render_game();
        } else {
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) state.quit = true;
                if (ev.type == SDL_KEYDOWN && \
                    ev.key.keysym.sym == SDLK_TAB)
                    state.mode = 0; // Switch Game Mode
                // Draw
                if (ev.type == SDL_MOUSEBUTTONDOWN && \
                    ev.button.button == SDL_BUTTON_LEFT) {
                    const i32 mx = ev.button.x - 331,
                              my = ev.button.y - 29;

                    if (mx >= 0 && mx < EDITOR_WIDTH && \
                        my >= 0 && my < EDITOR_HEIGHT)
                        state.walls[state.wall_count++] = (v2i){ mx, my };
                    else if (state.wall_count > 0)
                        state.wall_count--; // Remove the last added point

                    save_map();
                }
            }
            // Render
            render_editor();
        }
        // Render
        if (state.mode == 0) {
            SDL_UpdateTexture(
                state.texture,
                NULL,
                state.pixels,
                SCREEN_WIDTH * 4
            );
            SDL_RenderCopyEx(
                state.renderer,
                state.texture,
                NULL,
                NULL,
                0.0, NULL,
                SDL_FLIP_VERTICAL
            );
            SDL_RenderPresent(
                state.renderer
            );
        }
    }

    SDL_Quit();
    return 0;
}