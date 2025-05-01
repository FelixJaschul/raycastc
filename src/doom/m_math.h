#pragma once

#define PI 3.14159265359f
#define TAU (2.0f * PI)
#define PI_2 (PI / 2.0f)
#define PI_4 (PI / 4.0f)

#define DEG2RAD(_d) ((_d) * (PI / 180.0f))
#define RAD2DEG(_d) ((_d) * (180.0f / PI))

#define v2_to_v2i(_v) ({ __typeof__(_v) __v = (_v); (v2i) { __v.x, __v.y }; })
#define v2i_to_v2(_v) ({ __typeof__(_v) __v = (_v); (v2) { __v.x, __v.y }; })

#define dot(_v0, _v1) ({ __typeof__(_v0) __v0 = (_v0), __v1 = (_v1); \
		(__v0.x * __v1.x) + (__v0.y * __v1.y); })
#define length(_vl) ({ __typeof__(_vl) __vl = (_vl); sqrtf(dot(__vl, __vl)); })
#define normalize(_vn) ({ __typeof__(_vn) __vn = (_vn); const f32 l = length(__vn); \
		(__typeof__(_vn)) { __vn.x / l, __vn.y / l }; })
#define min(_a, _b) ({ __typeof__(_a) __a = (_a), __b = (_b); __a < __b ? __a : __b; })
#define max(_a, _b) ({ __typeof__(_a) __a = (_a), __b = (_b); __a > __b ? __a : __b; })
#define clamp(_x, _mi, _ma) (min(max(_x, _mi), _ma))
#define ifnan(_x, _alt) ({ __typeof__(_x) __x = (_x); isnan(__x) ? (_alt) : __x; })

// -1 right, 0 on, 1 left
#define point_side(_p, _a, _b) ({ __typeof__(_p) __p = (_p), __a = (_a), __b = (_b); \
        -(((__p.x - __a.x) * (__b.y - __a.y)) - ((__p.y - __a.y) * (__b.x - __a.x))); })

// rotate vector v by angle a
#define rotate(v, a) ({ (v.x * cos(a)) - (v.y * sin(a)), (v.x * sin(a)) + (v.y * cos(a)); })