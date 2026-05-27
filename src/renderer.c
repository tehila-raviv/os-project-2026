#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include "raylib.h"
#include "renderer.h"
#include "ipc.h"         /* GO_SIGNAL */
#include "ipc.h"
#include "graph.h"

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

static const Color TRAVELER_COLORS[MAX_TRAVELERS] = {
    {255,  80,  80, 255},  {  80, 200, 255, 255},
    {255, 200,  50, 255},  {  80, 255, 130, 255},
    {200,  80, 255, 255},  { 255, 160,  40, 255},
    { 40, 180, 255, 255},  { 255,  80, 180, 255},
    {160, 255,  80, 255},  {  80,  80, 255, 255},
    {255, 240, 180, 255},  {   0, 220, 180, 255},
    {220, 120, 120, 255},  { 120, 220, 120, 255},
    {220, 220,  80, 255},  { 160, 120, 255, 255},
};

/* ── Layout ──────────────────────────────────────────────────── */

void renderer_compute_positions(int num_nodes, Vec2 *positions) {
    float cx = WINDOW_WIDTH  / 2.0f;
    float cy = WINDOW_HEIGHT / 2.0f;
    float r  = (WINDOW_HEIGHT < WINDOW_WIDTH ? WINDOW_HEIGHT : WINDOW_WIDTH)
               * 0.36f;
    if (num_nodes == 1) { positions[0].x = cx; positions[0].y = cy; return; }

    float step  = (2.0f * 3.14159265f) / (float)num_nodes;
    float start = -3.14159265f / 2.0f;
    for (int i = 0; i < num_nodes; i++) {
        float angle    = start + (float)i * step;
        positions[i].x = cx + r * cosf(angle);
        positions[i].y = cy + r * sinf(angle);
    }
}

/* ── Drawing helpers ─────────────────────────────────────────── */

static void draw_grid(void) {
    for (int x = 0; x < WINDOW_WIDTH;  x += 40)
        DrawLine(x, 0, x, WINDOW_HEIGHT, COL_GRID);
    for (int y = 0; y < WINDOW_HEIGHT; y += 40)
        DrawLine(0, y, WINDOW_WIDTH, y, COL_GRID);
}

static void draw_arrow(float x1, float y1, float x2, float y2,
                       Color col, float thickness) {
    float dx = x2-x1, dy = y2-y1;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 1.0f) return;
    float ux = dx/len, uy = dy/len;
    float margin = (float)NODE_RADIUS + 8.0f;
    float sx = x1+ux*margin, sy = y1+uy*margin;
    float ex = x2-ux*margin, ey = y2-uy*margin;
    DrawLineEx((Vector2){sx,sy}, (Vector2){ex,ey}, thickness, col);
    float head = 14.0f+thickness, angle = 0.45f;
    float dir  = atan2f(ey-sy, ex-sx);
    DrawTriangle((Vector2){ex,ey},
                 (Vector2){ex-head*cosf(dir-angle), ey-head*sinf(dir-angle)},
                 (Vector2){ex-head*cosf(dir+angle), ey-head*sinf(dir+angle)},
                 col);
}

static void draw_weight_label(float x1, float y1, float x2, float y2, int w) {
    float mx = (x1+x2)/2.0f, my = (y1+y2)/2.0f;
    float dx = x2-x1, dy = y2-y1;
    float len = sqrtf(dx*dx+dy*dy);
    if (len > 0.1f) { mx += (-dy/len)*14.0f; my += (dx/len)*14.0f; }
    char buf[16]; snprintf(buf, sizeof(buf), "%d", w);
    int tw = MeasureText(buf, 14);
    DrawRectangle((int)(mx-tw/2-3),(int)(my-9), tw+6, 18, (Color){10,12,22,200});
    DrawText(buf, (int)(mx-tw/2), (int)(my-7), 14, COL_WEIGHT);
}

/* Returns true if node u is the current or next stop of any traveler */
static bool node_is_active(int u, const TrainAnim *anims, int num) {
    for (int t = 0; t < num; t++) {
        if (anims[t].phase == PHASE_ARRIVED) continue;
        if (anims[t].cur_node == u || anims[t].next_node == u) return true;
    }
    return false;
}

