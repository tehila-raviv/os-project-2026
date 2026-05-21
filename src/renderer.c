#define _POSIX_C_SOURCE 200809L  /* expose kill(), pid_t, etc. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/types.h>   /* pid_t */
#include "raylib.h"
#include "renderer.h"
#include "graph.h"
#include "dijkstra.h"

/* ── Color palette ─────────────────────────────────────────── */
#define COL_BG          ((Color){ 15,  17,  26, 255})
#define COL_GRID        ((Color){ 30,  35,  55, 255})
#define COL_NODE        ((Color){ 20,  30,  60, 255})
#define COL_NODE_BORDER ((Color){ 80, 140, 255, 255})
#define COL_EDGE        ((Color){100, 120, 180, 100})
#define COL_WEIGHT      ((Color){255, 200,  80, 255})
#define COL_LABEL       ((Color){220, 230, 255, 255})
#define COL_NAME        ((Color){160, 200, 255, 200})
#define COL_TITLE       ((Color){ 80, 140, 255, 255})
#define COL_BTN_PLAY    ((Color){ 40, 180,  80, 255})
#define COL_BTN_STOP    ((Color){200,  60,  60, 255})

/* Distinct colors for up to MAX_TRAVELERS travelers */
static const Color TRAVELER_COLORS[MAX_TRAVELERS] = {
    {255,  80,  80, 255},   /* red        */
    { 80, 200, 255, 255},   /* cyan       */
    {255, 200,  50, 255},   /* yellow     */
    { 80, 255, 130, 255},   /* green      */
    {200,  80, 255, 255},   /* purple     */
    {255, 160,  40, 255},   /* orange     */
    { 40, 180, 255, 255},   /* sky blue   */
    {255,  80, 180, 255},   /* pink       */
    {160, 255,  80, 255},   /* lime       */
    { 80,  80, 255, 255},   /* indigo     */
    {255, 240, 180, 255},   /* cream      */
    {  0, 220, 180, 255},   /* teal       */
    {220, 120, 120, 255},   /* salmon     */
    {120, 220, 120, 255},   /* mint       */
    {220, 220,  80, 255},   /* gold       */
    {160, 120, 255, 255},   /* lavender   */
};

/* ── Layout ──────────────────────────────────────────────────── */

void renderer_compute_positions(int num_nodes, Vec2 *positions) {
    float cx = WINDOW_WIDTH  / 2.0f;
    float cy = WINDOW_HEIGHT / 2.0f;
    float r  = (WINDOW_HEIGHT < WINDOW_WIDTH ? WINDOW_HEIGHT : WINDOW_WIDTH)
               * 0.36f;

    if (num_nodes == 1) {
        positions[0].x = cx;
        positions[0].y = cy;
        return;
    }

    float step  = (2.0f * 3.14159265f) / (float)num_nodes;
    float start = -3.14159265f / 2.0f;   /* start at the top */

    for (int i = 0; i < num_nodes; i++) {
        float angle    = start + (float)i * step;
        positions[i].x = cx + r * cosf(angle);
        positions[i].y = cy + r * sinf(angle);
    }
}

/* ── Path query helpers ──────────────────────────────────────── */

/* Returns true if the directed edge u->v is on the path of traveler t */
static bool edge_on_traveler_path(int u, int v, const DijkstraResult *res) {
    if (!res || !res->found || res->path_len < 2) return false;
    for (int i = 0; i < res->path_len - 1; i++) {
        if (res->path[i] == u && res->path[i + 1] == v) return true;
    }
    return false;
}

/* ── Drawing helpers ─────────────────────────────────────────── */

static void draw_grid(void) {
    int spacing = 40;
    for (int x = 0; x < WINDOW_WIDTH; x += spacing)
        DrawLine(x, 0, x, WINDOW_HEIGHT, COL_GRID);
    for (int y = 0; y < WINDOW_HEIGHT; y += spacing)
        DrawLine(0, y, WINDOW_WIDTH, y, COL_GRID);
}

