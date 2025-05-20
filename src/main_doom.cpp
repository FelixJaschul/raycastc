#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

// SDL2 includes
#include <SDL2/SDL.h>

#define ASSERT(_e, ...)                                                        \
  if (!(_e)) {                                                                 \
    fprintf(stderr, __VA_ARGS__);                                              \
    exit(1);                                                                   \
  }

typedef float f32;
typedef double f64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef size_t usize;

constexpr bool downscaled = false;

constexpr f32 PI = 3.14159265359f;
constexpr f32 TAU = 2.0f * PI;
constexpr f32 PI_2 = PI / 2.0f;
constexpr f32 PI_4 = PI / 4.0f;

constexpr f32 deg_to_rad(f32 d) { return d * (PI / 180.0f); }
constexpr f32 rad_to_deg(f32 d) { return d * (180.0f / PI); }

constexpr int SCREEN_WIDTH = downscaled ? 384 : 1280;
constexpr int SCREEN_HEIGHT = downscaled ? 216 : 720;

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;

constexpr f32 EYE_Z = 1.65f;
constexpr f32 HFOV = deg_to_rad(90.0f);
constexpr f32 VFOV = 0.5f;

constexpr f32 ZNEAR = 0.0001f;
constexpr f32 ZFAR = 128.0f;

const char *LEVEL_FILE = "res/level.txt";

struct v2 {
  f32 x, y;
};
struct v2i {
  i32 x, y;
};

inline v2i to_v2i(const v2 &v) {
  return {static_cast<i32>(v.x), static_cast<i32>(v.y)};
}
inline v2 to_v2(const v2i &v) {
  return {static_cast<f32>(v.x), static_cast<f32>(v.y)};
}

inline f32 dot(const v2 &v0, const v2 &v1) {
  return (v0.x * v1.x) + (v0.y * v1.y);
}

inline f32 length(const v2 &vl) { return std::sqrt(dot(vl, vl)); }
inline v2 normalize(const v2 &vn) {
  const f32 l = length(vn);
  if (l == 0.0f)
    return {0.0f, 0.0f}; // avoid division by zero
  return {vn.x / l, vn.y / l};
}

// using std::min, std::max, std::clamp from <algorithm>
// #define min(_a, _b) ... -> use std::min
// #define max(_a, _b) ... -> use std::max
// #define clamp(_x, _mi, _ma) ... -> use std::clamp

inline f32 ifnan(f32 x, f32 alt) { return std::isnan(x) ? alt : x; }

// -1 right, 0 on, 1 left
inline f32 point_side(const v2 &p, const v2 &a, const v2 &b) {
  return -(((p.x - a.x) * (b.y - a.y)) - ((p.y - a.y) * (b.x - a.x)));
}

// rotate vector v by angle a
static v2 rotate(const v2 v, const f32 a) {
  return {
      v.x * std::cos(a) - v.y * std::sin(a),
      v.x * std::sin(a) + v.y * std::cos(a),
  };
}

// see: https://en.wikipedia.org/wiki/Line–line_intersection
// compute intersection of two-line segments, returns {NAN, NAN} if there is
// no intersection.
static v2 intersect_segs(const v2 a0, const v2 a1, const v2 b0, const v2 b1) {
  const f32 d = (a0.x - a1.x) * (b0.y - b1.y) - (a0.y - a1.y) * (b0.x - b1.x);

  if (std::fabs(d) < 0.000001f) {
    return {NAN, NAN};
  }

  const f32 t = ((a0.x - b0.x) * (b0.y - b1.y) -
                 (a0.y - b0.y) * (b0.x - b1.x)) /
                d,
            u = ((a0.x - b0.x) * (a0.y - a1.y) -
                 (a0.y - b0.y) * (a0.x - a1.x)) /
                d;
  return (t >= 0 && t <= 1 && u >= 0 && u <= 1)
             ? v2{a0.x + t * (a1.x - a0.x), a0.y + t * (a1.y - a0.y)}
             : v2{NAN, NAN};
}

