#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <ctype.h>
#include <string.h>

// SDL2
#include <SDL2/SDL.h>

// CImGui
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#include "imgui/imgui_impl_sdl2.h"
#include "imgui/imgui_impl_sdlrenderer2.h"

#define ASSERT(_e, ...) if (!(_e)) { fprintf(stderr, __VA_ARGS__); exit(1); }

typedef float    f32;
typedef double   f64;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef size_t   usize;
// typedef ssize_t  isize; // ssize_t is POSIX-specific, avoid if not strictly needed or provide alternative for Windows

#define PI 3.14159265359f
#define TAU (2.0f * PI)
#define PI_2 (PI / 2.0f)
#define PI_4 (PI / 4.0f)

#define DEG2RAD(_d) ((_d) * (PI / 180.0f))
#define RAD2DEG(_d) ((_d) * (180.0f / PI))

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

#define GRID_SIZE 1
#define INITIAL_SCALE 40.0f

#define MIN_SCALE 10.f
#define MAX_SCALE 100.0f

#define LEVEL_FILE "res/level.txt" // Make sure "res" directory exists or change path

typedef struct v2_s { f32 x, y; } v2;
typedef struct v2i_s { i32 x, y; } v2i;

#define v2_to_v2i(_v) ({ __typeof__(_v) __v = (_v); (v2i) { (i32)__v.x, (i32)__v.y }; })
#define v2i_to_v2(_v) ({ __typeof__(_v) __v = (_v); (v2) { (f32)__v.x, (f32)__v.y }; })

#define dot(_v0, _v1) ({                                                        \
        __typeof__(_v0) __v0 = (_v0), __v1 = (_v1);                             \
        (__v0.x * __v1.x)                                                       \
            + (__v0.y * __v1.y);                                                \
    })
#define length(_vl) ({ __typeof__(_vl) __vl = (_vl); sqrtf(dot(__vl, __vl)); })
#define normalize(_vn) ({                                                       \
        __typeof__(_vn) __vn = (_vn);                                           \
        const f32 l = length(__vn);                                             \
        (__typeof__(_vn)) { (l == 0.0f ? 0.0f : __vn.x / l), (l == 0.0f ? 0.0f : __vn.y / l) };                           \
    })
#define min(_a, _b) ({ __typeof__(_a) __a = (_a), __b = (_b); __a < __b ? __a : __b; })
#define max(_a, _b) ({ __typeof__(_a) __a = (_a), __b = (_b); __a > __b ? __a : __b; })
#define clamp(_x, _mi, _ma) (min(max(_x, _mi), _ma))

struct wall {
    v2i a, b;
    int portal; // sector id this wall portals to, or SECTOR_NONE
};

#define SECTOR_NONE 0 // sector id for "no sector" (e.g. solid wall)
#define SECTOR_MAX 128

struct sector {
    int id;
    usize firstwall, nwalls;
    f32 zfloor, zceil;
};

typedef enum {
    TOOL_NONE,
    TOOL_SECTOR,  // For creating new sectors
    TOOL_WALL,    // For selecting existing walls (e.g. for portals)
    TOOL_PORTAL,  // For creating portals between a selected wall and a target sector
    TOOL_SELECT,  // General purpose selection of sectors or walls
    TOOL_MOVE     // (Not implemented) For moving points/walls/sectors
} EditorTool;

// PropertyEditor enum is removed as ImGui handles direct property editing.

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    bool quit;

    struct {
        struct sector arr[SECTOR_MAX];
        usize n; // Number of sectors, arr[0] is unused (SECTOR_NONE)
    } sectors;

    struct {
        struct wall arr[256]; // Max walls
        usize n;
    } walls;

    struct {
        v2 offset;
        f32 scale;
        bool panning;
        v2 pan_start; // Mouse position when panning started (screen coords)
        v2 last_mouse_pos; // Last known mouse position (screen coords)
    } camera;

    struct {
        EditorTool current;
        // PropertyEditor prop_editor; // Removed

        int selected_sector; // ID of the currently selected sector, or SECTOR_NONE
        int selected_wall;   // Index of the currently selected wall, or -1

        // For wall/sector creation
        bool is_placing;        // True if currently placing points for a new sector
        v2i start_point;       // First point of a new sector (not actively used here, temp_points[0] is)
        v2i temp_points[32];   // Temporary points for new sector creation
        int temp_point_count;
    } editor;

    // TTF_Font *font; // Removed

    bool show_grid;
    // bool show_help; // Removed
    bool unsaved_changes;
} EditorState;

EditorState state;

// Forward declarations
static int save_sectors(const char *path);
static int load_sectors(const char *path);
static void render();
static void process_input();
static void init_editor();

// Convert world coordinates to screen coordinates
v2i world_to_screen(const v2 world_pos) {
    return (v2i) {
        (int)((world_pos.x - state.camera.offset.x) * state.camera.scale + WINDOW_WIDTH / 2),
        (int)((world_pos.y - state.camera.offset.y) * state.camera.scale + WINDOW_HEIGHT / 2)
    };
}

// Convert screen coordinates to world coordinates
v2 screen_to_world(const v2i screen_pos) {
    return (v2) {
        (screen_pos.x - WINDOW_WIDTH / 2) / state.camera.scale + state.camera.offset.x,
        (screen_pos.y - WINDOW_HEIGHT / 2) / state.camera.scale + state.camera.offset.y
    };
}

// Snap to grid
v2i snap_to_grid(const v2 world_pos) {
    // Corrected snapping: round to nearest grid intersection
    const int grid_x = (int)roundf(world_pos.x / GRID_SIZE) * GRID_SIZE;
    const int grid_y = (int)roundf(world_pos.y / GRID_SIZE) * GRID_SIZE;
    return (v2i){ grid_x, grid_y };
}

