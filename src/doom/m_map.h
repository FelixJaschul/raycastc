#pragma once

// normalize angle to +/-PI
#define normalize_angle(a) ({ a - (TAU * floor((a + PI) / TAU)); });