#pragma once

struct wall {
    v2i a, b;
    int portal;
};

struct sector {
    int id;
    usize firstwall, nwalls;
    f32 zfloor, zceil;
};