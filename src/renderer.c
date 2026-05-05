#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "raylib.h"
#include "renderer.h"
#include "graph.h"
#include "dijkstra.h"

/* ── Color palette ─────────────────────────────────────── */
#define COL_BG          ((Color){ 15,  17,  26, 255})   /* deep navy */
#define COL_GRID        ((Color){ 30,  35,  55, 255})   /* subtle grid */
#define COL_NODE        ((Color){ 20,  30,  60, 255})   /* node fill */
#define COL_NODE_BORDER ((Color){ 80, 140, 255, 255})   /* electric blue */
#define COL_EDGE        ((Color){100, 120, 180, 100})   /* muted blue edge */
#define COL_SOURCE      ((Color){255, 255, 255, 255})   /* white source node */
#define COL_PATH        ((Color){ 50, 255, 150, 255})   /* neon green path */
#define COL_DEST        ((Color){255, 150,  50, 255})   /* amber dest node */
#define COL_ARROW       ((Color){ 80, 140, 255, 255})   /* default arrow */
#define COL_WEIGHT      ((Color){255, 200,  80, 255})   /* amber weight label */
#define COL_LABEL       ((Color){220, 230, 255, 255})   /* node ID */
#define COL_NAME        ((Color){160, 200, 255, 200})   /* station name */
#define COL_TITLE       ((Color){ 80, 140, 255, 255})   /* header text */
#define COL_TRAIN       ((Color){255,  80,  80, 255})   /* train body */
#define COL_TRAIN_GLOW  ((Color){255,  80,  80,  60})   /* train glow */
#define COL_BTN_PLAY    ((Color){ 40, 180,  80, 255})   /* play button */
#define COL_BTN_STOP    ((Color){200,  60,  60, 255})   /* stop button */

/* ── Layout ─────────────────────────────────────────────── */

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

    float step  = (2.0f * 3.14159265f) / num_nodes;
    float start = -3.14159265f / 2.0f; /* start at top */

    for (int i = 0; i < num_nodes; i++) {
        float angle    = start + i * step;
        positions[i].x = cx + r * cosf(angle);
        positions[i].y = cy + r * sinf(angle);
    }
}

/* ── Path helpers ───────────────────────────────────────── */

static bool is_edge_on_path(int u, int v, const DijkstraResult *res) {
    if (!res || !res->found || res->path_len < 2) return false;
    for (int i = 0; i < res->path_len - 1; i++) {
        if (res->path[i] == u && res->path[i + 1] == v) return true;
    }
    return false;
}

static bool is_node_on_path(int u, const DijkstraResult *res) {
    if (!res || !res->found) return false;
    for (int i = 0; i < res->path_len; i++) {
        if (res->path[i] == u) return true;
    }
    return false;
}

/* ── Drawing helpers ────────────────────────────────────── */

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

    float margin = NODE_RADIUS + 8.0f;
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

/* ── Graph draw ─────────────────────────────────────────── */

void renderer_draw_graph(const Graph *g, const Vec2 *pos,
                         const char **station_names,
                         const DijkstraResult *res,
                         int src_id, int dst_id) {
    ClearBackground(COL_BG);
    draw_grid();

    /* Title */
    DrawText("TrainOS  |  Railway Network", 25, 20, 22, COL_TITLE);

    /* Route info bar */
    if (res && res->found) {
        char info[128];
        snprintf(info, sizeof(info),
                 "Shortest path: %d min  |  %d stations",
                 res->total_cost, res->path_len);
        DrawText(info, 25, 50, 16, WHITE);
    } else if (res && !res->found) {
        DrawText("No path found between source and destination.",
                 25, 50, 16, RED);
    }

    /* ── Edges ── */
    for (int u = 0; u < g->num_vertices; u++) {
        EdgeNode *e = g->lists[u].head;
        while (e) {
            int   v       = e->dest;
            bool  on_path = is_edge_on_path(u, v, res);
            Color col     = on_path ? COL_PATH : COL_EDGE;
            float thick   = on_path ? 4.0f : 1.5f;

            draw_arrow(pos[u].x, pos[u].y, pos[v].x, pos[v].y, col, thick);
            draw_weight_label(pos[u].x, pos[u].y, pos[v].x, pos[v].y, e->weight);
            e = e->next;
        }
    }

    /* ── Nodes ── */
    for (int i = 0; i < g->num_vertices; i++) {
        Color border = COL_NODE_BORDER;
        if      (i == src_id)             border = COL_SOURCE;
        else if (i == dst_id)             border = COL_DEST;
        else if (is_node_on_path(i, res)) border = COL_PATH;

        /* Glow ring */
        DrawCircle((int)pos[i].x, (int)pos[i].y,
                   NODE_RADIUS + 5, Fade(border, 0.3f));
        /* Fill */
        DrawCircle((int)pos[i].x, (int)pos[i].y, NODE_RADIUS, COL_NODE);
        /* Border */
        DrawCircleLines((int)pos[i].x, (int)pos[i].y, NODE_RADIUS, border);

        /* Node ID */
        char id_buf[16];
        snprintf(id_buf, sizeof(id_buf), "%d", i);
        int iw = MeasureText(id_buf, 18);
        DrawText(id_buf,
                 (int)(pos[i].x - iw / 2),
                 (int)(pos[i].y - 9),
                 18, COL_LABEL);

        /* Station name */
        if (station_names && station_names[i]) {
            int nw = MeasureText(station_names[i], 12);
            DrawText(station_names[i],
                     (int)(pos[i].x - nw / 2),
                     (int)(pos[i].y + NODE_RADIUS + 8),
                     12, COL_NAME);
        }
    }

    /* ── Legend ── */
    int lx = WINDOW_WIDTH - 190;
    int ly = WINDOW_HEIGHT - 100;
    DrawRectangleLines(lx - 10, ly - 10, 185, 95, COL_GRID);

    DrawCircle(lx, ly,      6, COL_SOURCE);
    DrawText("Source",       lx + 15, ly - 6,  12, WHITE);

    DrawCircle(lx, ly + 28, 6, COL_DEST);
    DrawText("Destination",  lx + 15, ly + 22, 12, WHITE);

    DrawLineEx((Vector2){lx - 6, ly + 56},
               (Vector2){lx + 6, ly + 56}, 3, COL_PATH);
    DrawText("Shortest path", lx + 15, ly + 50, 12, WHITE);
}