static void draw_arrow(float x1, float y1, float x2, float y2,
                       Color col, float thickness) {
    float dx  = x2 - x1;
    float dy  = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1.0f) return;

    float ux = dx / len;
    float uy = dy / len;

    float margin = (float)NODE_RADIUS + 8.0f;
    float sx = x1 + ux * margin;
    float sy = y1 + uy * margin;
    float ex = x2 - ux * margin;
    float ey = y2 - uy * margin;

    DrawLineEx((Vector2){sx, sy}, (Vector2){ex, ey}, thickness, col);

    float head  = 14.0f + thickness;
    float angle = 0.45f;
    float dir   = atan2f(ey - sy, ex - sx);
    Vector2 v1  = {ex, ey};
    Vector2 v2  = {ex - head * cosf(dir - angle), ey - head * sinf(dir - angle)};
    Vector2 v3  = {ex - head * cosf(dir + angle), ey - head * sinf(dir + angle)};
    DrawTriangle(v1, v2, v3, col);
}

static void draw_weight_label(float x1, float y1, float x2, float y2,
                              int weight) {
    float mx  = (x1 + x2) / 2.0f;
    float my  = (y1 + y2) / 2.0f;
    float dx  = x2 - x1;
    float dy  = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    if (len > 0.1f) {
        mx += (-dy / len) * 14.0f;
        my += ( dx / len) * 14.0f;
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", weight);
    int tw = MeasureText(buf, 14);
    DrawRectangle((int)(mx - tw / 2 - 3), (int)(my - 9),
                  tw + 6, 18, (Color){10, 12, 22, 200});
    DrawText(buf, (int)(mx - tw / 2), (int)(my - 7), 14, COL_WEIGHT);
}

/* ── Graph draw ──────────────────────────────────────────────── */

void renderer_draw_graph(const Graph          *g,
                         const Vec2           *pos,
                         const char          **station_names,
                         const DijkstraResult *results,
                         const TrainAnim      *anim_list,
                         int                   num_travelers)
{
    ClearBackground(COL_BG);
    draw_grid();

    DrawText("TrainOS  |  Railway Network", 25, 20, 22, COL_TITLE);

    /* ── Edges ── */
    for (int u = 0; u < g->num_vertices; u++) {
        EdgeNode *e = g->lists[u].head;
        while (e) {
            int v = e->dest;

            /* Check if this edge is on any traveler's path */
            Color col   = COL_EDGE;
            float thick = 1.5f;

            for (int t = 0; t < num_travelers; t++) {
                if (results && edge_on_traveler_path(u, v, &results[t])) {
                    /* Use the traveler's color; thicker line */
                    col   = TRAVELER_COLORS[anim_list[t].color_index % MAX_TRAVELERS];
                    thick = 3.0f;
                    break;   /* first match wins (edges are rarely shared) */
                }
            }

            draw_arrow(pos[u].x, pos[u].y, pos[v].x, pos[v].y, col, thick);
            draw_weight_label(pos[u].x, pos[u].y, pos[v].x, pos[v].y, e->weight);
            e = e->next;
        }
    }

    /* ── Nodes ── */
    for (int i = 0; i < g->num_vertices; i++) {
        Color border = COL_NODE_BORDER;

        /* If this node is a source or destination of any traveler, tint it */
        for (int t = 0; t < num_travelers; t++) {
            if (!anim_list) break;
            if (i == anim_list[t].src_id || i == anim_list[t].dst_id) {
                border = TRAVELER_COLORS[anim_list[t].color_index % MAX_TRAVELERS];
                break;
            }
        }

        DrawCircle((int)pos[i].x, (int)pos[i].y,
                   NODE_RADIUS + 5, Fade(border, 0.3f));
        DrawCircle((int)pos[i].x, (int)pos[i].y, NODE_RADIUS, COL_NODE);
        DrawCircleLines((int)pos[i].x, (int)pos[i].y, NODE_RADIUS, border);

        char id_buf[16];
        snprintf(id_buf, sizeof(id_buf), "%d", i);
        int iw = MeasureText(id_buf, 18);
        DrawText(id_buf,
                 (int)(pos[i].x - iw / 2),
                 (int)(pos[i].y - 9),
                 18, COL_LABEL);

        if (station_names && station_names[i]) {
            int nw = MeasureText(station_names[i], 12);
            DrawText(station_names[i],
                     (int)(pos[i].x - nw / 2),
                     (int)(pos[i].y + NODE_RADIUS + 8),
                     12, COL_NAME);
        }
    }

    /* ── Legend: one row per traveler ── */
    int lx = WINDOW_WIDTH - 200;
    int ly = 80;
    DrawRectangleLines(lx - 10, ly - 10,
                       190, 24 * num_travelers + 20, COL_GRID);

    for (int t = 0; t < num_travelers; t++) {
        if (!anim_list) break;
        Color tc = TRAVELER_COLORS[anim_list[t].color_index % MAX_TRAVELERS];
        DrawCircle(lx, ly + t * 24, 7, tc);

        char legend[48];
        const char *status = "";
        if (anim_list[t].phase == PHASE_ARRIVED) status = " [arrived]";
        snprintf(legend, sizeof(legend), "Train %d: %d->%d%s",
                 t + 1, anim_list[t].src_id, anim_list[t].dst_id, status);
        DrawText(legend, lx + 16, ly + t * 24 - 7, 12, WHITE);
    }
}

/* ── Play/Stop button ────────────────────────────────────────── */

#define BTN_X (WINDOW_WIDTH  - 140)
#define BTN_Y 10
#define BTN_W 120
#define BTN_H 40

static void draw_play_button(int all_paused) {
    Color       bg    = all_paused ? COL_BTN_PLAY : COL_BTN_STOP;
    const char *label = all_paused ? "  PLAY"     : "  STOP";
    DrawRectangleRounded((Rectangle){BTN_X, BTN_Y, BTN_W, BTN_H},
                         0.3f, 8, bg);
    DrawRectangleRoundedLines((Rectangle){BTN_X, BTN_Y, BTN_W, BTN_H},
                               0.3f, 8, 1.5f, WHITE);
    DrawText(label, BTN_X + 10, BTN_Y + 11, 18, WHITE);
}

static int button_clicked(void) {
    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) return 0;
    Vector2 mp = GetMousePosition();
    return mp.x >= BTN_X && mp.x <= BTN_X + BTN_W &&
           mp.y >= BTN_Y && mp.y <= BTN_Y + BTN_H;
}