// Find which sector contains a point (current: convex polygon test)
int find_sector_at_point(const v2 point) {
    for (int i = 1; i < state.sectors.n; i++) { // Start from 1, sector 0 is SECTOR_NONE
        const struct sector *sector = &state.sectors.arr[i];
        if (sector->nwalls < 3) continue; // Not a valid polygon

        bool inside = true;
        for (usize j = 0; j < sector->nwalls; j++) {
            const struct wall *wall = &state.walls.arr[sector->firstwall + j];
            const v2 a = v2i_to_v2(wall->a);
            const v2 b = v2i_to_v2(wall->b);

            // Check if point is on the "inside" (e.g., left) of the directed edge (a,b)
            // Assumes consistent winding order for sector walls (e.g., counter-clockwise)
            const float side = (b.x - a.x) * (point.y - a.y) - (b.y - a.y) * (point.x - a.x);

            // If sectors are defined CCW, a point is inside if it's to the left of all edges (side > 0 or >=0 for on_edge)
            // If sectors are defined CW,  a point is inside if it's to the right of all edges (side < 0 or <=0 for on_edge)
            // The original code used `if (side > 0) inside = false;`
            // Let's assume CCW winding for sectors, so point must be to the left or on the edge.
            // side > 0 means point is to the left. side < 0 means point is to the right.
            // For CCW polygon, if point is to the right of any edge, it's outside.
            if (side < 0.0f) { // If point is to the right of any CCW edge
                inside = false;
                break;
            }
        }

        if (inside) {
            return i; // Return sector ID
        }
    }
    return SECTOR_NONE;
}


// Find the closest wall to a point
int find_closest_wall(const v2 point, float *closest_dist_out) {
    int closest_wall_idx = -1;
    float min_dist_sq = INFINITY;

    for (usize i = 0; i < state.walls.n; i++) {
        const struct wall *wall = &state.walls.arr[i];
        const v2 a = v2i_to_v2(wall->a);
        const v2 b = v2i_to_v2(wall->b);

        v2 ab = {b.x - a.x, b.y - a.y};
        v2 ap = {point.x - a.x, point.y - a.y};

        const float ab_len_sq = dot(ab, ab);
        if (ab_len_sq == 0.0f) { // Points are coincident
            v2 dist_vec = {point.x - a.x, point.y - a.y};
            const float dist_sq = dot(dist_vec, dist_vec);
            if (dist_sq < min_dist_sq) {
                min_dist_sq = dist_sq;
                closest_wall_idx = i;
            }
            continue;
        }

        const float dot_prod = dot(ap, ab);
        const float t = clamp(dot_prod / ab_len_sq, 0.0f, 1.0f);

        const v2 closest_point_on_segment = {a.x + t * ab.x, a.y + t * ab.y};
        v2 dist_vec = {point.x - closest_point_on_segment.x, point.y - closest_point_on_segment.y};
        const float dist_sq = dot(dist_vec, dist_vec);

        if (dist_sq < min_dist_sq) {
            min_dist_sq = dist_sq;
            closest_wall_idx = i;
        }
    }

    if (closest_dist_out) {
        if (closest_wall_idx != -1) {
            *closest_dist_out = sqrtf(min_dist_sq);
        } else {
            *closest_dist_out = INFINITY;
        }
    }
    return closest_wall_idx;
}

// Create a new sector with given walls
void create_sector(const v2i *points, const int point_count) {
    if (point_count < 3) {
        printf("Cannot create sector: needs at least 3 points.\n");
        return;
    }
    if (state.sectors.n >= SECTOR_MAX) {
        printf("Max sector limit reached (%d)\n", SECTOR_MAX);
        return;
    }

    const int sector_id = state.sectors.n; // Next available sector ID

    struct sector *new_sector = &state.sectors.arr[sector_id];
    new_sector->id = sector_id;
    new_sector->firstwall = state.walls.n;
    new_sector->nwalls = point_count;
    new_sector->zfloor = 0.0f;
    new_sector->zceil = 3.0f;

    for (int i = 0; i < point_count; i++) {
        if (state.walls.n >= sizeof(state.walls.arr) / sizeof(state.walls.arr[0])) {
            printf("Max wall limit reached. Sector partially created.\n");
            new_sector->nwalls = i; // Adjust nwalls for partially created sector
            if (i == 0) state.sectors.n--; // No walls created, rollback sector count
            return;
        }
        struct wall *new_wall = &state.walls.arr[state.walls.n++];
        new_wall->a = points[i];
        new_wall->b = points[(i + 1) % point_count]; // Loop back for the last wall
        new_wall->portal = SECTOR_NONE;
    }

    state.sectors.n++;
    state.unsaved_changes = true;
    printf("Sector %d created with %d walls.\n", sector_id, point_count);
}

