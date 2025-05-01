#pragma once

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