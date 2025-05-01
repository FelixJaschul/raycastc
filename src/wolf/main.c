#include "all.h"

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