// Create a portal between two sectors
void create_portal(const int wall_idx, const int target_sector_id) {
    if (wall_idx < 0 || wall_idx >= (int)state.walls.n) {
        printf("Portal creation failed: Invalid wall index %d.\n", wall_idx);
        return;
    }
    if (target_sector_id <= 0 || target_sector_id >= (int)state.sectors.n) {
        printf("Portal creation failed: Invalid target sector ID %d.\n", target_sector_id);
        return;
    }

    struct wall *wall1 = &state.walls.arr[wall_idx];

    // Find which sector wall1 belongs to
    int source_sector_id = SECTOR_NONE;
    for (int i = 1; i < state.sectors.n; i++) {
        const struct sector *s = &state.sectors.arr[i];
        if (wall_idx >= (int)s->firstwall && wall_idx < (int)(s->firstwall + s->nwalls)) {
            source_sector_id = s->id;
            break;
        }
    }

    if (source_sector_id == SECTOR_NONE) {
        printf("Portal creation failed: Wall %d does not belong to any sector.\n", wall_idx);
        return;
    }
    if (source_sector_id == target_sector_id) {
        printf("Portal creation failed: Cannot portal a sector to itself.\n");
        return;
    }

    // Check if there's a matching wall in the target sector
    const struct sector *target_s = &state.sectors.arr[target_sector_id];
    bool found_match = false;

    for (usize i = 0; i < target_s->nwalls; i++) {
        struct wall *wall2 = &state.walls.arr[target_s->firstwall + i];

        // Check if walls are coincident but oppositely wound
        if ((wall1->a.x == wall2->b.x && wall1->a.y == wall2->b.y) &&
            (wall1->b.x == wall2->a.x && wall1->b.y == wall2->a.y)) {

            wall1->portal = target_sector_id;
            wall2->portal = source_sector_id;
            found_match = true;
            printf("Portal created between wall %d (Sector %d) and wall %zu (Sector %d).\n",
                   wall_idx, source_sector_id, target_s->firstwall + i, target_sector_id);
            break;
        }
    }

    if (found_match) {
        state.unsaved_changes = true;
    } else {
        printf("Portal creation failed: No matching wall found in target sector %d for wall %d.\n", target_sector_id, wall_idx);
        printf("Wall1: (%d,%d)-(%d,%d)\n", wall1->a.x, wall1->a.y, wall1->b.x, wall1->b.y);
        printf("Target sector %d walls:\n", target_sector_id);
        for (usize i = 0; i < target_s->nwalls; i++) {
             struct wall *wall2 = &state.walls.arr[target_s->firstwall + i];
             printf("  Wall %zu: (%d,%d)-(%d,%d)\n",target_s->firstwall+i, wall2->a.x, wall2->a.y, wall2->b.x, wall2->b.y);
        }
    }
}

// Draw a filled triangle
void draw_filled_triangle(v2i p1_screen, v2i p2_screen, v2i p3_screen, SDL_Color color) {
    // SDL_RenderGeometry can do this more efficiently if available and complex shapes are needed,
    // but for simple triangles, direct drawing or a basic scanline filler is okay.
    // The provided scanline method is kept for simplicity for now.

    SDL_Vertex vertices[] = {
        { { (float)p1_screen.x, (float)p1_screen.y }, color, { 0, 0 } },
        { { (float)p2_screen.x, (float)p2_screen.y }, color, { 0, 0 } },
        { { (float)p3_screen.x, (float)p3_screen.y }, color, { 0, 0 } },
    };
    SDL_RenderGeometry(state.renderer, NULL, vertices, 3, NULL, 0);
}


static void draw_grid() {
    if (!state.show_grid) return;

    const v2 top_left_world = screen_to_world((v2i){0, 0});
    const v2 bottom_right_world = screen_to_world((v2i){WINDOW_WIDTH, WINDOW_HEIGHT});

    const int start_x = (int)floorf(top_left_world.x / GRID_SIZE) * GRID_SIZE;
    const int end_x = (int)ceilf(bottom_right_world.x / GRID_SIZE) * GRID_SIZE;
    const int start_y = (int)floorf(top_left_world.y / GRID_SIZE) * GRID_SIZE;
    const int end_y = (int)ceilf(bottom_right_world.y / GRID_SIZE) * GRID_SIZE;

    SDL_SetRenderDrawColor(state.renderer, 30, 30, 30, 255);

    for (int x = start_x; x <= end_x; x += GRID_SIZE) {
        v2i p1_screen = world_to_screen((v2){(f32)x, (f32)start_y});
        v2i p2_screen = world_to_screen((v2){(f32)x, (f32)end_y});
        SDL_RenderDrawLine(state.renderer, p1_screen.x, p1_screen.y, p2_screen.x, p2_screen.y);
    }
    for (int y = start_y; y <= end_y; y += GRID_SIZE) {
        v2i p1_screen = world_to_screen((v2){(f32)start_x, (f32)y});
        v2i p2_screen = world_to_screen((v2){(f32)end_x, (f32)y});
        SDL_RenderDrawLine(state.renderer, p1_screen.x, p1_screen.y, p2_screen.x, p2_screen.y);
    }

    // Draw origin marker
    v2i origin_screen = world_to_screen((v2){0,0});
    SDL_SetRenderDrawColor(state.renderer, 255, 0, 0, 128); // X-axis (Red)
    SDL_RenderDrawLine(state.renderer, origin_screen.x, origin_screen.y, world_to_screen((v2){(f32)GRID_SIZE*5, 0}).x, origin_screen.y);
    SDL_SetRenderDrawColor(state.renderer, 0, 255, 0, 128); // Y-axis (Green)
    SDL_RenderDrawLine(state.renderer, origin_screen.x, origin_screen.y, origin_screen.x, world_to_screen((v2){0, (f32)GRID_SIZE*5}).y);
}

