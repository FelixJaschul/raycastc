#pragma once

#define ASSERT(_e, ...) do { if (!(_e)) { fprintf(stderr, __VA_ARGS__); exit(1); } } while (0)
#define MIN(a, b)       ((a) < (b) ? (a) : (b))
#define MAX(a, b)       ((a) > (b) ? (a) : (b))
#define CLAMP(x, a, b)  (MIN(MAX((x), (a)), (b)))
#define ABS(x)          ((x) < 0 ? -(x) : (x))
#define SQR(x)          ((x) * (x))