#pragma once

// see: https://en.wikipedia.org/wiki/Line–line_intersection
// compute intersection of two-line segments, returns (NAN, NAN) if there is
// no intersection.
static inline v2 intersect_segs(v2 a0, v2 a1, v2 b0, v2 b1) {
    const f32 d = ((a0.x - a1.x) * (b0.y - b1.y)) - ((a0.y - a1.y) * (b0.x - b1.x));

    if (fabsf(d) < 0.000001f) { return (v2) { NAN, NAN }; }

    const f32 t = (((a0.x - b0.x) * (b0.y - b1.y)) - ((a0.y - b0.y) * (b0.x - b1.x))) / d;
    const f32 u = (((a0.x - b0.x) * (a0.y - a1.y)) - ((a0.y - b0.y) * (a0.x - a1.x))) / d;

    return (t >= 0 && t <= 1 && u >= 0 && u <= 1) ?
        ((v2) { a0.x + (t * (a1.x - a0.x)), a0.y + (t * (a1.y - a0.y)) }) : ((v2) { NAN, NAN });
}

static inline u32 abgr_mul(u32 col, u32 a) {
    const u32 br = ((col & 0xFF00FF) * a) >> 8;
    const u32 g  = ((col & 0x00FF00) * a) >> 8;

    return 0xFF000000 | (br & 0xFF00FF) | (g & 0x00FF00);
}