static void draw_walls_and_sectors() {
    // Draw sectors (filled)
    for (int i = 1; i < state.sectors.n; i++) {
        const struct sector *sector = &state.sectors.arr[i];
        if (sector->nwalls < 3) continue;

        SDL_Color fill_color = {40, 40, 40, 64}; // Default fill
        if (i == state.editor.selected_sector) {
            fill_color = (SDL_Color){100, 100, 200, 100}; // Selected sector fill
        }

        // Simple fan triangulation from the first vertex of the sector
        v2i p1_screen = world_to_screen(v2i_to_v2(state.walls.arr[sector->firstwall].a));
        for (usize j = 1; j < sector->nwalls - 1; j++) {
            v2i p2_screen = world_to_screen(v2i_to_v2(state.walls.arr[sector->firstwall + j].a));
            v2i p3_screen = world_to_screen(v2i_to_v2(state.walls.arr[sector->firstwall + j + 1].a));
            draw_filled_triangle(p1_screen, p2_screen, p3_screen, fill_color);
        }
    }

    // Draw walls (lines)
    for (usize i = 0; i < state.walls.n; i++) {
        const struct wall *wall = &state.walls.arr[i];
        v2i start_screen = world_to_screen(v2i_to_v2(wall->a));
        v2i end_screen = world_to_screen(v2i_to_v2(wall->b));

        if ((int)i == state.editor.selected_wall) {
            SDL_SetRenderDrawColor(state.renderer, 255, 0, 0, 255); // Selected wall (Red)
            // Optionally draw thicker or draw normal for selected wall
             v2 mid = {(wall->a.x + wall->b.x) / 2.0f, (wall->a.y + wall->b.y) / 2.0f};
             v2 normal = {-(wall->b.y - wall->a.y), wall->b.x - wall->a.x}; // Points "left" assuming CCW
             normal = normalize(normal);
             v2i mid_screen = world_to_screen(mid);
             v2i normal_end_screen = world_to_screen((v2){mid.x + normal.x * 0.5f, mid.y + normal.y * 0.5f}); // Scaled normal
             SDL_RenderDrawLine(state.renderer, mid_screen.x, mid_screen.y, normal_end_screen.x, normal_end_screen.y);

        } else if (wall->portal != SECTOR_NONE) {
            SDL_SetRenderDrawColor(state.renderer, 0, 255, 0, 255); // Portal wall (Green)
        } else {
            SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255); // Normal wall (White)
        }
        SDL_RenderDrawLine(state.renderer, start_screen.x, start_screen.y, end_screen.x, end_screen.y);

        // Draw wall vertices (optional, for visual aid)
        SDL_Rect vert_rect_a = {start_screen.x - 2, start_screen.y - 2, 4, 4};
        SDL_Rect vert_rect_b = {end_screen.x - 2, end_screen.y - 2, 4, 4};
        SDL_RenderFillRect(state.renderer, &vert_rect_a);
        // SDL_RenderFillRect(state.renderer, &vert_rect_b); // Avoid double drawing shared verts
    }

    // Draw temporary points and lines during sector creation
    if (state.editor.current == TOOL_SECTOR && state.editor.is_placing && state.editor.temp_point_count > 0) {
        SDL_SetRenderDrawColor(state.renderer, 255, 255, 0, 255); // Yellow for temp lines/points
        for (int i = 0; i < state.editor.temp_point_count; i++) {
            v2i p_screen = world_to_screen(v2i_to_v2(state.editor.temp_points[i]));
            SDL_Rect rect = {p_screen.x - 3, p_screen.y - 3, 6, 6};
            SDL_RenderFillRect(state.renderer, &rect);
            if (i > 0) {
                v2i prev_p_screen = world_to_screen(v2i_to_v2(state.editor.temp_points[i-1]));
                SDL_RenderDrawLine(state.renderer, prev_p_screen.x, prev_p_screen.y, p_screen.x, p_screen.y);
            }
        }
        // Line from last temp point to current mouse position (snapped)
        v2i last_temp_p_screen = world_to_screen(v2i_to_v2(state.editor.temp_points[state.editor.temp_point_count - 1]));
        v2 mouse_world_snapped = v2i_to_v2(snap_to_grid(screen_to_world(v2_to_v2i(state.camera.last_mouse_pos))));
        v2i mouse_snapped_screen = world_to_screen(mouse_world_snapped);
        SDL_RenderDrawLine(state.renderer, last_temp_p_screen.x, last_temp_p_screen.y, mouse_snapped_screen.x, mouse_snapped_screen.y);

        // Line from current mouse to first point if enough points exist (to close polygon)
        if (state.editor.temp_point_count >= 2) {
             v2i first_temp_p_screen = world_to_screen(v2i_to_v2(state.editor.temp_points[0]));
             SDL_RenderDrawLine(state.renderer, mouse_snapped_screen.x, mouse_snapped_screen.y, first_temp_p_screen.x, first_temp_p_screen.y);
        }
    }
}