/* Returns true if directed edge u->v is the current segment of any traveler */
static bool edge_is_active(int u, int v, const TrainAnim *anims, int num,
                            int *owner) {
    for (int t = 0; t < num; t++) {
        if (anims[t].phase == PHASE_TRAVELLING &&
            anims[t].cur_node == u && anims[t].next_node == v) {
            *owner = t;
            return true;
        }
    }
    return false;
}

/* Returns true if edge u->v is anywhere on traveler t's full stored path */
static bool edge_on_full_path(int u, int v, const TrainAnim *a) {
    if (!a->path || a->path_len < 2) return false;
    for (int i = 0; i < a->path_len - 1; i++)
        if (a->path[i] == u && a->path[i + 1] == v) return true;
    return false;
}

/* ── Graph draw ──────────────────────────────────────────────── */

void renderer_draw_graph(const Graph     *g,
                         const Vec2      *pos,
                         const char     **station_names,
                         const TrainAnim *anims,
                         int              num_travelers,
                         int              animate)
{
    ClearBackground(COL_BG);
    draw_grid();
    DrawText("TrainOS  |  Railway Network", 25, 20, 22, COL_TITLE);

    /* Edges */
    for (int u = 0; u < g->num_vertices; u++) {
        EdgeNode *e = g->lists[u].head;
        while (e) {
            int  v = e->dest, owner = -1;
            Color col   = COL_EDGE;
            float thick = 1.5f;
            if (!animate) {
                /* M2 static mode: highlight full stored path for each traveler */
                for (int t = 0; t < num_travelers; t++) {
                    if (edge_on_full_path(u, v, &anims[t])) {
                        col   = TRAVELER_COLORS[anims[t].color_index % MAX_TRAVELERS];
                        thick = 3.5f;
                        break;
                    }
                }
            } else {
                /* M3/M4/M5: highlight only the current active segment */
                bool active = edge_is_active(u, v, anims, num_travelers, &owner);
                if (active) {
                    col   = TRAVELER_COLORS[anims[owner].color_index % MAX_TRAVELERS];
                    thick = 3.5f;
                }
            }
            draw_arrow(pos[u].x, pos[u].y, pos[v].x, pos[v].y, col, thick);
            draw_weight_label(pos[u].x, pos[u].y, pos[v].x, pos[v].y, e->weight);
            e = e->next;
        }
    }

    /* Nodes */
    for (int i = 0; i < g->num_vertices; i++) {
        Color border = COL_NODE_BORDER;
        /* Only highlight active node in animated modes, not static M2 */
        if (animate && anims && node_is_active(i, anims, num_travelers))
            border = WHITE;
        /* Tint source/destination nodes for each traveler */
        for (int t = 0; anims && t < num_travelers; t++) {
            if (i == anims[t].src_id || i == anims[t].dst_id) {
                border = TRAVELER_COLORS[anims[t].color_index % MAX_TRAVELERS];
                break;
            }
        }
        DrawCircle((int)pos[i].x,(int)pos[i].y, NODE_RADIUS+5, Fade(border,0.3f));
        DrawCircle((int)pos[i].x,(int)pos[i].y, NODE_RADIUS,   COL_NODE);
        DrawCircleLines((int)pos[i].x,(int)pos[i].y, NODE_RADIUS, border);

        /* Static table avoids -Wformat-truncation on "%d" for node IDs */
        static const char *NODE_IDS[] = {
            "0","1","2","3","4","5","6","7",
            "8","9","10","11","12","13","14"
        };
        const char *id_buf = (i >= 0 && i < MAX_NODES) ? NODE_IDS[i] : "?";
        int iw = MeasureText(id_buf, 18);
        DrawText(id_buf, (int)(pos[i].x-iw/2), (int)(pos[i].y-9), 18, COL_LABEL);

        if (station_names && station_names[i]) {
            int nw = MeasureText(station_names[i], 12);
            DrawText(station_names[i],
                     (int)(pos[i].x-nw/2), (int)(pos[i].y+NODE_RADIUS+8),
                     12, COL_NAME);
        }
    }

    /* Legend */
    int lx = WINDOW_WIDTH - 200, ly = 80;
    DrawRectangleLines(lx-10, ly-10, 190, 24*num_travelers+20, COL_GRID);
    for (int t = 0; anims && t < num_travelers; t++) {
        Color tc = TRAVELER_COLORS[anims[t].color_index % MAX_TRAVELERS];
        DrawCircle(lx, ly+t*24, 7, tc);
        char leg[48];
        const char *status = (anims[t].phase == PHASE_ARRIVED) ? " [arrived]" : "";
        snprintf(leg, sizeof(leg), "Train %d: %d->%d%s",
                 t+1, anims[t].src_id, anims[t].dst_id, status);
        DrawText(leg, lx+16, ly+t*24-7, 12, WHITE);
    }
}