/* ── Train animation ─────────────────────────────────────────── */

static int edge_weight_lookup(const Graph *g, int u, int v) {
    EdgeNode *e = g->lists[u].head;
    while (e) {
        if (e->dest == v) return e->weight;
        e = e->next;
    }
    return 1;   /* fallback: should never happen on a valid path */
}

static void anim_init(TrainAnim *a, const DijkstraResult *res,
                      const Vec2 *pos, int src_id, int dst_id,
                      int color_index, pid_t child_pid)
{
    memset(a, 0, sizeof(*a));
    a->src_id      = src_id;
    a->dst_id      = dst_id;
    a->color_index = color_index;
    a->child_pid   = child_pid;

    if (!res || !res->found || res->path_len < 1) {
        /* No valid path: train is considered arrived immediately */
        a->train_x = pos[src_id].x;
        a->train_y = pos[src_id].y;
        a->phase   = PHASE_ARRIVED;
        return;
    }

    if (res->path_len == 1) {
        /* Source == destination */
        a->path     = res->path;
        a->path_len = 1;
        a->train_x  = pos[res->path[0]].x;
        a->train_y  = pos[res->path[0]].y;
        a->phase    = PHASE_ARRIVED;
        return;
    }

    a->path       = res->path;
    a->path_len   = res->path_len;
    a->seg        = 0;
    a->seg_weight = 0;
    a->phase      = PHASE_TRAVELLING;
    a->paused     = 1;   /* user must press PLAY to start */
    a->timer_ms   = 0.0;
    a->train_x    = pos[res->path[0]].x;
    a->train_y    = pos[res->path[0]].y;
}

/* Advance one traveler's animation by delta_ms.
   Returns true if the traveler just reached PHASE_ARRIVED this tick
   (so the caller can send SIGTERM to the child). */
static bool anim_update(TrainAnim *a, const Graph *g,
                        const Vec2 *pos, double delta_ms)
{
    if (a->paused || a->phase == PHASE_ARRIVED) return false;

    a->timer_ms += delta_ms;

    if (a->phase == PHASE_IDLE) {
        if (a->timer_ms >= MS_STATION_WAIT) {
            a->timer_ms   = 0.0;
            a->seg++;
            if (a->seg + 1 >= a->path_len) {
                a->phase = PHASE_ARRIVED;
                return true;
            }
            a->phase      = PHASE_TRAVELLING;
            a->seg_weight = edge_weight_lookup(g, a->path[a->seg],
                                                  a->path[a->seg + 1]);
        }
        return false;
    }

    /* PHASE_TRAVELLING: lazy-load segment weight on first tick */
    if (a->seg_weight == 0)
        a->seg_weight = edge_weight_lookup(g, a->path[a->seg],
                                              a->path[a->seg + 1]);

    int u = a->path[a->seg];
    int v = a->path[a->seg + 1];

    double total_ms = (double)a->seg_weight * MS_PER_JUMP;
    float  t        = (float)(a->timer_ms / total_ms);
    if (t > 1.0f) t = 1.0f;

    a->train_x = pos[u].x + t * (pos[v].x - pos[u].x);
    a->train_y = pos[u].y + t * (pos[v].y - pos[u].y);

    if (a->timer_ms >= total_ms) {
        a->train_x    = pos[v].x;
        a->train_y    = pos[v].y;
        a->timer_ms   = 0.0;
        a->seg_weight = 0;

        if (a->seg + 2 >= a->path_len) {
            a->phase = PHASE_ARRIVED;
            return true;
        }
        a->phase = PHASE_IDLE;
    }
    return false;
}