static void render_imgui_ui() {
    igBegin("Editor Controls", NULL, 0);

    // Tool selection
    if (igRadioButton_Bool("Select", state.editor.current == TOOL_SELECT)) state.editor.current = TOOL_SELECT; igSameLine(0, -1);
    if (igRadioButton_Bool("Sector Create", state.editor.current == TOOL_SECTOR)) {
        state.editor.current = TOOL_SECTOR;
        if (state.editor.is_placing) { // Cancel ongoing placement if tool switched
            state.editor.is_placing = false;
            state.editor.temp_point_count = 0;
        }
    }
    igSameLine(0, -1);
    // TOOL_WALL is effectively part of TOOL_SELECT or TOOL_PORTAL for selecting existing walls
    if (igRadioButton_Bool("Portal Tool", state.editor.current == TOOL_PORTAL)) {
        state.editor.current = TOOL_PORTAL;
        // state.editor.selected_wall = -1; // Reset wall selection for portal tool
    }
    // TOOL_MOVE not implemented yet
    // if (igRadioButton_Bool("Move", state.editor.current == TOOL_MOVE)) state.editor.current = TOOL_MOVE;
    igNewLine();

    igCheckbox("Show Grid", &state.show_grid);

    if (igButton("Save Level (Ctrl+S)", (ImVec2){0,0})) {
        if (save_sectors(LEVEL_FILE) == 0) {
            printf("Level saved successfully.\n");
            state.unsaved_changes = false;
        } else {
            printf("Failed to save level.\n");
        }
    }
    igSameLine(0, -1);
    if (igButton("Load Level (Ctrl+L)", (ImVec2){0,0})) {
        if (load_sectors(LEVEL_FILE) == 0) {
            printf("Level loaded successfully.\n");
            state.unsaved_changes = false;
            state.editor.selected_sector = SECTOR_NONE; // Deselect after load
            state.editor.selected_wall = -1;
        } else {
            printf("Failed to load level.\n");
        }
    }

    if (state.unsaved_changes) {
        igPushStyleColor_Vec4(ImGuiCol_Text, (ImVec4){1.0f, 0.4f, 0.4f, 1.0f});
        igText("Unsaved Changes!");
        igPopStyleColor(1);
    }

    igSeparator();
    igText("Camera Offset: %.1f, %.1f", state.camera.offset.x, state.camera.offset.y);
    igText("Camera Scale: %.1f", state.camera.scale);
    v2 mouse_world = screen_to_world(v2_to_v2i(state.camera.last_mouse_pos));
    v2i mouse_world_snapped = snap_to_grid(mouse_world);
    igText("Mouse World: (%.1f, %.1f) Snapped: (%d, %d)", mouse_world.x, mouse_world.y, mouse_world_snapped.x, mouse_world_snapped.y);

    igEnd(); // Editor Controls

    // Sector Properties
    if (state.editor.selected_sector != SECTOR_NONE && state.editor.selected_sector < (int)state.sectors.n) {
        struct sector* s = &state.sectors.arr[state.editor.selected_sector];
        bool is_sector_window_open = true;
        igBegin("Sector Properties", &is_sector_window_open, 0);
        if (!is_sector_window_open) {
            state.editor.selected_sector = SECTOR_NONE; // Closing window deselects
        } else {
            igText("Sector ID: %d", s->id);
            if (igInputFloat("Floor Z", &s->zfloor, 0.25f, 1.0f, "%.2f", 0)) state.unsaved_changes = true;
            if (igInputFloat("Ceiling Z", &s->zceil, 0.25f, 1.0f, "%.2f", 0)) state.unsaved_changes = true;

            if (s->zfloor >= s->zceil - 0.01f) s->zfloor = s->zceil - 0.01f; // Min separation
            if (s->zceil <= s->zfloor + 0.01f) s->zceil = s->zfloor + 0.01f;

            igText("Walls: %zu (Index from: %zu)", s->nwalls, s->firstwall);
        }
        igEnd();
    }

    // Wall Properties
    if (state.editor.selected_wall != -1 && state.editor.selected_wall < (int)state.walls.n) {
        struct wall* w = &state.walls.arr[state.editor.selected_wall];
        bool is_wall_window_open = true;
        igBegin("Wall Properties", &is_wall_window_open, 0);
        if (!is_wall_window_open) {
            state.editor.selected_wall = -1; // Closing window deselects
        } else {
            igText("Wall Index: %d", state.editor.selected_wall);
            igText("A: (%d, %d), B: (%d, %d)", w->a.x, w->a.y, w->b.x, w->b.y);
            if (w->portal == SECTOR_NONE) {
                igText("Portal: None");
            } else {
                igText("Portal to Sector ID: %d", w->portal);
                if (igButton("Remove Portal", (ImVec2){0,0})) {
                    int owner_sector_id = SECTOR_NONE;
                    for (int i = 1; i < state.sectors.n; i++) {
                        const struct sector *sec = &state.sectors.arr[i];
                        if (state.editor.selected_wall >= (int)sec->firstwall &&
                            state.editor.selected_wall < (int)(sec->firstwall + sec->nwalls)) {
                            owner_sector_id = sec->id;
                            break;
                        }
                    }

                    int portal_target_sector_id = w->portal;
                    w->portal = SECTOR_NONE; // Clear this wall's portal

                    // Find and clear the other side of the portal
                    if (portal_target_sector_id != SECTOR_NONE && portal_target_sector_id < (int)state.sectors.n) {
                        const struct sector* other_s = &state.sectors.arr[portal_target_sector_id];
                        for (usize i = 0; i < other_s->nwalls; i++) {
                            struct wall* other_w = &state.walls.arr[other_s->firstwall + i];
                            if (other_w->portal == owner_sector_id &&
                                other_w->a.x == w->b.x && other_w->a.y == w->b.y &&
                                other_w->b.x == w->a.x && other_w->b.y == w->a.y) {
                                other_w->portal = SECTOR_NONE;
                                break;
                            }
                        }
                    }
                    state.unsaved_changes = true;
                    printf("Portal removed from wall %d.\n", state.editor.selected_wall);
                }
            }
        }
        igEnd();
    }

    // Sector Creation UI Feedback (simple)
    if (state.editor.current == TOOL_SECTOR && state.editor.is_placing) {
        // Using a less intrusive way to show status, could be a dedicated status bar area too
        igSetNextWindowPos((ImVec2){(float)WINDOW_WIDTH / 2 - 150, 40.0f}, ImGuiCond_Always, (ImVec2){0.5f, 0.0f});
        igSetNextWindowBgAlpha(0.75f);
        if (igBegin("SectorCreationStatus", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav)) {
            igText("Placing points for new sector (%d/32). Click to add.", state.editor.temp_point_count);
            igText("Press ENTER to complete, ESC to cancel.");
            if (state.editor.temp_point_count > 2) {
                igText("Click near first point to close and complete.");
            }
        }
        igEnd();
    }
}