static u32 abgr_mul(const u32 col, const u32 a) {
  const u32 br = ((col & 0xFF00FF) * a) >> 8, g = ((col & 0x00FF00) * a) >> 8;

  return 0xFF000000 | (br & 0xFF00FF) | (g & 0x00FF00);
}

struct Wall { // Renamed from struct wall
  v2i a, b;
  int portal;
};

// sector id for "no sector"
constexpr int SECTOR_NONE = 0;
constexpr int SECTOR_MAX = 128;

struct Sector {
  int id;
  usize firstwall, nwalls;
  f32 zfloor, zceil;
};

struct GlobalState {
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *texture, *debug;
  u32 *pixels;
  bool quit;

  struct {
    Sector arr[32];
    usize n;
  } sectors;

  struct {
    Wall arr[128];
    usize n;
  } walls;

  u16 y_lo[SCREEN_WIDTH], y_hi[SCREEN_WIDTH];

  struct {
    v2 pos;
    f32 angle, anglecos, anglesin;
    int sector;
  } camera;

  struct {
    bool mode;
  } dev;

  bool sleepy;
};

static GlobalState state; // global instance

// convert angle in [-(HFOV / 2)..+(HFOV / 2)] to X coordinate
static int screen_angle_to_x(const f32 angle) {
  return SCREEN_WIDTH / 2 *
         (1.0f - std::tan((angle + HFOV / 2.0f) / HFOV * PI_2 - PI_4));
}

// normalize angle to +/-PI
static f32 normalize_angle(const f32 a) {
  return a - TAU * std::floor((a + PI) / TAU);
}

// world space -> camera space (translate and rotate)
static v2 world_pos_to_camera(const v2 p) {
  const v2 u = {p.x - state.camera.pos.x, p.y - state.camera.pos.y};
  return {
      u.x * state.camera.anglesin - u.y * state.camera.anglecos,
      u.x * state.camera.anglecos + u.y * state.camera.anglesin,
  };
}

static void present(); // forward declaration

