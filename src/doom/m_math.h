#pragma once

#define DEG2RAD(_d) ((_d) * (PI / 180.0f))
#define RAD2DEG(_d) ((_d) * (180.0f / PI))

#define v2_to_v2i(_v) ({ (v2i) { _v.x, _v.y }; })
#define v2i_to_v2(_v) ({ (v2) { _v.x, _v.y }; })

#define dot(_v0, _v1) ({ (_v0.x * _v1.x) + (_v0.y * _v1.y); })
#define length(_vl) ({ sqrtf(dot(_vl, _vl)); })
#define normalize(_vn) ({ const f32 l = length(_vn); (__typeof__(_vn)) { _vn.x / l, _vn.y / l }; })
#define min(_a, _b) ({ _a < _b ? _a : _b; })
#define max(_a, _b) ({ _a > _b ? _a : _b; })
#define clamp(_x, _mi, _ma) (min(max(_x, _mi), _ma))
#define ifnan(_x, _alt) ({ isnan(_x) ? (_alt) : _x; })