static void render() {
    // Start the Dear ImGui frame
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    igNewFrame();

    // 1. Render ImGui UI (gets user input for this frame)
    render_imgui_ui();

    // 2. Clear screen for game world rendering
    SDL_SetRenderDrawColor(state.renderer, 20, 20, 20, 255);
    SDL_RenderClear(state.renderer);

    // 3. Render game world (grid, walls, sectors, etc.)
    draw_grid();
    draw_walls_and_sectors(); // Combined drawing function

    // 4. Render ImGui draw data on top
    igRender();
    ImGui_ImplSDLRenderer2_RenderDrawData(igGetDrawData(), state.renderer);

    // 5. Present the final frame
    SDL_RenderPresent(state.renderer);
}


static void process_input() {
    SDL_Event event;
    ImGuiIO* io = igGetIO(); // Get ImGui IO state

    // SDL_GetMouseState updates state.camera.last_mouse_pos for UI display
    // Call it once per frame before event polling if needed for continuous display,
    // or within mouse motion event if only for interaction.
    int mx, my;
    SDL_GetMouseState(&mx, &my);
    state.camera.last_mouse_pos = (v2){(f32)mx, (f32)my};


    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event); // Pass all events to ImGui

        if (event.type == SDL_QUIT) {
            state.quit = true;
        }

        // Keyboard events not captured by ImGui
        if (event.type == SDL_KEYDOWN && !io->WantCaptureKeyboard) {
            const bool ctrl_pressed = (SDL_GetModState() & KMOD_CTRL) != 0;
            switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    if (state.editor.is_placing) {
                        state.editor.is_placing = false;
                        state.editor.temp_point_count = 0;
                        printf("Sector creation cancelled.\n");
                    } else {
                        state.editor.selected_sector = SECTOR_NONE;
                        state.editor.selected_wall = -1;
                    }
                    break;
                case SDLK_s:
                    if (ctrl_pressed) {
                        if (save_sectors(LEVEL_FILE) == 0) {
                            printf("Level saved successfully.\n");
                            state.unsaved_changes = false;
                        } else {
                            printf("Failed to save level.\n");
                        }
                    }
                    break;
                case SDLK_l:
                    if (ctrl_pressed) {
                        if (load_sectors(LEVEL_FILE) == 0) {
                            printf("Level loaded successfully.\n");
                            state.unsaved_changes = false;
                            state.editor.selected_sector = SECTOR_NONE;
                            state.editor.selected_wall = -1;
                        } else {
                            printf("Failed to load level.\n");
                        }
                    }
                    break;
                case SDLK_RETURN: // Enter key
                case SDLK_KP_ENTER:
                    if (state.editor.current == TOOL_SECTOR && state.editor.is_placing && state.editor.temp_point_count >= 3) {
                        create_sector(state.editor.temp_points, state.editor.temp_point_count);
                        state.editor.is_placing = false;
                        state.editor.temp_point_count = 0;
                    }
                    break;
                // Camera panning with arrow keys
                case SDLK_LEFT:  state.camera.offset.x -= 32.0f / state.camera.scale; break;
                case SDLK_RIGHT: state.camera.offset.x += 32.0f / state.camera.scale; break;
                case SDLK_UP:    state.camera.offset.y -= 32.0f / state.camera.scale; break;
                case SDLK_DOWN:  state.camera.offset.y += 32.0f / state.camera.scale; break;
            }
        }

        // Mouse events not captured by ImGui
        if (!io->WantCaptureMouse) {
            v2 mouse_world = screen_to_world(v2_to_v2i(state.camera.last_mouse_pos)); // current mouse in world
            v2i snapped_world = snap_to_grid(mouse_world); // current mouse snapped in world

            switch (event.type) {
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        switch (state.editor.current) {
                            case TOOL_SECTOR:
                                if (!state.editor.is_placing) {
                                    state.editor.is_placing = true;
                                    state.editor.temp_point_count = 0;
                                    printf("Started placing new sector. Click to add points.\n");
                                }
                                if (state.editor.temp_point_count < 32) {
                                    // Check for closing loop by clicking near first point
                                    if (state.editor.temp_point_count > 2) {
                                        v2i first_p = state.editor.temp_points[0];
                                        float dx = (float)first_p.x - snapped_world.x;
                                        float dy = (float)first_p.y - snapped_world.y;
                                        // Using a small pixel-independent distance threshold (e.g., 0.5 world units)
                                        if (sqrtf(dx*dx + dy*dy) < 0.5f * GRID_SIZE * state.camera.scale / INITIAL_SCALE * 5.0f) { // Heuristic threshold
                                            create_sector(state.editor.temp_points, state.editor.temp_point_count);
                                            state.editor.is_placing = false;
                                            state.editor.temp_point_count = 0;
                                            break; // Exit TOOL_SECTOR switch case
                                        }
                                    }
                                    // Add new point
                                    state.editor.temp_points[state.editor.temp_point_count++] = snapped_world;
                                    printf("Added point %d at (%d, %d)\n", state.editor.temp_point_count, snapped_world.x, snapped_world.y);
                                } else {
                                    printf("Max points for sector reached.\n");
                                }
                                break;
                            case TOOL_PORTAL:
                                if (state.editor.selected_wall == -1) { // First click: select wall
                                    float dist;
                                    int wall_idx = find_closest_wall(mouse_world, &dist);
                                    // Threshold for selection (e.g., 10 pixels scaled)
                                    if (wall_idx != -1 && dist < (10.0f / state.camera.scale)) {
                                        state.editor.selected_wall = wall_idx;
                                        printf("Wall %d selected for portal. Click target sector.\n", wall_idx);
                                    } else {
                                        printf("No wall close enough to select for portal.\n");
                                    }
                                } else { // Second click: select target sector
                                    int sector_id = find_sector_at_point(mouse_world);
                                    if (sector_id != SECTOR_NONE) {
                                        create_portal(state.editor.selected_wall, sector_id);
                                        state.editor.selected_wall = -1; // Reset after attempt
                                    } else {
                                        printf("No sector selected for portal target. Click on a sector.\n");
                                        // Optionally, deselect wall if clicking empty space:
                                        // state.editor.selected_wall = -1;
                                    }
                                }
                                break;
                            case TOOL_SELECT:
                            {
                                int sector_id = find_sector_at_point(mouse_world);
                                if (sector_id != SECTOR_NONE) {
                                    state.editor.selected_sector = sector_id;
                                    state.editor.selected_wall = -1; // Deselect wall if sector is selected
                                    printf("Sector %d selected.\n", sector_id);
                                } else {
                                    float dist;
                                    int wall_idx = find_closest_wall(mouse_world, &dist);
                                    if (wall_idx != -1 && dist < (10.0f / state.camera.scale)) {
                                        state.editor.selected_wall = wall_idx;
                                        state.editor.selected_sector = SECTOR_NONE; // Deselect sector
                                        printf("Wall %d selected.\n", wall_idx);
                                    } else {
                                        // Clicked empty space, deselect all
                                        state.editor.selected_sector = SECTOR_NONE;
                                        state.editor.selected_wall = -1;
                                        printf("Selection cleared.\n");
                                    }
                                }
                            }
                            break;
                            case TOOL_NONE:
                            case TOOL_WALL: // TOOL_WALL is for selection in this minimal impl.
                            case TOOL_MOVE: // Not implemented
                                break;
                        }
                    } else if (event.button.button == SDL_BUTTON_MIDDLE) {
                        state.camera.panning = true;
                        state.camera.pan_start = state.camera.last_mouse_pos; // current mouse in screen coords
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (event.button.button == SDL_BUTTON_MIDDLE) {
                        state.camera.panning = false;
                    }
                    break;
                case SDL_MOUSEMOTION:
                    // state.camera.last_mouse_pos is already updated at start of loop
                    if (state.camera.panning) {
                        v2 current_mouse_screen = state.camera.last_mouse_pos;
                        float dx_screen = current_mouse_screen.x - state.camera.pan_start.x;
                        float dy_screen = current_mouse_screen.y - state.camera.pan_start.y;

                        state.camera.offset.x -= dx_screen / state.camera.scale;
                        state.camera.offset.y -= dy_screen / state.camera.scale;

                        state.camera.pan_start = current_mouse_screen; // Update pan_start for next delta
                    }
                    break;
                case SDL_MOUSEWHEEL:
                {
                    const float zoom_factor = 1.1f;
                    const float old_scale = state.camera.scale;
                    v2 mouse_pre_zoom_world = screen_to_world(v2_to_v2i(state.camera.last_mouse_pos));

                    if (event.wheel.y > 0) { // Scroll up, zoom in
                        state.camera.scale = min(state.camera.scale * zoom_factor, MAX_SCALE);
                    } else if (event.wheel.y < 0) { // Scroll down, zoom out
                        state.camera.scale = max(state.camera.scale / zoom_factor, MIN_SCALE);
                    }

                    if (old_scale != state.camera.scale) {
                        // Adjust offset to keep the point under the mouse stationary
                        v2 mouse_post_zoom_world = screen_to_world(v2_to_v2i(state.camera.last_mouse_pos));
                        state.camera.offset.x += (mouse_pre_zoom_world.x - mouse_post_zoom_world.x);
                        state.camera.offset.y += (mouse_pre_zoom_world.y - mouse_post_zoom_world.y);
                    }
                }
                break;
            }
        }
    }
}


