#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <SDL2/SDL.h>

/* -------------------- MACROS AND DEFINITIONS -------------------- */
#define ASSERT(_e, ...) do { if (!(_e)) { fprintf(stderr, __VA_ARGS__); exit(1); } } while (0)
#define MIN(a, b)       ((a) < (b) ? (a) : (b))
#define MAX(a, b)       ((a) > (b) ? (a) : (b))
#define CLAMP(x, a, b)  (MIN(MAX((x), (a)), (b)))
#define ABS(x)          ((x) < 0 ? -(x) : (x))
#define SQR(x)          ((x) * (x))

/* -------------------- DISPLAY SETTINGS -------------------- */
#define SCREEN_WIDTH    384
#define SCREEN_HEIGHT   216
#define GRID_SIZE       32
#define GRID_DIST_LEFT  331
#define GRID_DIST_TOP   29
#define EDITOR_SIZE     640
#define EDITOR_STEP     16

/* -------------------- PLAYER SETTINGS -------------------- */
#define MOVE_SPEED      0.05f
#define ROT_SPEED       0.03f
#define COLLISION_BUFFER 0.01f

/* -------------------- COLORS -------------------- */
#define COLOR_BLACK     0xFF000000
#define COLOR_FLOOR     0xFF505050
#define COLOR_CEILING   0xFF202020
#define COLOR_WALL1     0xFF7070FF  // Wall side 1
#define COLOR_WALL2     0xFF6060A0  // Wall side 2
#define COLOR_GRID      0xFF323232
#define COLOR_DOT       0xFF505050
#define COLOR_LINE      0xFFC8C8C8
#define COLOR_SELECT    0xFFFF0000

/* -------------------- TYPE DEFINITIONS -------------------- */
typedef float           f32;
typedef int             i32;
typedef uint8_t         u8;
typedef uint32_t        u32;
typedef bool            b;

typedef struct { f32 x; f32 y; } v2;
typedef struct { i32 x; i32 y; } v2i;
typedef struct { v2i p0; v2i p1; } wall_line;

/* -------------------- GLOBAL STATE -------------------- */
static struct {
    SDL_Window*     window;
    SDL_Renderer*   renderer;
    SDL_Texture*    texture;
    u32             pixels[SCREEN_WIDTH * SCREEN_HEIGHT];

    v2              pos;
    v2              dir;
    v2              plane;

    i32             mode;
    wall_line       walls[1024];
    wall_line       undo_stack[1024];
    wall_line       redo_stack[1024];

    i32             wall_count;
    i32             undo_count;
    i32             redo_count;

    b               quit;
    b               move_forward;
    b               move_backward;
    b               strafe_left;
    b               strafe_right;
    b               mouse_control;

    i32             prev_mouse_x;
    i32             prev_mouse_y;
    f32             mouse_sensitivity;
} state;

/* -------------------- FUNCTION DECLARATIONS -------------------- */
void save_map(void);
void load_map(void);
void toggle_mouse_control(void);
void handle_key_event(const SDL_KeyboardEvent* key, b down);
void handle_mouse_motion(const SDL_MouseMotionEvent* motion);
void update_player(void);
b will_collide(v2 new_pos);
b ray_vs_segment(v2 ro, v2 rd, v2 n, v2 m, f32* out_dist, b* out_side);
void verline(i32 x, i32 y0, i32 y1, u32 color);
void render_game(void);
void render_editor(void);
i32 main(void);

/* -------------------- MAP SAVING/LOADING -------------------- */
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

/* -------------------- INPUT HANDLING -------------------- */
void toggle_mouse_control(void) {
    state.mouse_control = !state.mouse_control;

    SDL_SetRelativeMouseMode(state.mouse_control ? SDL_TRUE : SDL_FALSE);
}

void handle_key_event(const SDL_KeyboardEvent* key, const b down) {
    if (down && key->keysym.sym == SDLK_m) {
        toggle_mouse_control();
        return;
    }

    switch (key->keysym.sym) {
        case SDLK_w: state.move_forward = down; break;
        case SDLK_s: state.move_backward = down; break;
        case SDLK_a: state.strafe_left = down; break;
        case SDLK_d: state.strafe_right = down; break;
        default: break;
    }
}