// load sectors from file -> state
static int load_sectors(const char *path) {
  // sector 0 does not exist
  state.sectors.n = 1;
  state.walls.n = 0; // initialize walls count

  FILE *f = fopen(path, "r");
  if (!f)
    return -1; // file cant be opened

  int retval = 0;

  enum ScanState { // renamed enum
    SCAN_SECTOR,
    SCAN_WALL,
    SCAN_NONE
  };
  ScanState ss = SCAN_NONE;

  char line[1024], buf[64];
  while (fgets(line, sizeof(line), f)) {
    const char *p = line;
    while (isspace(
        static_cast<unsigned char>(*p))) { // cast to unsigned char for isspace
      p++;
    }

    // skip line, empty or comment
    if (*p && *p != '#') {
      if (*p == '[') {
        strncpy(buf, p + 1, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0'; // ensure null termination
        char *section_end = strchr(buf, ']');
        if (section_end) {
          *section_end = '\0'; // terminate at ']'
        } else {
          retval = -2; // invalid section format (no closing ']')
          goto done;
        }
        const char *section = buf; // strtok modifies buf, use directly

        if (!strcmp(section, "SECTOR"))
          ss = SCAN_SECTOR;
        else if (!strcmp(section, "WALL")) {
          ss = SCAN_WALL;
        } else {
          retval = -3; // unknown section
          goto done;
        }
      } else {
        switch (ss) {
        case SCAN_WALL: {
          if (state.walls.n >=
              sizeof(state.walls.arr) / sizeof(state.walls.arr[0])) {
            retval = -7; // too many walls
            goto done;
          }
          Wall *wall = &state.walls.arr[state.walls.n++];
          if (sscanf(p, "%d %d %d %d %d", &wall->a.x, &wall->a.y, &wall->b.x,
                     &wall->b.y, &wall->portal) != 5) {
            retval = -4; // invalid wall data format
            goto done;
          }
        } break;
        case SCAN_SECTOR: {
          if (state.sectors.n >=
              sizeof(state.sectors.arr) / sizeof(state.sectors.arr[0])) {
            retval = -8; // too many sectors
            goto done;
          }
          Sector *sector = &state.sectors.arr[state.sectors.n++];
          if (sscanf(p, "%d %zu %zu %f %f", &sector->id, &sector->firstwall,
                     &sector->nwalls, &sector->zfloor, &sector->zceil) != 5) {
            retval = -5; // invalid sector data format
            goto done;
          }
        } break;
        default:
          retval = -6; // parsing data out of recognized section
          goto done;
        }
      }
	}
  }

  if (ferror(f))
    retval = -128; // file read error
done:
  fclose(f);
  return retval;
}

static void verline(const int x, const int y0, const int y1, const u32 color) {
  for (int y = y0; y <= y1; y++)
    state.pixels[y * SCREEN_WIDTH + x] = color;
}

// the point is in sector if it is on the left side of all walls
static bool point_in_sector(const Sector *sector, v2 p) {
  for (usize i = 0; i < sector->nwalls; i++) {
    const Wall *wall = &state.walls.arr[sector->firstwall + i];
    if (point_side(p, to_v2(wall->a), to_v2(wall->b)) > 0)
      return false;
  }
  return true;
}

static void render() {
  for (int i = 0; i < SCREEN_WIDTH; i++) {
    state.y_hi[i] = SCREEN_HEIGHT - 1;
    state.y_lo[i] = 0;
  }

  bool sectdraw[SECTOR_MAX];
  std::fill(std::begin(sectdraw), std::end(sectdraw), false);

  const v2 zdl = rotate({0.0f, 1.0f}, +(HFOV / 2.0f)),
           zdr = rotate({0.0f, 1.0f}, -(HFOV / 2.0f)),
           znl = {zdl.x * ZNEAR, zdl.y * ZNEAR},
           znr = {zdr.x * ZNEAR, zdr.y * ZNEAR},
           zfl = {zdl.x * ZFAR, zdl.y * ZFAR},
           zfr = {zdr.x * ZFAR, zdr.y * ZFAR};

  constexpr usize QUEUE_MAX = 64;
  struct QueueEntry {
    int id;
    int x0;
    int x1;
  };

  struct {
    QueueEntry arr[QUEUE_MAX];
    usize n;
  } queue = {{{state.camera.sector, 0, SCREEN_WIDTH - 1}}, 1};

  while (queue.n != 0) {
    QueueEntry entry = queue.arr[--queue.n];

    if (entry.id < 0 || entry.id >= SECTOR_MAX || sectdraw[entry.id])
      continue;

    sectdraw[entry.id] = true;

    const Sector *sector = &state.sectors.arr[entry.id];

    for (usize i = 0; i < sector->nwalls; i++) {
      const Wall *wall = &state.walls.arr[sector->firstwall + i];

      const v2 op0 = world_pos_to_camera(to_v2(wall->a)),
               op1 = world_pos_to_camera(to_v2(wall->b));

      v2 cp0 = op0, cp1 = op1;

      if (cp0.y <= 0 && cp1.y <= 0)
        continue;

      f32 ap0 = normalize_angle(std::atan2(cp0.y, cp0.x) - PI_2),
          ap1 = normalize_angle(std::atan2(cp1.y, cp1.x) - PI_2);

      if (cp0.y < ZNEAR || cp1.y < ZNEAR || ap0 > +(HFOV / 2) ||
          ap1 < -(HFOV / 2)) {
        const v2 il = intersect_segs(cp0, cp1, znl, zfl),
                 ir = intersect_segs(cp0, cp1, znr, zfr);

        if (!std::isnan(
                il.x)) { // check against il.x, as il.y would also be NaN
          cp0 = il;
          ap0 = normalize_angle(std::atan2(cp0.y, cp0.x) - PI_2);
        }

        if (!std::isnan(ir.x)) {
          cp1 = ir;
          ap1 = normalize_angle(std::atan2(cp1.y, cp1.x) - PI_2);
        }
      }

      if (ap0 < ap1)
        continue;

      if ((ap0 < -(HFOV / 2) && ap1 < -(HFOV / 2)) ||
          (ap0 > +(HFOV / 2) && ap1 > +(HFOV / 2))) {
        continue;
      }

      const int tx0 = screen_angle_to_x(ap0), tx1 = screen_angle_to_x(ap1);

      if (tx0 > entry.x1) {
        continue;
      }
      if (tx1 < entry.x0) {
        continue;
      }

      // for a quick port, using the original logic even if it seems odd:
      const int wallshade =
          16 * (std::sin(std::atan2(static_cast<f32>(wall->b.x - wall->a.x),
                                    static_cast<f32>(wall->b.y - wall->a.y))) +
                1.0f);

      const int x0 = std::clamp(tx0, entry.x0, entry.x1),
                x1 = std::clamp(tx1, entry.x0, entry.x1);

      const f32 z_floor = sector->zfloor, z_ceil = sector->zceil,
                nz_floor = wall->portal != SECTOR_NONE
                               ? state.sectors.arr[wall->portal].zfloor
                               : 0,
                nz_ceil = wall->portal != SECTOR_NONE
                              ? state.sectors.arr[wall->portal].zceil
                              : 0;

      const f32 sy0 = ifnan((VFOV * SCREEN_HEIGHT) / cp0.y, 1e10f),
                sy1 = ifnan((VFOV * SCREEN_HEIGHT) / cp1.y, 1e10f);

      const int
          yf0 = SCREEN_HEIGHT / 2 + static_cast<int>((z_floor - EYE_Z) * sy0),
          yc0 = SCREEN_HEIGHT / 2 + static_cast<int>((z_ceil - EYE_Z) * sy0),
          yf1 = SCREEN_HEIGHT / 2 + static_cast<int>((z_floor - EYE_Z) * sy1),
          yc1 = SCREEN_HEIGHT / 2 + static_cast<int>((z_ceil - EYE_Z) * sy1),
          nyf0 = SCREEN_HEIGHT / 2 + static_cast<int>((nz_floor - EYE_Z) * sy0),
          nyc0 = SCREEN_HEIGHT / 2 + static_cast<int>((nz_ceil - EYE_Z) * sy0),
          nyf1 = SCREEN_HEIGHT / 2 + static_cast<int>((nz_floor - EYE_Z) * sy1),
          nyc1 = SCREEN_HEIGHT / 2 + static_cast<int>((nz_ceil - EYE_Z) * sy1),
          txd = tx1 - tx0, yfd = yf1 - yf0, ycd = yc1 - yc0, nyfd = nyf1 - nyf0,
          nycd = nyc1 - nyc0;

      for (int x = x0; x <= x1; x++) {
        int shade = (x == x0 || x == x1) ? 192 : 255 - wallshade;

        const f32 xp =
            ifnan(txd == 0 ? 0.0f : (x - tx0) / static_cast<f32>(txd), 0.0f);

        const int tyf = static_cast<int>(xp * yfd) + yf0,
                  tyc = static_cast<int>(xp * ycd) + yc0,
                  yf = std::clamp(tyf, static_cast<int>(state.y_lo[x]),
                                  static_cast<int>(state.y_hi[x])),
                  yc = std::clamp(tyc, static_cast<int>(state.y_lo[x]),
                                  static_cast<int>(state.y_hi[x]));

        // floor
        if (yf > state.y_lo[x]) {
          verline(x, state.y_lo[x], yf, 0xFFFF0000); // red for floor
        }

        // celing
        if (yc < state.y_hi[x]) {
          verline(x, yc, state.y_hi[x], 0xFF00FFFF); // magenta
        }

        if (wall->portal != SECTOR_NONE) {
          const int tnyf = static_cast<int>(xp * nyfd) + nyf0,
                    tnyc = static_cast<int>(xp * nycd) + nyc0,
                    nyf = std::clamp(tnyf, static_cast<int>(state.y_lo[x]),
                                     static_cast<int>(state.y_hi[x])),
                    nyc = std::clamp(tnyc, static_cast<int>(state.y_lo[x]),
                                     static_cast<int>(state.y_hi[x]));

          // draw upper part of portal wall
          verline(x, nyc, yc, abgr_mul(0xFF00FF00, shade)); // green
          // draw lower part of portal wall
          verline(x, yf, nyf, abgr_mul(0xFF0000FF, shade)); // blue

          state.y_hi[x] = std::clamp(
              std::min(std::min(yc, nyc), static_cast<int>(state.y_hi[x])), 0,
              SCREEN_HEIGHT - 1);

          state.y_lo[x] = std::clamp(
              std::max(std::max(yf, nyf), static_cast<int>(state.y_lo[x])), 0,
              SCREEN_HEIGHT - 1);
        } else {
          // solid wall
          verline(x, yf, yc, abgr_mul(0xFFD0D0D0, shade)); // grey
        }

        if (state.sleepy) {
          present();
          SDL_Delay(10);
        }
      }

      if (wall->portal != SECTOR_NONE) {
        ASSERT(queue.n != QUEUE_MAX, "out of queue space");
        queue.arr[queue.n++] = {wall->portal, x0, x1};
      }
    }
  }
  state.sleepy = false;
}

static void draw_pixel(int x, int y, u32 color) {
  if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
    state.pixels[y * SCREEN_WIDTH + x] = color;
  }
}

