#include <math.h>
#include <stdio.h>
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

/* ── Main draw call ─────────────────────────────────────── */

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
            int   v      = e->dest;
            bool  on_path = is_edge_on_path(u, v, res);
            Color col    = on_path ? COL_PATH : COL_EDGE;
            float thick  = on_path ? 4.0f : 1.5f;

            draw_arrow(pos[u].x, pos[u].y, pos[v].x, pos[v].y, col, thick);
            draw_weight_label(pos[u].x, pos[u].y, pos[v].x, pos[v].y, e->weight);
            e = e->next;
        }
    }

    /* ── Nodes ── */
    for (int i = 0; i < g->num_vertices; i++) {
        Color border = COL_NODE_BORDER;
        if      (i == src_id)            border = COL_SOURCE;
        else if (i == dst_id)            border = COL_DEST;
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
    DrawText("Source",       lx + 15, ly - 6,      12, WHITE);

    DrawCircle(lx, ly + 28, 6, COL_DEST);
    DrawText("Destination",  lx + 15, ly + 22,     12, WHITE);

    DrawLineEx((Vector2){lx - 6, ly + 56},
               (Vector2){lx + 6, ly + 56}, 3, COL_PATH);
    DrawText("Shortest path", lx + 15, ly + 50,    12, WHITE);
}

/* ── Window loop ────────────────────────────────────────── */

void renderer_run(const Graph *g, const char **station_names,
                  const DijkstraResult *res, int src_id, int dst_id) {
    Vec2 positions[MAX_NODES];
    renderer_compute_positions(g->num_vertices, positions);

    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
               "TrainOS - Railway Traffic Simulation");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        renderer_draw_graph(g, positions, station_names,
                            res, src_id, dst_id);
        EndDrawing();
    }

    CloseWindow();
}