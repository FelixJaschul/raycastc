#pragma once

static struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture, *debug;
    u32 *pixels;
    bool quit;

    struct { struct sector arr[32]; usize n; } sectors;
    struct { struct wall arr[128]; usize n; } walls;

    u16 y_lo[SCREEN_WIDTH], y_hi[SCREEN_WIDTH];

    struct {
        v2 pos;
        f32 angle, anglecos, anglesin;
        int sector;
    } camera;

    bool mode;
    bool sleepy;
} state;