/* ── Play/Stop button ────────────────────────────────────────── */
#define BTN_X (WINDOW_WIDTH - 140)
#define BTN_Y 10
#define BTN_W 120
#define BTN_H 40

static void draw_play_button(int paused) {
    Color       bg  = paused ? COL_BTN_PLAY : COL_BTN_STOP;
    const char *lbl = paused ? "  PLAY"     : "  STOP";
    DrawRectangleRounded((Rectangle){BTN_X,BTN_Y,BTN_W,BTN_H}, 0.3f, 8, bg);
    DrawRectangleRoundedLines((Rectangle){BTN_X,BTN_Y,BTN_W,BTN_H},0.3f,8,1.5f,WHITE);
    DrawText(lbl, BTN_X+10, BTN_Y+11, 18, WHITE);
}

static int button_clicked(void) {
    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) return 0;
    Vector2 mp = GetMousePosition();
    return mp.x >= BTN_X && mp.x <= BTN_X+BTN_W &&
           mp.y >= BTN_Y && mp.y <= BTN_Y+BTN_H;
}

/* ── Train draw ──────────────────────────────────────────────── */

static void draw_train(const TrainAnim *a, int offset_index) {
    if (a->phase == PHASE_ARRIVED) return;

    Color tc = TRAVELER_COLORS[a->color_index % MAX_TRAVELERS];
    Color glow = tc; glow.a = 60;

    float ox = (float)(offset_index % 3) * 6.0f - 6.0f;
    float oy = (float)(offset_index / 3) * 6.0f - 3.0f;
    float cx = a->train_x + ox;
    float cy = a->train_y + oy;

    DrawCircle((int)cx,(int)cy, NODE_RADIUS-4,  glow);
    DrawCircle((int)cx,(int)cy, NODE_RADIUS-10, tc);
    DrawCircleLines((int)cx,(int)cy, NODE_RADIUS-10, WHITE);

    static const char *LABELS[MAX_TRAVELERS] = {
        "T1","T2","T3","T4","T5","T6","T7","T8",
        "T9","T10","T11","T12","T13","T14","T15","T16"
    };
    const char *lbl = LABELS[a->color_index % MAX_TRAVELERS];
    int lw = MeasureText(lbl, 13);
    DrawText(lbl, (int)(cx-lw/2), (int)(cy-7), 13, WHITE);
}

static void draw_arrived_overlay(const TrainAnim *a, int slot) {
    Color tc = TRAVELER_COLORS[a->color_index % MAX_TRAVELERS];
    int bw=360, bh=36, bx=20, by=WINDOW_HEIGHT-45-slot*42;
    DrawRectangleRounded((Rectangle){(float)bx,(float)by,(float)bw,(float)bh},
                         0.25f, 8, (Color){10,20,40,220});
    DrawRectangleRoundedLines((Rectangle){(float)bx,(float)by,(float)bw,(float)bh},
                               0.25f, 8, 1.5f, tc);
    char msg[80];
    snprintf(msg, sizeof(msg), "Train %d arrived: %d -> %d",
             a->color_index+1, a->src_id, a->dst_id);
    int tw = MeasureText(msg, 16);
    DrawText(msg, bx+bw/2-tw/2, by+10, 16, tc);
}

/* ── Edge weight lookup ──────────────────────────────────────── */

static int edge_weight_lookup(const Graph *g, int u, int v) {
    EdgeNode *e = g->lists[u].head;
    while (e) { if (e->dest == v) return e->weight; e = e->next; }
    return 1;
}

