#pragma once

// convert angle in [-(HFOV / 2)..+(HFOV / 2)] to X coordinate
static inline int screen_angle_to_x(f32 angle) {
    return ((int) (SCREEN_WIDTH / 2)) * (1.0f - tan(((angle + (HFOV / 2.0)) / HFOV) * PI_2 - PI_4));
}