/* Compute per-slot offset so multiple trains at the same position
   orbit the node center instead of stacking on top of each other.
   With only one train present, offset is (0,0) so it stays centered. */
static void slot_offset(int slot, int total, float *ox, float *oy) {
    if (total <= 1) { *ox = 0.0f; *oy = 0.0f; return; }
    float r     = (NODE_RADIUS - 12) * 0.7f;
    float angle = (2.0f * 3.14159265f / (float)total) * (float)slot
                  - 3.14159265f / 2.0f;
    *ox = r * cosf(angle);
    *oy = r * sinf(angle);
}

static void draw_train(const TrainAnim *a, int slot, int total_at_pos) {
    Color tc = TRAVELER_COLORS[a->color_index % MAX_TRAVELERS];

    /* Arrived trains: draw faded so they're visible but clearly done */
    if (a->phase == PHASE_ARRIVED) {
        tc.a = 120;   /* semi-transparent */
        Color glow = tc; glow.a = 30;
        float cx = a->train_x, cy = a->train_y;
        DrawCircle((int)cx, (int)cy, NODE_RADIUS - 4,  glow);
        DrawCircle((int)cx, (int)cy, NODE_RADIUS - 10, tc);
        DrawCircleLines((int)cx, (int)cy, NODE_RADIUS - 10,
                        (Color){200, 200, 200, 120});
        static const char *LABELS[MAX_TRAVELERS] = {
            "T1","T2","T3","T4","T5","T6","T7","T8",
            "T9","T10","T11","T12","T13","T14","T15","T16"
        };
        const char *lbl = LABELS[a->color_index % MAX_TRAVELERS];
        int lw = MeasureText(lbl, 13);
        DrawText(lbl, (int)(cx - lw/2), (int)(cy - 7), 13,
                 (Color){200, 200, 200, 120});
        return;
    }

    Color glow = tc; glow.a = 60;

    float ox, oy;
    slot_offset(slot, total_at_pos, &ox, &oy);
    float cx = a->train_x + ox;
    float cy = a->train_y + oy;

    DrawCircle((int)cx, (int)cy, NODE_RADIUS - 4,  glow);
    DrawCircle((int)cx, (int)cy, NODE_RADIUS - 10, tc);
    DrawCircleLines((int)cx, (int)cy, NODE_RADIUS - 10, WHITE);

    static const char *TRAIN_LABELS[MAX_TRAVELERS] = {
        "T1",  "T2",  "T3",  "T4",  "T5",  "T6",  "T7",  "T8",
        "T9",  "T10", "T11", "T12", "T13", "T14", "T15", "T16"
    };
    const char *lbl = TRAIN_LABELS[a->color_index % MAX_TRAVELERS];
    int lw = MeasureText(lbl, 13);
    DrawText(lbl, (int)(cx - lw / 2), (int)(cy - 7), 13, WHITE);
}

/* Small "arrived" overlay shown for travelers that finished */
static void draw_arrived_overlay(const TrainAnim *a, int slot, int total) {
    (void)total;
    Color tc = TRAVELER_COLORS[a->color_index % MAX_TRAVELERS];

    int bw = 360, bh = 36;
    int bx = 20;
    int by = WINDOW_HEIGHT - 45 - slot * 42;

    DrawRectangleRounded((Rectangle){(float)bx, (float)by,
                                     (float)bw, (float)bh},
                         0.25f, 8, (Color){10, 20, 40, 220});
    DrawRectangleRoundedLines((Rectangle){(float)bx, (float)by,
                                          (float)bw, (float)bh},
                               0.25f, 8, 1.5f, tc);

    char msg[80];
    snprintf(msg, sizeof(msg),
             "Train %d arrived: %d -> %d",
             a->color_index + 1, a->src_id, a->dst_id);
    int tw = MeasureText(msg, 16);
    DrawText(msg, bx + bw / 2 - tw / 2, by + 10, 16, tc);
}