void handle_mouse_motion(const SDL_MouseMotionEvent* motion) {
    if (state.mode != 0 || !state.mouse_control) return;

    if (SDL_GetRelativeMouseMode()) {
        const i32 dx = motion->xrel;

        if (dx != 0) {
            const f32 rot_amount = -dx * state.mouse_sensitivity;
            const f32 cos_rot = cosf(rot_amount);
            const f32 sin_rot = sinf(rot_amount);

            // Rotate direction vector
            const f32 old_dir_x = state.dir.x;

            state.dir.x = state.dir.x * cos_rot - state.dir.y * sin_rot;
            state.dir.y = old_dir_x * sin_rot + state.dir.y * cos_rot;

            // Rotate camera plane
            const f32 old_plane_x = state.plane.x;

            state.plane.x = state.plane.x * cos_rot - state.plane.y * sin_rot;
            state.plane.y = old_plane_x * sin_rot + state.plane.y * cos_rot;
        }
    }

    state.prev_mouse_x = motion->x;
    state.prev_mouse_y = motion->y;
}

/* -------------------- GAME LOGIC -------------------- */
void update_player(void) {
    if (state.mode != 0) return;

    // Movement is in the reverse direction (forward is negative)
    const f32 dx = -state.dir.x;
    const f32 dy = -state.dir.y;

    // Handle forward/backward movement
    if (state.move_forward) {
        const v2 new_pos = {
            state.pos.x + dx * MOVE_SPEED,
            state.pos.y + dy * MOVE_SPEED
        };

        if (!will_collide(new_pos)) state.pos = new_pos;
    }

    if (state.move_backward) {
        const v2 new_pos = {
            state.pos.x - dx * MOVE_SPEED,
            state.pos.y - dy * MOVE_SPEED
        };

        if (!will_collide(new_pos)) state.pos = new_pos;
    }

    // Handle strafing movement
    if (state.strafe_left) {
        const v2 new_pos = {
            state.pos.x - dy * MOVE_SPEED,
            state.pos.y + dx * MOVE_SPEED
        };

        if (!will_collide(new_pos)) state.pos = new_pos;
    }

    if (state.strafe_right) {
        const v2 new_pos = {
            state.pos.x + dy * MOVE_SPEED,
            state.pos.y - dx * MOVE_SPEED
        };

        if (!will_collide(new_pos)) state.pos = new_pos;
    }
}

b will_collide(v2 new_pos) {
    for (i32 i = 0; i < state.wall_count; i++) {
        // Convert wall coordinates to world space
        const v2 a = {
            state.walls[i].p0.x,
            state.walls[i].p0.y
        };
        const v2 b = {
            state.walls[i].p1.x,
            state.walls[i].p1.y
        };

        // Calculate vectors for distance check
        const v2 ab = {b.x - a.x, b.y - a.y};
        const v2 ap = {new_pos.x - a.x, new_pos.y - a.y};

        const f32 ab_len_squared = SQR(ab.x) + SQR(ab.y);

        // Handle case where wall points are very close (effectively a point)
        if (ab_len_squared < 0.00001f) {
            const f32 dist_squared = SQR(ap.x) + SQR(ap.y);

            if (dist_squared < SQR(COLLISION_BUFFER)) return true;
            continue;
        }

        // Find the closest point on a line segment to the player position
        f32 t = (ap.x * ab.x + ap.y * ab.y) / ab_len_squared;

        t = CLAMP(t, 0.0f, 1.0f);

        const v2 closest = {
            a.x + t * ab.x,
            a.y + t * ab.y
        };

        // Check if the closest point is within the collision buffer
        const f32 dist_squared = SQR(new_pos.x - closest.x) + SQR(new_pos.y - closest.y);

        if (dist_squared < SQR(COLLISION_BUFFER)) return true;
    }

    return false;
}

/* -------------------- RENDERING -------------------- */
b ray_vs_segment(const v2 ro, const v2 rd, const v2 n, const v2 m, f32* out_dist, b* out_side) {
    const v2 ab = {m.x - n.x, m.y - n.y};
    const v2 ao = {ro.x - n.x, ro.y - n.y};

    // Calculate cross-products for an intersection test
    const f32 cross_rd_ab = rd.x * ab.y - rd.y * ab.x;
    const f32 cross_ao_rd = ao.x * rd.y - ao.y * rd.x;
    const f32 cross_ao_ab = ao.x * ab.y - ao.y * ab.x;

    // Check if ray and segment are parallel
    if (fabsf(cross_rd_ab) < 1e-6f) return false;

    // Calculate intersection parameters
    const f32 t = cross_ao_ab / cross_rd_ab;
    const f32 u = cross_ao_rd / cross_rd_ab;

    // Check if the intersection is valid
    if (t >= 0.0f && u >= 0.0f && u <= 1.0f) {
        *out_dist = t;
        *out_side = (fabsf(ab.x) < fabsf(ab.y)); // Determine which side was hit
        return true;
    }

    return false;
}

