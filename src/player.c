#pragma once

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

            if (dist_squared < SQR(COLL_BUFFER)) return true;
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

        if (dist_squared < SQR(COLL_BUFFER)) return true;
    }

    return false;
}

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