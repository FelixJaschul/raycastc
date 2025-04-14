#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <stdbool.h>

// Error handling
#define ASSERT(_e, ...) do { if (!(_e)) { fprintf(stderr, __VA_ARGS__); exit(1); } } while(0)

typedef float           f32;
typedef double          f64;
typedef uint8_t         u8;
typedef uint16_t        u16;
typedef uint32_t        u32;
typedef uint64_t        u64;
typedef int8_t          i8;
typedef int16_t         i16;
typedef int32_t         i32;
typedef int64_t         i64;
typedef size_t          usize;
typedef ssize_t         isize;

#define SCREEN_WIDTH    384
#define SCREEN_HEIGHT   216

typedef struct v2_s     { f32 x, y; } v2;
typedef struct v2i_s    { i32 x, y; } v2i;

#define dot(v0, v1)     ({ v2 _v0 = (v0), _v1 = (v1); (_v0.x * _v1.x) + (_v0.y * _v1.y); })
#define length(v)       ({ const v2 _v = (v); sqrt(dot(_v, _v)); })
#define normalize(v)    ({ const v2 _u = (v); const f32 l = length(_u); (v2) { _u.x / l, _u.y / l }; })
#define min(a, b)       ({ __typeof__(a) _a = (a), _b = (b); _a < _b ? _a : _b; })
#define max(a, b)       ({ __typeof__(a) _a = (a), _b = (b); _a > _b ? _a : _b; })
#define sign(a)         ({ __typeof__(a) _a = (a); (__typeof__(a)) (_a < 0 ? -1 : (_a > 0 ? 1 : 0)); })

#define MAP_SIZE        8
static u8 MAPDATA[MAP_SIZE * MAP_SIZE] = {
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 3, 0, 2, 2, 0, 1,
    1, 0, 3, 0, 0, 0, 0, 1,
    1, 0, 3, 0, 4, 4, 0, 1,
    1, 0, 3, 0, 4, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
};

struct {
    SDL_Window *window;
    SDL_Texture *texture;
    SDL_Renderer *renderer;
    u32 pixels[SCREEN_WIDTH * SCREEN_HEIGHT];
    bool quit;
    v2 pos, dir, plane;
} state;

static void verline(int x, int y0, int y1, u32 color) {
    for (int y = y0; y < y1; y++)
        state.pixels[(y * SCREEN_WIDTH) + x] = color;
}

static void r() {
    for (int x = 0; x < SCREEN_WIDTH; x++)
        verline(x, 20, SCREEN_HEIGHT - 20, 0xFFFFA500 | (x & 0xFF));
}

static void render() {
    // 1. Iterating over Screen
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        // 2. Compute ray dir for this column
        const f32 xcam  = (2 * (x / (f32) (SCREEN_WIDTH))) -1;
        const v2 dir    = { state.dir.x + state.plane.x * xcam,
                            state.dir.y + state.plane.y * xcam
        };

        // 3. Initial pos and integer tile pos
        v2 pos = state.pos; // Starting point of ray
        v2i ipos = { (int) pos.x, (int) pos.y }; // Storing integer map coords

        // 4. Pre-get dist between x / y sides (delta)
        const v2 deltaDist = {  fabsf(dir.x) < 1e-20 ? 1e30 : fabsf(1.0f / dir.x),
                                fabsf(dir.y) < 1e-20 ? 1e30 : fabsf(1.0f / dir.y)
        };

        // 5. Calc initial dist to dirst x / y grid lines
        v2 sideDist = { deltaDist.x * (dir.x < 0 ? (pos.x - ipos.x) : ((ipos.x + 1) - pos.x)),
                        deltaDist.y * (dir.y < 0 ? (pos.y - ipos.y) : ((ipos.y + 1) - pos.y))
        }; // How fast ray has to go till first x / y wall boundary

        // 6. Step dir on x / y axes
        const v2i step = {  (int) sign(dir.x),
                            (int) sign(dir.y)
        };

        // 7. DDA ???
        struct {
            int val, side;
            v2 pos;
        } hit = { 0, 0, { 0.0f, 0.0f } };

        while (!hit.val) {
            if (sideDist.x < sideDist.y) {
                sideDist.x += deltaDist.x;
                ipos.x += step.x;
                hit.side = 0;
            }
            else {
                sideDist.y += deltaDist.y;
                ipos.y += step.y;
                hit.side = 1;
            }

            // ASSERT( ipos.x >= 0 && ipos.x < MAP_SIZE && ipos.y >= 0 && ipos.y < MAP_SIZE, "DDA out of Bounds");

            hit.val = MAPDATA[ipos.y * MAP_SIZE + ipos.x];
        }   // -- March along the grid until wall is hit
            // -- sideDist => next side boundary
            // -- hit.side => weather x (0) or y (1) was crossed
            // -- Loop ends when a non-zero value (wall) is found in the map

        // 8. Color based on map value
        u32 color = 0;
        switch (hit.val) {
            case 1: color = 0xFF0000FF; break;
            case 2: color = 0xFF00FF00; break;
            case 3: color = 0xFFFF0000; break;
            case 4: color = 0xFFFF00FF; break;
        }

        // 9. Walls on y-Sides are darker
        if (hit.side == 1) {
            const u32
                r   = ((color & 0xFF0000) >> 16) * 0.6,
                g   = ((color & 0x00FF00) >> 8) * 0.6,
                b   = (color & 0x0000FF) * 0.6;

            color = (0xFF000000) | (min(r, 255) << 16) | (min(g, 255) << 8) | min(b, 255);
        } // Darkens color of ray hit wall from the y-axis side


        // 10. Hit point and perpendicular dist
        hit.pos = (v2) { pos.x + sideDist.x, pos.y + sideDist.y };

        const f32 dperp = hit.side == 0 ? (sideDist.x - deltaDist.x) : (sideDist.y - deltaDist.y);

        const int
            h   = (int) (SCREEN_HEIGHT / dperp),
            y0  = max((SCREEN_HEIGHT / 2) - (h / 2), 0),
            y1  = min((SCREEN_HEIGHT / 2) + (h / 2), SCREEN_HEIGHT - 1);

        verline(x, 0, y0, 0xFF202020);
        verline(x, y0, y1, color);
        verline(x, y1, SCREEN_HEIGHT - 1, 0xFF505050);
    }
}