// save sectors from state -> file
static int save_sectors(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) {
        perror("Failed to open file for writing");
        return -1;
    }

    int retval = 0;

    if (fprintf(f, "[SECTOR]\n") < 0) { retval = -2; goto done; }
    for (usize i = 1; i < state.sectors.n; i++) { // Start from 1
        const struct sector *s = &state.sectors.arr[i];
        if (fprintf(f, "%d %zu %zu %.2f %.2f\n", s->id, s->firstwall, s->nwalls, s->zfloor, s->zceil) < 0) {
            retval = -3; goto done;
        }
    }

    if (fprintf(f, "[WALL]\n") < 0) { retval = -4; goto done; }
    for (usize i = 0; i < state.walls.n; i++) {
        const struct wall *w = &state.walls.arr[i];
        if (fprintf(f, "%d %d %d %d %d\n", w->a.x, w->a.y, w->b.x, w->b.y, w->portal) < 0) {
            retval = -5; goto done;
        }
    }

    if (ferror(f)) retval = -128;

done:
    if (fclose(f) == EOF && retval == 0) retval = -129; // Error closing file
    return retval;
}

static int load_sectors(const char *path) {
    state.sectors.n = 1; // Reset, keeping SECTOR_NONE placeholder at index 0
    state.walls.n = 0;
    state.editor.selected_sector = SECTOR_NONE;
    state.editor.selected_wall = -1;
    state.editor.is_placing = false;
    state.editor.temp_point_count = 0;


    FILE *file = fopen(path, "r");
    if (!file) {
        // This is not necessarily an error if file doesn't exist on first run
        // perror("Failed to open file for reading");
        return -1;
    }

    int retval = 0;
    enum { SCAN_NONE, SCAN_SECTOR, SCAN_WALL } scan_state = SCAN_NONE;
    char line[256];

    while (fgets(line, sizeof(line), file)) {
        char *p = line;
        while (isspace((unsigned char)*p)) p++; // Skip leading whitespace
        if (*p == '\0' || *p == '#') continue; // Skip empty or comment lines

        if (*p == '[') {
            char section_name[64];
            if (sscanf(p, "[%63[^]]]", section_name) == 1) {
                if (strcmp(section_name, "SECTOR") == 0) scan_state = SCAN_SECTOR;
                else if (strcmp(section_name, "WALL") == 0) scan_state = SCAN_WALL;
                else { scan_state = SCAN_NONE; printf("Warning: Unknown section %s\n", section_name); }
            } else {
                 scan_state = SCAN_NONE; printf("Warning: Malformed section line: %s\n", line);
            }
        } else {
            switch (scan_state) {
                case SCAN_SECTOR:
                    if (state.sectors.n >= SECTOR_MAX) { retval = -10; goto done; } // Too many sectors
                    struct sector *s = &state.sectors.arr[state.sectors.n];
                    if (sscanf(p, "%d %zu %zu %f %f", &s->id, &s->firstwall, &s->nwalls, &s->zfloor, &s->zceil) == 5) {
                        // Basic validation: ensure ID matches expected next ID if loading sequentially
                        if (s->id != (int)state.sectors.n) {
                            printf("Warning: Sector ID %d mismatch, expected %zu. Adjusting.\n", s->id, state.sectors.n);
                            s->id = state.sectors.n;
                        }
                        state.sectors.n++;
                    } else { retval = -5; goto done; } // Invalid sector format
                    break;
                case SCAN_WALL:
                    if (state.walls.n >= sizeof(state.walls.arr)/sizeof(state.walls.arr[0])) { retval = -11; goto done; } // Too many walls
                    struct wall *w = &state.walls.arr[state.walls.n];
                    if (sscanf(p, "%d %d %d %d %d", &w->a.x, &w->a.y, &w->b.x, &w->b.y, &w->portal) == 5) {
                        state.walls.n++;
                    } else { retval = -4; goto done; } // Invalid wall format
                    break;
                case SCAN_NONE: // Data outside a known section
                    printf("Warning: Data found outside section: %s\n", line);
                    break;
            }
        }
    }

    if (ferror(file)) retval = -128;

done:
    if (file) fclose(file); // fclose can be called on NULL but good practice to check
    if (retval != 0) { // If error, reset to empty state
        state.sectors.n = 1;
        state.walls.n = 0;
    }
    return retval;
}