/* ── Main window loop ────────────────────────────────────────── */

void renderer_run(const Graph          *g,
                  const char          **station_names,
                  const DijkstraResult *results,
                  int                   num_travelers,
                  pid_t                *child_pids)
{
    Vec2 positions[MAX_NODES];
    renderer_compute_positions(g->num_vertices, positions);

    /* Initialise per-traveler animation state */
    TrainAnim anims[MAX_TRAVELERS];
    for (int t = 0; t < num_travelers; t++) {
        anim_init(&anims[t], &results[t], positions,
                  /* src/dst are stored inside DijkstraResult path endpoints */
                  (results[t].found && results[t].path_len > 0)
                      ? results[t].path[0] : 0,
                  (results[t].found && results[t].path_len > 0)
                      ? results[t].path[results[t].path_len - 1] : 0,
                  t,
                  child_pids ? child_pids[t] : -1);
    }

    /* Pre-load first segment weights */
    for (int t = 0; t < num_travelers; t++) {
        if (anims[t].phase == PHASE_TRAVELLING && anims[t].path_len >= 2)
            anims[t].seg_weight = edge_weight_lookup(
                    g, anims[t].path[0], anims[t].path[1]);
    }

    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
               "TrainOS - Railway Traffic Simulation");
    SetTargetFPS(60);

    int global_paused = 1;   /* all trains start paused */

    while (!WindowShouldClose()) {
        double delta_ms = GetFrameTime() * 1000.0;

        /* Toggle play/stop for all non-arrived travelers */
        if (button_clicked()) {
            global_paused = !global_paused;
            for (int t = 0; t < num_travelers; t++) {
                if (anims[t].phase != PHASE_ARRIVED)
                    anims[t].paused = global_paused;
            }
        }

        /* Advance each traveler; send SIGTERM when it arrives */
        for (int t = 0; t < num_travelers; t++) {
            bool just_arrived = anim_update(&anims[t], g, positions, delta_ms);
            if (just_arrived && anims[t].child_pid > 0) {
                kill(anims[t].child_pid, SIGTERM);
            }
        }

        BeginDrawing();

        renderer_draw_graph(g, positions, station_names,
                            results, anims, num_travelers);

        /* Decide button label from whether all active trains are paused */
        int any_active = 0;
        for (int t = 0; t < num_travelers; t++)
            if (anims[t].phase != PHASE_ARRIVED) { any_active = 1; break; }

        if (any_active)
            draw_play_button(global_paused);

        /* Draw all trains (including arrived ones so they stay visible).
           Compute how many active trains share each screen position and
           assign slots so they orbit the node center instead of stacking. */
        for (int t = 0; t < num_travelers; t++) {
            int total = 0, slot = 0;
            for (int u = 0; u < num_travelers; u++) {
                float dx = anims[u].train_x - anims[t].train_x;
                float dy = anims[u].train_y - anims[t].train_y;
                if (dx*dx + dy*dy < 4.0f) {   /* within 2px = same position */
                    if (u < t) slot++;
                    total++;
                }
            }
            draw_train(&anims[t], slot, total);
        }

        /* Arrived overlays (bottom-left, stacked) */
        int slot = 0;
        for (int t = 0; t < num_travelers; t++) {
            if (anims[t].phase == PHASE_ARRIVED) {
                draw_arrived_overlay(&anims[t], slot, num_travelers);
                slot++;
            }
        }

        EndDrawing();

        /* Exit the loop once all travelers have arrived */
        int all_done = 1;
        for (int t = 0; t < num_travelers; t++)
            if (anims[t].phase != PHASE_ARRIVED) { all_done = 0; break; }
        if (all_done) {
            /* Give the user a moment to see the final state */
            WaitTime(2.0f);
            break;
        }
    }

    CloseWindow();

    /* Send SIGTERM to any children that didn't finish naturally
       (e.g. user closed the window before all trains arrived).
       Children that already received SIGTERM are unaffected by a second one. */
    for (int t = 0; t < num_travelers; t++) {
        if (anims[t].child_pid > 0)
            kill(anims[t].child_pid, SIGTERM);
    }
}