void verline(i32 x, i32 y0, i32 y1, u32 color) {
    // Clip vertical line-to-screen bounds
    y0 = CLAMP(y0, 0, SCREEN_HEIGHT - 1);
    y1 = CLAMP(y1, 0, SCREEN_HEIGHT);

    // Draw a vertical line from y0 to y1 at position x
    for (i32 y = y0; y < y1; y++) {
        if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
            state.pixels[y * SCREEN_WIDTH + x] = color;
        }
    }
}

void render_game(void) {
    // Clear screen buffer
    memset(state.pixels, 0, sizeof(state.pixels));

    // Cast rays for each screen column
    for (i32 x = 0; x < SCREEN_WIDTH; x++) {
        // Render right-to-left for the proper orientation
        const i32 screen_x = SCREEN_WIDTH - 1 - x;

        // Calculate ray direction
        const f32 cam = 2.f * ((f32)screen_x / (f32)SCREEN_WIDTH) - 1.f;

        v2 rd = {
            state.dir.x + state.plane.x * cam,
            state.dir.y + state.plane.y * cam
        };

        // Normalize ray direction
        const f32 rl = sqrtf(SQR(rd.x) + SQR(rd.y));

        rd.x /= rl;
        rd.y /= rl;

        // Find the closest wall intersection
        f32 closest = 1e9f;
        b side = 0;

        for (i32 i = 0; i < state.wall_count; i++) {
            // Convert wall to world space
            const v2 n = {
                state.walls[i].p0.x,
                state.walls[i].p0.y
            };
            const v2 m = {
                state.walls[i].p1.x,
                state.walls[i].p1.y
            };

            // Test ray intersection
            f32 dist;
            b s;
            if (ray_vs_segment(state.pos, rd, n, m, &dist, &s)) {
                if (dist < closest) {
                    closest = dist;
                    side = s;
                }
            }
        }

        // Draw ceiling and floor
        verline(x, 0, SCREEN_HEIGHT / 2, COLOR_CEILING);
        verline(x, SCREEN_HEIGHT / 2, SCREEN_HEIGHT, COLOR_FLOOR);

        // Draw wall slice if intersection found
        if (closest < 1e9f) {
            const i32 h = (i32)(SCREEN_HEIGHT / closest);
            const i32 y0 = SCREEN_HEIGHT / 2 - h / 2;
            const i32 y1 = SCREEN_HEIGHT / 2 + h / 2;

            const u32 wall_color = side ? COLOR_WALL2 : COLOR_WALL1;

            verline(x, MAX(0, y0), MIN(SCREEN_HEIGHT, y1), wall_color);  // Wall
        }
    }
}

void render_editor(void) {
    SDL_SetRenderDrawColor(state.renderer, 20, 20, 20, 255);
    SDL_RenderClear(state.renderer);

    // Grid-Punkte zeichnen
    SDL_SetRenderDrawColor(state.renderer, 50, 50, 50, 255);
    for (i32 y = 0; y <= EDITOR_SIZE; y += EDITOR_STEP) {
        for (i32 x = 0; x <= EDITOR_SIZE; x += EDITOR_STEP) {
            SDL_Rect dot = {
                x + GRID_DIST_LEFT,
                y + GRID_DIST_TOP,
                3, 3
            };

            SDL_RenderFillRect(state.renderer, &dot);
        }
    }

    // Wände zeichnen
    SDL_SetRenderDrawColor(state.renderer, 200, 200, 200, 255);
    for (i32 i = 0; i < state.wall_count; i++) {
        const wall_line w = state.walls[i];
        SDL_RenderDrawLine(
            state.renderer,
            w.p0.x + GRID_DIST_LEFT, w.p0.y + GRID_DIST_TOP,
            w.p1.x + GRID_DIST_LEFT, w.p1.y + GRID_DIST_TOP
        );
    }
    // Highlight hovered vertex
    i32 mx, my;

    SDL_GetMouseState(&mx, &my);

    v2i hover = {-1, -1};
    f32 best = 10.0f;

    for (i32 i = 0; i < state.wall_count; i++) {
        const v2i pts[2] = {state.walls[i].p0, state.walls[i].p1};
        for (i32 j = 0; j < 2; j++) {
            const i32 vx = pts[j].x + GRID_DIST_LEFT;
            const i32 vy = pts[j].y + GRID_DIST_TOP;
            const f32 d = hypotf((f32)(mx - vx), (f32)(my - vy));

            if (d < best) {
                best = d;
                hover = pts[j];
            }
        }
    }

    if (hover.x != -1) {
        SDL_SetRenderDrawColor(state.renderer, 255, 0, 0, 255);
        const SDL_Rect r = {
            hover.x + GRID_DIST_LEFT - 5,
            hover.y + GRID_DIST_TOP - 5,
            10, 10
        };

        SDL_RenderDrawRect(state.renderer, &r);
    }

    SDL_RenderPresent(state.renderer);
}