/* ── Animation update (driven by IPC, not by path array) ──────── */

/*
 * Called every frame. Smoothly slides the train from cur_node to next_node.
 * When the slide finishes, the train waits for the next IPC message
 * (phase stays PHASE_IDLE until the parent receives and applies a new message).
 */
static void anim_update(TrainAnim *a, const Graph *g,
                        const Vec2 *pos, double delta_ms, int paused)
{
    if (paused || a->phase == PHASE_ARRIVED) return;

    /* M2/M3/M4 path-driven: after station pause, start next segment */
    if (a->phase == PHASE_IDLE) {
        if (a->path == NULL) return;   /* M5: wait for IPC message */
        a->timer_ms += delta_ms;
        if (a->timer_ms >= MS_STATION_WAIT) {
            a->timer_ms = 0.0;
            if (a->seg + 1 < a->path_len) {
                a->phase = PHASE_TRAVELLING;
            }
        }
        return;
    }

    /* PHASE_TRAVELLING: slide from cur_node to next_node */
    int u = a->cur_node;
    int v = a->next_node;
    int w = edge_weight_lookup(g, u, v);

    a->timer_ms += delta_ms;
    double total_ms = (double)w * MS_PER_JUMP;
    float  t        = (float)(a->timer_ms / total_ms);
    if (t > 1.0f) t = 1.0f;

    a->train_x = pos[u].x + t * (pos[v].x - pos[u].x);
    a->train_y = pos[u].y + t * (pos[v].y - pos[u].y);

    if (a->timer_ms >= total_ms) {
        a->train_x  = pos[v].x;
        a->train_y  = pos[v].y;
        a->timer_ms = 0.0;

        if (a->path != NULL) {
            /* M2/M3/M4: path-driven — auto-advance to next segment */
            a->seg++;
            a->cur_node = a->path[a->seg];
            if (a->seg + 1 < a->path_len) {
                a->next_node  = a->path[a->seg + 1];
                a->phase      = PHASE_IDLE;   /* brief pause then continue */
            } else {
                a->next_node = -1;
                a->phase     = PHASE_ARRIVED;
            }
        } else {
            /* M5: IPC-driven — wait for next message from child */
            a->phase = PHASE_IDLE;
        }
    }
}

/* ── IPC polling ─────────────────────────────────────────────── */

/*
 * Read all pending IpcMsg structs from child t's pipe (non-blocking).
 * For each message received, print the log line and update the animation.
 */
/*
 * Read at most ONE IpcMsg per call (not a drain loop).
 *
 * Why one message at a time?
 * The child sleeps (edge_weight * MS_PER_JUMP) ms between messages, which
 * matches the animation slide duration exactly. If we drained all pending
 * messages in one frame, the parent would consume future nodes before their
 * slides finished, making the train jump to the final destination instantly.
 * Reading one message per frame keeps IPC and animation in lockstep.
 *
 * We only read a new message when the current slide has finished (PHASE_IDLE),
 * so the pipe naturally self-throttles.
 */
static void poll_pipe(TrainAnim *a, int fd, const Vec2 *pos, int paused) {
    /* Skip if no pipe (M2/M3/M4 pass fd=-1) or animation still running */
    if (fd < 0 || a->phase == PHASE_TRAVELLING) return;

    IpcMsg msg;
    ssize_t n = read(fd, &msg, sizeof(IpcMsg));
    if (n != (ssize_t)sizeof(IpcMsg)) return;   /* EAGAIN = no data yet */

    /* Parent prints all log output */
    if (msg.next_node == -1)
        printf("[PID=%d] arrived at node %d | DESTINATION\n",
               (int)a->child_pid, msg.current_node);
    else
        printf("[PID=%d] arrived at node %d | next node: %d\n",
               (int)a->child_pid, msg.current_node, msg.next_node);
    fflush(stdout);

    a->cur_node  = msg.current_node;
    a->next_node = msg.next_node;

    if (msg.next_node == -1) {
        a->train_x = pos[msg.current_node].x;
        a->train_y = pos[msg.current_node].y;
        a->phase   = PHASE_ARRIVED;
    } else if (!paused) {
        a->timer_ms = 0.0;
        a->phase    = PHASE_TRAVELLING;
    }
    /* If paused, stays IDLE; unpausing will kick it to TRAVELLING */
}