static void draw_line(int x0, int y0, int x1, int y1, u32 color) {
  int dx = std::abs(x1 - x0);
  int dy = -std::abs(y1 - y0);
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;

  while (true) {
    draw_pixel(x0, y0, color);
    if (x0 == x1 && y0 == y1)
      break;
    int e2 = 2 * err;
    if (e2 >= dy) {
      if (x0 == x1)
        break;
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      if (y0 == y1)
        break;
      err += dx;
      y0 += sy;
    }
  }
}

static void draw_circle(int x0, int y0, int radius, u32 color) {
  int x = radius;
  int y = 0;
  int err = 0;

  while (x >= y) {
    draw_pixel(x0 + x, y0 + y, color);
    draw_pixel(x0 + y, y0 + x, color);
    draw_pixel(x0 - y, y0 + x, color);
    draw_pixel(x0 - x, y0 + y, color);
    draw_pixel(x0 - x, y0 - y, color);
    draw_pixel(x0 - y, y0 - x, color);
    draw_pixel(x0 + y, y0 - x, color);
    draw_pixel(x0 + x, y0 - y, color);

    y += 1;
    if (err <= 0) {
      err += 2 * y + 1;
    }
    if (err > 0) {
      x -= 1;
      err -= 2 * x + 1;
    }
  }
}