static void init_editor() {
    ASSERT(SDL_Init(SDL_INIT_VIDEO) == 0, "SDL_Init Error: %s\n", SDL_GetError());

    state.window = SDL_CreateWindow("Minimal Map Editor (ImGui)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    ASSERT(state.window, "SDL_CreateWindow Error: %s\n", SDL_GetError());

    state.renderer = SDL_CreateRenderer(state.window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    ASSERT(state.renderer, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
    SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_BLEND);


    // Setup Dear ImGui context
    igCreateContext(NULL);
    ImGuiIO* io = igGetIO();
    (void)io; // Unused variable warning if not using flags
    //io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    //io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // Enable Docking
    //io->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows

    // Setup Dear ImGui style
    igStyleColorsDark(NULL);

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForSDLRenderer(state.window, state.renderer);
    ImGui_ImplSDLRenderer2_Init(state.renderer);

    // Initialize editor state
    state.quit = false;
    state.show_grid = true;
    state.unsaved_changes = false;

    state.sectors.n = 1; // Sector 0 is SECTOR_NONE, actual sectors start at index 1
    state.walls.n = 0;

    state.camera.offset = (v2){0, 0};
    state.camera.scale = INITIAL_SCALE;
    state.camera.panning = false;
    state.camera.last_mouse_pos = (v2){0,0};

    state.editor.current = TOOL_SELECT;
    state.editor.selected_sector = SECTOR_NONE;
    state.editor.selected_wall = -1;
    state.editor.is_placing = false;
    state.editor.temp_point_count = 0;

    if (load_sectors(LEVEL_FILE) != 0) {
        printf("No existing level found or error loading. Starting with an empty map.\n");
    } else {
        printf("Level loaded successfully from %s.\n", LEVEL_FILE);
    }
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv; // Unused

    init_editor();

    while (!state.quit) {
        process_input();
        render();
        // SDL_Delay(16); // VSync should handle frame rate, but can be uncommented if issues
    }

    if (state.unsaved_changes) {
        // For a minimal version, a console prompt is acceptable.
        // A more robust solution would be an ImGui modal dialog.
        printf("You have unsaved changes. Save before quitting? (y/n): ");
        char response = getchar(); // Simple blocking char read
        if (response == 'y' || response == 'Y') {
            if (save_sectors(LEVEL_FILE) == 0) {
                printf("Level saved successfully.\n");
            } else {
                printf("Failed to save level.\n");
            }
        }
    }

    // Cleanup ImGui
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    igDestroyContext(NULL);

    // Cleanup SDL
    SDL_DestroyRenderer(state.renderer);
    SDL_DestroyWindow(state.window);
    SDL_Quit();
    
    return 0;
}