/* ── Main window loop ────────────────────────────────────────── */

void renderer_run(const Graph  *g,
                  const char  **station_names,
                  TrainAnim    *anims,
                  int           num_travelers,
                  int          *read_fds,
                  int          *go_fds,
                  int           animate)
{
    Vec2 positions[MAX_NODES];
    renderer_compute_positions(g->num_vertices, positions);

    /* Make all pipe read-ends non-blocking */
    for (int t = 0; t < num_travelers; t++) {
        int flags = fcntl(read_fds[t], F_GETFL, 0);
        fcntl(read_fds[t], F_SETFL, flags | O_NONBLOCK);
    }

    /* Snap every train to its source node position before the loop.
       This prevents trains from appearing at (0,0) until the first
       IPC message arrives. */
    for (int t = 0; t < num_travelers; t++) {
        int s = anims[t].src_id;
        if (s >= 0 && s < g->num_vertices) {
            anims[t].train_x = positions[s].x;
            anims[t].train_y = positions[s].y;
        }
    }

    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
               "TrainOS - Railway Traffic Simulation");
    SetTargetFPS(60);

    int global_paused = 1;   /* trains start paused, user presses PLAY */

    while (!WindowShouldClose()) {
        double delta_ms = GetFrameTime() * 1000.0;

        /* Toggle play/stop */
        if (button_clicked()) {
            global_paused = !global_paused;
            if (!global_paused) {
                /* Send GO_SIGNAL to every child (M5 only).
                   go_fds is NULL in M2/M3/M4 — skip in that case. */
                if (go_fds) {
                    /* M5: send GO_SIGNAL to unblock each child */
                    for (int t = 0; t < num_travelers; t++) {
                        if (go_fds[t] > 0) {
                            char go = GO_SIGNAL;
                            (void)write(go_fds[t], &go, 1);
                            close(go_fds[t]);
                            go_fds[t] = -1;
                        }
                    }
                } else {
                    /* M2/M3/M4: no IPC — directly start all path-driven trains */
                    for (int t = 0; t < num_travelers; t++) {
                        if (anims[t].phase == PHASE_IDLE &&
                            anims[t].path != NULL &&
                            anims[t].seg + 1 < anims[t].path_len) {
                            anims[t].timer_ms = 0.0;
                            anims[t].phase    = PHASE_TRAVELLING;
                        }
                    }
                }
            }
        }

        /* Poll pipes and advance animations (M3/M4/M5 only) */
        if (animate) {
            for (int t = 0; t < num_travelers; t++) {
                if (anims[t].phase != PHASE_ARRIVED)
                    poll_pipe(&anims[t], read_fds[t], positions, global_paused);
            }
            for (int t = 0; t < num_travelers; t++)
                anim_update(&anims[t], g, positions, delta_ms, global_paused);
        }

        BeginDrawing();

        renderer_draw_graph(g, positions, station_names, anims, num_travelers, animate);

        /* Show PLAY/STOP button only in animated modes (M3/M4/M5) */
        int any_active = 0;
        for (int t = 0; t < num_travelers; t++)
            if (anims[t].phase != PHASE_ARRIVED) { any_active = 1; break; }
        if (animate && any_active) draw_play_button(global_paused);

        /* In static mode (M2) trains are not drawn — path highlight is enough */
        if (animate) {
            for (int t = 0; t < num_travelers; t++)
                draw_train(&anims[t], t);
        }

        int slot = 0;
        for (int t = 0; t < num_travelers; t++)
            if (anims[t].phase == PHASE_ARRIVED)
                draw_arrived_overlay(&anims[t], slot++);

        EndDrawing();

        /* Exit when all travelers have arrived */
        int all_done = 1;
        for (int t = 0; t < num_travelers; t++)
            if (anims[t].phase != PHASE_ARRIVED) { all_done = 0; break; }
        if (all_done) { WaitTime(2.0f); break; }
    }

    CloseWindow();

    /* Ensure all children are terminated when window closes */
    for (int t = 0; t < num_travelers; t++)
        if (anims[t].child_pid > 0)
            kill(anims[t].child_pid, SIGTERM);
}