/* ── Play/Stop button ───────────────────────────────────── */

#define BTN_X (WINDOW_WIDTH  - 140)
#define BTN_Y 10
#define BTN_W 120
#define BTN_H 40

static void draw_play_button(int paused) {
    Color       bg    = paused ? COL_BTN_PLAY : COL_BTN_STOP;
    const char *label = paused ? "  PLAY"     : "  STOP";
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

/* ── Train animation ────────────────────────────────────── */

/* Look up edge weight for (u -> v) in the graph */
static int edge_weight(const Graph *g, int u, int v) {
    EdgeNode *e = g->lists[u].head;
    while (e) {
        if (e->dest == v) return e->weight;
        e = e->next;
    }
    return 1; /* fallback, should never happen on a valid path */
}

/* Initialise animation state; train starts paused at source */
static void anim_init(TrainAnim *a, const DijkstraResult *res,
                      const Vec2 *pos) {
    memset(a, 0, sizeof(*a));

    if (!res || !res->found || res->path_len < 1) {
        a->phase = PHASE_ARRIVED; /* nothing to animate */
        return;
    }

    /* src == dst: already at destination */
    if (res->path_len == 1) {
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
    a->paused     = 1; /* user must press PLAY to start */
    a->timer_ms   = 0.0;
    a->train_x    = pos[res->path[0]].x;
    a->train_y    = pos[res->path[0]].y;
}

/* Advance animation by delta_ms; updates train_x/train_y */
static void anim_update(TrainAnim *a, const Graph *g,
                        const Vec2 *pos, double delta_ms) {
    if (a->paused || a->phase == PHASE_ARRIVED) return;

    a->timer_ms += delta_ms;

    if (a->phase == PHASE_IDLE) {
        /* Waiting at an intermediate station */
        if (a->timer_ms >= MS_STATION_WAIT) {
            a->timer_ms   = 0.0;
            a->seg++;
            if (a->seg + 1 >= a->path_len) {
                a->phase = PHASE_ARRIVED;
                return;
            }
            a->phase      = PHASE_TRAVELLING;
            a->seg_weight = edge_weight(g, a->path[a->seg],
                                           a->path[a->seg + 1]);
        }
        return;
    }

    /* PHASE_TRAVELLING: lazy-load segment weight on first tick */
    if (a->seg_weight == 0)
        a->seg_weight = edge_weight(g, a->path[a->seg],
                                       a->path[a->seg + 1]);

    int u = a->path[a->seg];
    int v = a->path[a->seg + 1];

    /* Smooth interpolation: (elapsed_jumps + fractional) / total_jumps */
    double total_ms = (double)a->seg_weight * MS_PER_JUMP;
    float  t        = (float)(a->timer_ms / total_ms);
    if (t > 1.0f) t = 1.0f;

    a->train_x = pos[u].x + t * (pos[v].x - pos[u].x);
    a->train_y = pos[u].y + t * (pos[v].y - pos[u].y);

    /* Reached end of this edge */
    if (a->timer_ms >= total_ms) {
        a->train_x    = pos[v].x;
        a->train_y    = pos[v].y;
        a->timer_ms   = 0.0;
        a->seg_weight = 0;

        if (a->seg + 2 >= a->path_len) {
            a->phase = PHASE_ARRIVED;
        } else {
            a->phase = PHASE_IDLE; /* wait 1 s at intermediate station */
        }
    }
}

/* Draw the train circle at its current position */
static void draw_train(const TrainAnim *a) {
    if (a->path_len == 0) return;

    /* Glow */
    DrawCircle((int)a->train_x, (int)a->train_y,
               NODE_RADIUS - 4, COL_TRAIN_GLOW);
    /* Body */
    DrawCircle((int)a->train_x, (int)a->train_y,
               NODE_RADIUS - 10, COL_TRAIN);
    /* Border */
    DrawCircleLines((int)a->train_x, (int)a->train_y,
                    NODE_RADIUS - 10, WHITE);
    /* Label */
    DrawText("T",
             (int)(a->train_x - 5),
             (int)(a->train_y - 8),
             16, WHITE);
}

/* Centred overlay shown when train reaches destination */
static void draw_arrived_message(int src, int dst, int total_cost) {
    char line2[64];
    snprintf(line2, sizeof(line2),
             "Route %d -> %d  |  Total: %d min", src, dst, total_cost);

    int bw = 480, bh = 90;
    int bx = WINDOW_WIDTH  / 2 - bw / 2;
    int by = WINDOW_HEIGHT / 2 - bh / 2;

    DrawRectangleRounded((Rectangle){(float)bx, (float)by,
                                     (float)bw, (float)bh},
                         0.2f, 8, (Color){10, 20, 40, 230});
    DrawRectangleRoundedLines((Rectangle){(float)bx, (float)by,
                                          (float)bw, (float)bh},
                               0.2f, 8, 2.0f, COL_DEST);

    const char *line1 = "Train has arrived!";
    int tw1 = MeasureText(line1, 22);
    DrawText(line1, WINDOW_WIDTH / 2 - tw1 / 2, by + 14, 22, COL_DEST);

    int tw2 = MeasureText(line2, 16);
    DrawText(line2, WINDOW_WIDTH / 2 - tw2 / 2, by + 48, 16, COL_LABEL);
}

/* Status bar bottom-right */
static void draw_status(const TrainAnim *a, int src, int dst) {
    char buf[128];

    if (a->phase == PHASE_ARRIVED) {
        snprintf(buf, sizeof(buf),
                 "Status: Arrived at station %d", dst);
    } else if (a->paused) {
        int cur = (a->path && a->path_len > 0) ? a->path[a->seg] : src;
        snprintf(buf, sizeof(buf),
                 "Status: Paused at station %d  |  Press PLAY", cur);
    } else if (a->phase == PHASE_IDLE) {
        snprintf(buf, sizeof(buf),
                 "Status: Waiting at station %d", a->path[a->seg + 1]);
    } else {
        int jump_now = (int)(a->timer_ms / MS_PER_JUMP) + 1;
        snprintf(buf, sizeof(buf),
                 "Status: Travelling  %d -> %d  (jump %d / %d)",
                 a->path[a->seg], a->path[a->seg + 1],
                 jump_now, a->seg_weight);
    }

    int tw = MeasureText(buf, 13);
    DrawText(buf, WINDOW_WIDTH - tw - 12, WINDOW_HEIGHT - 26,
             13, (Color){160, 200, 180, 220});
}

/* ── Main window loop ───────────────────────────────────── */

void renderer_run(const Graph *g, const char **station_names,
                  const DijkstraResult *res, int src_id, int dst_id) {
    Vec2 positions[MAX_NODES];
    renderer_compute_positions(g->num_vertices, positions);

    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
               "TrainOS - Railway Traffic Simulation");
    SetTargetFPS(60);

    TrainAnim anim;
    anim_init(&anim, res, positions);

    /* Preload weight for the first edge */
    if (anim.phase == PHASE_TRAVELLING && anim.path_len >= 2)
        anim.seg_weight = edge_weight(g, anim.path[0], anim.path[1]);

    while (!WindowShouldClose()) {
        double delta_ms = GetFrameTime() * 1000.0;

        /* Toggle play/stop */
        if (button_clicked() && anim.phase != PHASE_ARRIVED)
            anim.paused = !anim.paused;

        /* Advance animation */
        anim_update(&anim, g, positions, delta_ms);

        BeginDrawing();

        /* Static graph (always drawn) */
        renderer_draw_graph(g, positions, station_names,
                            res, src_id, dst_id);

        /* Play/Stop button (hidden after arrival) */
        if (anim.phase != PHASE_ARRIVED)
            draw_play_button(anim.paused);

        /* Train */
        draw_train(&anim);

        /* Arrived overlay */
        if (anim.phase == PHASE_ARRIVED && res && res->found
                && res->path_len > 1)
            draw_arrived_message(src_id, dst_id, res->total_cost);

        /* Status bar */
        draw_status(&anim, src_id, dst_id);

        EndDrawing();
    }

    CloseWindow();
}