static void render_dev_version() {
  constexpr int scale = downscaled ? 30.0f : 100.0f;
  constexpr int offsetX =
      downscaled ? SCREEN_WIDTH / 2 + 140 : SCREEN_WIDTH / 2 + 400;
  constexpr int offsetY =
      downscaled ? SCREEN_HEIGHT / 2 + 120 : SCREEN_HEIGHT / 2 + 380;

  // draw all walls
  for (usize i = 0; i < state.walls.n; i++) {
    const Wall *wall = &state.walls.arr[i];

    // transform world coordinates to map coordinates (note y-axis inversion for
    // screen)
    int x0_map = offsetX - static_cast<int>(wall->a.x * scale);
    int y0_map = offsetY - static_cast<int>(wall->a.y * scale);
    int x1_map = offsetX - static_cast<int>(wall->b.x * scale);
    int y1_map = offsetY - static_cast<int>(wall->b.y * scale);

    u32 color = (wall->portal != SECTOR_NONE)
                    ? 0xFF00FF00
                    : 0xFFFFFFFF; // green for portals, white for walls
    draw_line(x0_map, y0_map, x1_map, y1_map, color);
  }

  // Draw player position and direction
  int playerX_map = offsetX - static_cast<int>(state.camera.pos.x * scale);
  int playerY_map = offsetY - static_cast<int>(state.camera.pos.y * scale);

  draw_circle(playerX_map, playerY_map, 3, 0xFFFF0000); // red circle for player

  int dirX_map = playerX_map - static_cast<int>(state.camera.anglecos * 10);
  int dirY_map = playerY_map - static_cast<int>(state.camera.anglesin * 10);
  draw_line(playerX_map, playerY_map, dirX_map, dirY_map, 0xFFFF0000);
}

