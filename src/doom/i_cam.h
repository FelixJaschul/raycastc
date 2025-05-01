#pragma once

// convert angle in [-(HFOV / 2)..+(HFOV / 2)] to X coordinate
static inline int screen_angle_to_x(f32 angle) {
    return ((int) (SCREEN_WIDTH / 2)) * (1.0f - tan(((angle + (HFOV / 2.0)) / HFOV) * PI_2 - PI_4));
}

// world space -> camera space (translate and rotate)
static inline v2 world_pos_to_camera(v2 p) {
    const v2 u = { p.x - state.camera.pos.x, p.y - state.camera.pos.y };

    return (v2) {
        u.x * state.camera.anglesin - u.y * state.camera.anglecos,
        u.x * state.camera.anglecos + u.y * state.camera.anglesin,
    };
}