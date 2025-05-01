#pragma once

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