static void present() {
  void *px;
  int pitch;
  SDL_LockTexture(state.texture, nullptr, &px, &pitch);
  {
    for (usize y = 0; y < SCREEN_HEIGHT; y++) {
      memcpy(&static_cast<u8 *>(px)[y * pitch], &state.pixels[y * SCREEN_WIDTH],
             SCREEN_WIDTH * 4);
    }
  }
  SDL_UnlockTexture(state.texture);

  SDL_SetRenderTarget(state.renderer, nullptr);
  SDL_SetRenderDrawColor(state.renderer, 0, 0, 0, 0xFF);
  SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_NONE);

  SDL_RenderClear(state.renderer);
  SDL_RenderCopyEx(state.renderer, state.texture, nullptr, nullptr, 0.0,
                   nullptr, SDL_FLIP_VERTICAL);

  SDL_RenderPresent(state.renderer);
}

int main(int argc, char *argv[]) {
  ASSERT(!SDL_Init(SDL_INIT_VIDEO), "SDL failed to initialize: %s",
         SDL_GetError());

  state.window = SDL_CreateWindow(
      "raycast_cpp", SDL_WINDOWPOS_CENTERED_DISPLAY(0),
      SDL_WINDOWPOS_CENTERED_DISPLAY(0), WINDOW_WIDTH, WINDOW_HEIGHT, 0);
  ASSERT(state.window, "failed to create SDL window: %s\n", SDL_GetError());

  state.renderer = SDL_CreateRenderer(
      state.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  ASSERT(state.renderer, "failed to create SDL renderer: %s\n", SDL_GetError());

  state.texture = SDL_CreateTexture(state.renderer, SDL_PIXELFORMAT_ABGR8888,
                                    SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH,
                                    SCREEN_HEIGHT);
  ASSERT(state.texture, "failed to create SDL texture: %s\n", SDL_GetError());

  state.pixels = new u32[SCREEN_WIDTH * SCREEN_HEIGHT];
  ASSERT(state.pixels, "failed to allocate pixel buffer\n");

  state.camera.pos = {3.0f, 3.0f};
  state.camera.angle = 0.0f;
  state.camera.sector = 1; // default sector

  state.dev.mode = false; // default to non-dev mode
  state.quit = false;
  state.sleepy = false;

  int retval = 0;
  retval = load_sectors(LEVEL_FILE);
  ASSERT(retval == 0, "error while loading sectors: %d\n", retval);
  printf("loaded %zu sectors with %zu walls\n",
         state.sectors.n - 1, // state.sectors.n includes the dummy sector 0
         state.walls.n);

  while (!state.quit) {
	int mouseX, mouseY;
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      switch (ev.type) {
      case SDL_QUIT:
        state.quit = true;
        break;
      default:
        break;
      }
    }

    if (state.quit)
      break;

    const f32 rot_speed = 3.0f * 0.016f, move_speed = 3.0f * 0.016f;

    const u8 *keystate = SDL_GetKeyboardState(nullptr);

    if (keystate[SDL_SCANCODE_RIGHT])
      state.camera.angle -= rot_speed;
    if (keystate[SDL_SCANCODE_D]) {
      state.camera.pos.x += move_speed * state.camera.anglesin;
      state.camera.pos.y -= move_speed * state.camera.anglecos;
    }
    if (keystate[SDL_SCANCODE_LEFT])
      state.camera.angle += rot_speed;
    if (keystate[SDL_SCANCODE_A]) {
      state.camera.pos.x -= move_speed * state.camera.anglesin;
      state.camera.pos.y += move_speed * state.camera.anglecos;
    }

    state.camera.anglecos = std::cos(state.camera.angle);
    state.camera.anglesin = std::sin(state.camera.angle);

    if (keystate[SDL_SCANCODE_UP] || keystate[SDL_SCANCODE_W]) {
      state.camera.pos.x += move_speed * state.camera.anglecos;
      state.camera.pos.y += move_speed * state.camera.anglesin;
    }
    if (keystate[SDL_SCANCODE_DOWN] || keystate[SDL_SCANCODE_S]) {
      state.camera.pos.x -= move_speed * state.camera.anglecos;
      state.camera.pos.y -= move_speed * state.camera.anglesin;
    }

    if (keystate[SDL_SCANCODE_F1])
      state.sleepy = true;
    if (keystate[SDL_SCANCODE_F2])
      state.dev.mode = true;
    if (keystate[SDL_SCANCODE_F3])
      state.dev.mode = false;

    // update player sector
    {
      constexpr int PLAYER_SECTOR_QUEUE_MAX = 64;
      int queue[PLAYER_SECTOR_QUEUE_MAX] = {state.camera.sector},
          head = 0,  // head for circular queue
          tail = 0,  // tail for circular queue
          count = 0; // number of elements in queue

      // Add initial sector
      queue[tail] = state.camera.sector;
      tail = (tail + 1) % PLAYER_SECTOR_QUEUE_MAX;
      count = 1;

      int found_sector = SECTOR_NONE;
      bool visited[SECTOR_MAX] = {false};

      while (count > 0) {
        const int id = queue[head];
        head = (head + 1) % PLAYER_SECTOR_QUEUE_MAX;
        count--;

        if (id < 1 || id >= static_cast<int>(state.sectors.n) || visited[id]) {
          continue;
        }
        visited[id] = true;

        const Sector *sector = &state.sectors.arr[id];
        if (point_in_sector(sector, state.camera.pos)) {
          found_sector = id;
          break;
        }

        for (usize j = 0; j < sector->nwalls; j++) {
          const Wall *wall = &state.walls.arr[sector->firstwall + j];
          if (wall->portal != SECTOR_NONE) {
            if (count == PLAYER_SECTOR_QUEUE_MAX) {
              fprintf(stderr, "Player sector update: out of queue space!\n");
              goto player_sector_done;
            }
            if (wall->portal > 0 && wall->portal < SECTOR_MAX &&
                !visited[wall->portal]) {
              queue[tail] = wall->portal;
              tail = (tail + 1) % PLAYER_SECTOR_QUEUE_MAX;
              count++;
            }
          }
        }
      }
    player_sector_done:
      if (found_sector == SECTOR_NONE) {
        // fallback: if player is not in any reachable sector (e.g. noclip out
        // of map) Try checking all sectors (less efficient but robust)
        bool truly_lost = true;
        for (usize k = 1; k < state.sectors.n; ++k) {
          if (point_in_sector(&state.sectors.arr[k], state.camera.pos)) {
            state.camera.sector = state.sectors.arr[k].id;
            truly_lost = false;
            break;
          }
        }
        if (truly_lost)
          state.camera.sector = 1; // default to sector 1 if completely lost
      } else {
        state.camera.sector = found_sector;
      }
    }

    memset(state.pixels, 0, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(u32));

    if (state.dev.mode) {
      render();
      render_dev_version(); // overlay dev map
    } else {
      render();
    }

    if (!state.sleepy)
      present();
  }

  delete[] state.pixels;
  SDL_DestroyTexture(state.texture);
  SDL_DestroyRenderer(state.renderer);
  SDL_DestroyWindow(state.window);
  SDL_Quit();
  return 0;
}
