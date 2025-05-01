#pragma once
#include "s_state.h"

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

// rotate vector v by angle a
static inline v2 rotate(v2 v, f32 a) {
    return (v2) {
        (v.x * cos(a)) - (v.y * sin(a)),
        (v.x * sin(a)) + (v.y * cos(a)),
    };
}