static void move() {
    const f32
            rot = 3.0f * 0.016f,
            mov = 3.0f * 0.016f;
    const v2
            d = state.dir,
            p = state.plane;
    const u8
            *keystate = SDL_GetKeyboardState(NULL);
    if (keystate[SDL_SCANCODE_LEFT]) {
        state.dir.x = d.x * cos(rot) - d.y * sin(rot);
        state.dir.y = d.x * sin(rot) + d.y * cos(rot);
        state.plane.x = p.x * cos(rot) - p.y * sin(rot);
        state.plane.y = p.x * sin(rot) + p.y * cos(rot);
    }
    if (keystate[SDL_SCANCODE_RIGHT]) {
        state.dir.x = d.x * cos(-rot) - d.y * sin(-rot);
        state.dir.y = d.x * sin(-rot) + d.y * cos(-rot);
        state.plane.x = p.x * cos(-rot) - p.y * sin(-rot);
        state.plane.y = p.x * sin(-rot) + p.y * cos(-rot);
    }
    if (keystate[SDL_SCANCODE_UP]) {
        state.pos.x += state.dir.x * mov;
        state.pos.y += state.dir.y * mov;
    }
    if (keystate[SDL_SCANCODE_DOWN]) {
        state.pos.x -= state.dir.x * mov;
        state.pos.y -= state.dir.y * mov;
    }
}

int main(void) {
    // Handle and Init. Events

    // ASSERT(!SDL_Init(SDL_INIT_EVERYTHING), "SDL_Init failed");

    state.window = SDL_CreateWindow("raycast",
        SDL_WINDOWPOS_CENTERED_DISPLAY(1),
        SDL_WINDOWPOS_CENTERED_DISPLAY(1),
        1280, 720,
        SDL_WINDOW_ALLOW_HIGHDPI);
    // ASSERT(state.window, "SDL_CreateWindow failed");
    
    state.renderer = SDL_CreateRenderer(state.window, 
        -1, SDL_RENDERER_PRESENTVSYNC);
    // ASSERT(state.renderer, "SDL_CreateRenderer failed");

    state.texture = SDL_CreateTexture(state.renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_WIDTH, SCREEN_HEIGHT);
    // ASSERT(state.texture, "SDL_CreateTexture failed");

    state.pos = (v2) { 2, 2 };
    state.dir = normalize(((v2) { -1.0f, 0.1f }));
    state.plane = (v2) {0.0f, 0.66f };

    while (!state.quit) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT:
                    state.quit = true;
                break;
            }
        }
        // Movement
        move();

        // Setup

        memset(state.pixels, 0, sizeof(state.pixels));
        render();

        SDL_UpdateTexture(state.texture, NULL, state.pixels, SCREEN_WIDTH * 4);
        SDL_RenderCopyEx(state.renderer,
            state.texture,NULL, NULL,
            0.0, NULL, SDL_FLIP_VERTICAL);
        SDL_RenderPresent(state.renderer);
    }

    // SDL_DestroyWindow(state.texture);
    // SDL_DestroyWindow(state.renderer);
    SDL_DestroyWindow(state.window);

    return 0;
}