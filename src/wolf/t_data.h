#pragma once

typedef float           f32;
typedef uint8_t         u8;
typedef uint32_t        u32;
typedef bool            b;

typedef struct { f32 x; f32 y; } v2;
typedef struct { int x; int y; } v2i;
typedef struct { v2i p0; v2i p1; } wall_line;