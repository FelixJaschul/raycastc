#pragma once

// normalize angle to +/-PI
#define normalize_angle(a) ({ a - (TAU * floor((a + PI) / TAU)); });

// -1 right, 0 on, 1 left
#define point_side(_p, _a, _b) ({ -(((_p.x - _a.x) * (_b.y - _a.y)) - ((_p.y - _a.y) * (_b.x - _a.x))); })