/* -------------------- MAIN FUNCTION -------------------- */
i32 main(void) {
    // Initialize SDL
    SDL_Init(SDL_INIT_VIDEO);
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1");

    // Create a window, renderer and texture
    state.window = SDL_CreateWindow(
        "RAYCAST",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
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

    // Initialize game state
    state.mouse_control = true;
    state.mouse_sensitivity = 0.002f;
    state.prev_mouse_x = 0;
    state.prev_mouse_y = 0;

    state.pos = (v2){40, 40};
    state.dir = (v2){1, 0};
    state.plane = (v2){0, 0.66f};

    // Normalize direction vector
    const f32 dl = sqrtf(SQR(state.dir.x) + SQR(state.dir.y));

    state.dir.x /= dl;
    state.dir.y /= dl;

    state.mode = 0;
    state.wall_count = 0;
    state.undo_count = 0;
    state.redo_count = 0;

    state.move_forward = false;
    state.move_backward = false;
    state.strafe_left = false;
    state.strafe_right = false;

    state.quit = false;

    // Set mouse mode and load a map
    if (state.mouse_control) SDL_SetRelativeMouseMode(SDL_TRUE);
    load_map();

    // Main game loop
    while (!state.quit) {
        // Process events
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT:
                    state.quit = true; break;

                case SDL_KEYDOWN:
                    if (ev.key.keysym.sym == SDLK_TAB) {
                        // Toggle between game and editor mode
                        state.mode = state.mode == 0 ? 1 : 0;
                        if (state.mode == 0 && state.mouse_control) {
                            SDL_SetRelativeMouseMode(SDL_TRUE);
                        } else {
                            SDL_SetRelativeMouseMode(SDL_FALSE);
                        }
                    } else if (ev.key.keysym.sym == SDLK_ESCAPE) {
                        state.quit = true;
                    } else {
                        handle_key_event(&ev.key, true);
                    } break;

                case SDL_KEYUP:
                    handle_key_event(&ev.key, false); break;

                case SDL_MOUSEMOTION:
                    handle_mouse_motion(&ev.motion); break;

                case SDL_MOUSEBUTTONDOWN:
                    if (state.mode == 1 && ev.button.button == SDL_BUTTON_LEFT) {
                        // Handle editor click
                        i32 mx = ev.button.x - GRID_DIST_LEFT;
                        i32 my = ev.button.y - GRID_DIST_TOP;

                        // Check if clicked near an existing vertex
                        v2i hover = {-1, -1};
                        f32 best = 10.0f;

                        for (i32 i = 0; i < state.wall_count; i++) {
                            const v2i pts[2] = {state.walls[i].p0, state.walls[i].p1};
                            for (i32 j = 0; j < 2; j++) {
                                const i32 vx = pts[j].x;
                                const i32 vy = pts[j].y;
                                const f32 d = hypotf((f32)(mx - vx), (f32)(my - vy));

                                if (d < best) {
                                    best = d;
                                    hover = pts[j];
                                }
                            }
                        }

                        // Snap to vertex if close enough
                        if (hover.x != -1 && best < 10.0f) {
                            mx = hover.x;
                            my = hover.y;
                        }

                        // Place wall point if in editor bounds
                        if (mx >= 0 && mx < GRID_SIZE && my >= 0 && my < GRID_SIZE) {
                            if (state.wall_count % 2 == 0) {
                                // Place first point of wall
                                state.walls[state.wall_count / 2].p0 = (v2i){mx, my};
                                state.walls[state.wall_count / 2].p1 = (v2i){mx, my};
                                state.wall_count++;
                            } else {
                                // Place second point of wall and save
                                state.walls[(state.wall_count - 1) / 2].p1 = (v2i){mx, my};
                                state.wall_count++;
                                save_map();
                            }
                        }
                    } break;
                default: break;
            }
        }

        // Update game state
        update_player();

        // Render based on current mode
        if (state.mode == 0) {
            // Game mode
            render_game();

            SDL_UpdateTexture(
                state.texture, 
                NULL, state.pixels, 
                SCREEN_WIDTH * 4
            );

            SDL_RenderClear(state.renderer);

            SDL_RenderCopyEx(
                state.renderer,
                state.texture,
                NULL, NULL,
                0, NULL,
                SDL_FLIP_VERTICAL
            );

            SDL_RenderPresent(state.renderer);
        } else {
            // Editor mode
            render_editor();
        }
    }

    // Clean up resources
    SDL_DestroyTexture(state.texture);
    SDL_DestroyRenderer(state.renderer);
    SDL_DestroyWindow(state.window);

    SDL_Quit();

    return 0;
}
