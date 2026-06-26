#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include "raylib.h"
#include "renderer.h"
#include "ipc.h"
#include "graph.h"
#include "scheduler.h"

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
#define COL_WAITING     ((Color){255, 165,   0, 255})
#define COL_ALGO_LABEL  ((Color){160, 255, 160, 255})

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

static bool node_is_active(int u, const TrainAnim *anims, int num) {
    for (int t = 0; t < num; t++) {
        if (anims[t].phase == PHASE_ARRIVED) continue;
        if (anims[t].cur_node == u || anims[t].next_node == u) return true;
    }
    return false;
}

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
                         int              animate,
                         SchedAlgo        algo)
{
    ClearBackground(COL_BG);
    draw_grid();
    DrawText("TrainOS  |  Railway Network", 25, 20, 22, COL_TITLE);

    /* Active scheduler label — only shown in Milestone 7 */
    if (algo != SCHED_NONE) {
        char algo_buf[32];
        snprintf(algo_buf, sizeof(algo_buf), "Scheduler: %s", sched_algo_name(algo));
        DrawText(algo_buf, 25, 48, 16, COL_ALGO_LABEL);
    }

    /* Edges */
    for (int u = 0; u < g->num_vertices; u++) {
        EdgeNode *e = g->lists[u].head;
        while (e) {
            int  v = e->dest, owner = -1;
            Color col   = COL_EDGE;
            float thick = 1.5f;
            if (!animate) {
                for (int t = 0; t < num_travelers; t++) {
                    if (edge_on_full_path(u, v, &anims[t])) {
                        col   = TRAVELER_COLORS[anims[t].color_index % MAX_TRAVELERS];
                        thick = 3.5f;
                        break;
                    }
                }
            } else {
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

        if (animate && anims && node_is_active(i, anims, num_travelers))
            border = WHITE;

        for (int t = 0; anims && t < num_travelers; t++) {
            if (i == anims[t].src_id || i == anims[t].dst_id) {
                border = TRAVELER_COLORS[anims[t].color_index % MAX_TRAVELERS];
                break;
            }
        }

        /* Orange ring if any traveler is waiting for this node */
        bool node_locked = false;
        for (int t = 0; anims && t < num_travelers; t++) {
            if (anims[t].phase == PHASE_WAITING &&
                anims[t].waiting_for_node == i) {
                node_locked = true;
                break;
            }
        }
        if (node_locked)
            DrawCircleLines((int)pos[i].x, (int)pos[i].y,
                            NODE_RADIUS + 10, COL_WAITING);

        DrawCircle((int)pos[i].x,(int)pos[i].y, NODE_RADIUS+5, Fade(border,0.3f));
        DrawCircle((int)pos[i].x,(int)pos[i].y, NODE_RADIUS,   COL_NODE);
        DrawCircleLines((int)pos[i].x,(int)pos[i].y, NODE_RADIUS, border);

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
        if (anims[t].phase == PHASE_WAITING) tc = COL_WAITING;
        DrawCircle(lx, ly+t*24, 7, tc);
        char leg[64];
        const char *status = "";
        if (anims[t].phase == PHASE_ARRIVED)      status = " [arrived]";
        else if (anims[t].phase == PHASE_WAITING) status = " [waiting]";
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

static void draw_train(const TrainAnim *a, int offset_index,
                       const Vec2 *pos) {
    if (a->phase == PHASE_ARRIVED) return;

    Color tc   = TRAVELER_COLORS[a->color_index % MAX_TRAVELERS];
    Color glow = tc; glow.a = 60;
    float cx, cy;

    if (a->phase == PHASE_WAITING) {
        tc      = COL_WAITING;
        glow    = tc; glow.a = 60;
        float angle = (float)offset_index * (2.0f * 3.14159265f / MAX_TRAVELERS);
        float r     = (float)(NODE_RADIUS + 20);
        cx = pos[a->waiting_for_node].x + r * cosf(angle);
        cy = pos[a->waiting_for_node].y + r * sinf(angle);
    } else {
        float ox = (float)(offset_index % 3) * 6.0f - 6.0f;
        float oy = (float)(offset_index / 3) * 6.0f - 3.0f;
        cx = a->train_x + ox;
        cy = a->train_y + oy;
    }

    DrawCircle((int)cx,(int)cy, NODE_RADIUS-4,  glow);
    DrawCircle((int)cx,(int)cy, NODE_RADIUS-10, tc);
    DrawCircleLines((int)cx,(int)cy, NODE_RADIUS-10, WHITE);

    if (a->phase == PHASE_WAITING) {
        DrawText("W", (int)(cx-5), (int)(cy-7), 13, WHITE);
    } else {
        static const char *LABELS[MAX_TRAVELERS] = {
            "T1","T2","T3","T4","T5","T6","T7","T8",
            "T9","T10","T11","T12","T13","T14","T15","T16"
        };
        const char *lbl = LABELS[a->color_index % MAX_TRAVELERS];
        int lw = MeasureText(lbl, 13);
        DrawText(lbl, (int)(cx-lw/2), (int)(cy-7), 13, WHITE);
    }
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

/* ── Animation update ────────────────────────────────────────── */

static void anim_update(TrainAnim *a, const Graph *g,
                        const Vec2 *pos, double delta_ms, int paused)
{
    if (paused || a->phase == PHASE_ARRIVED || a->phase == PHASE_WAITING)
        return;

    if (a->phase == PHASE_IDLE) {
        if (a->path == NULL) return;
        a->timer_ms += delta_ms;
        if (a->timer_ms >= MS_STATION_WAIT) {
            a->timer_ms = 0.0;
            if (a->seg + 1 < a->path_len)
                a->phase = PHASE_TRAVELLING;
        }
        return;
    }

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
            a->seg++;
            a->cur_node = a->path[a->seg];
            if (a->seg + 1 < a->path_len) {
                a->next_node = a->path[a->seg + 1];
                a->phase     = PHASE_IDLE;
            } else {
                a->next_node = -1;
                a->phase     = PHASE_ARRIVED;
            }
        } else {
            a->phase = PHASE_IDLE;
        }
    }
}

/* ── IPC polling (M5/M6/M7) ─────────────────────────────────── */

/*
 * poll_pipe_m7: reads one IpcMsg and handles M7 scheduling.
 *   MSG_WAITING : parent decides admit or queue via sched_request().
 *   MSG_AT_NODE : update animation.
 *   MSG_LEAVING : parent calls sched_release() to wake next waiter.
 */
/*
 * poll_pipe_m7: reads one IpcMsg from a child's pipe and handles
 * M7 parent-side scheduling.
 *
 * Parameters:
 *   a            – animation state for this traveler
 *   traveler_idx – index in the parent's traveler arrays
 *   fd           – child->parent read fd (non-blocking)
 *   pos          – node screen positions
 *   paused       – whether the simulation is paused
 *   queues       – per-node scheduling queues
 *   admit_fds    – parent->child admit pipe write-ends
 *   algo         – scheduling algorithm
 *   seq          – monotonic sequence counter for FCFS ordering
 *   all_anims    – full anim array (to look up PIDs of admitted travelers)
 */
static void poll_pipe_m7(TrainAnim *a, int traveler_idx, int fd,
                          const Vec2 *pos, int paused,
                          NodeQueue *queues, int *admit_fds,
                          SchedAlgo algo, long *seq,
                          TrainAnim *all_anims)
{
    if (fd < 0 || a->phase == PHASE_TRAVELLING) return;

    IpcMsg msg;
    ssize_t n = read(fd, &msg, sizeof(IpcMsg));
    if (n != (ssize_t)sizeof(IpcMsg)) return;

    /* Once ARRIVED, only MSG_LEAVING is meaningful (releases the node
     * for waiting travelers). Ignore any other stale messages. */
    if (a->phase == PHASE_ARRIVED && msg.type != MSG_LEAVING) return;

    switch (msg.type) {

    case MSG_WAITING: {
        a->waiting_for_node = msg.current_node;
        a->phase            = PHASE_WAITING;
        int admitted = sched_request(queues, msg.current_node,
                                     traveler_idx, msg.remaining_hops, seq);
        if (admitted) {
            /* Node was free — admit immediately */
            char sig = ADMIT_SIGNAL;
            (void)write(admit_fds[traveler_idx], &sig, 1);
            printf("[PID=%d] admitted to node %d (%s, immediate)\n",
                   (int)a->child_pid, msg.current_node,
                   sched_algo_name(algo));
        } else {
            printf("[PID=%d] waiting outside node %d (%s)\n",
                   (int)a->child_pid, msg.current_node,
                   sched_algo_name(algo));
        }
        fflush(stdout);
        break;
    }

    case MSG_AT_NODE:
        a->cur_node         = msg.current_node;
        a->next_node        = msg.next_node;
        a->waiting_for_node = -1;
        if (msg.next_node == -1) {
            printf("[PID=%d] arrived at node %d | DESTINATION\n",
                   (int)a->child_pid, msg.current_node);
            a->train_x = pos[msg.current_node].x;
            a->train_y = pos[msg.current_node].y;
            a->phase   = PHASE_ARRIVED;
        } else {
            printf("[PID=%d] arrived at node %d | next node: %d\n",
                   (int)a->child_pid, msg.current_node, msg.next_node);
            if (!paused) {
                a->timer_ms = 0.0;
                a->phase    = PHASE_TRAVELLING;
            } else {
                a->train_x = pos[msg.current_node].x;
                a->train_y = pos[msg.current_node].y;
                a->phase   = PHASE_IDLE;
            }
        }
        fflush(stdout);
        break;

    case MSG_LEAVING: {
        /* Release the node and admit the next waiter (if any) */
        int next_tidx = sched_release(queues, msg.current_node,
                                      algo, admit_fds);
        if (next_tidx >= 0)
            printf("[PID=%d] admitted to node %d (%s, scheduled)\n",
                   (int)all_anims[next_tidx].child_pid, msg.current_node,
                   sched_algo_name(algo));
        fflush(stdout);
        break;
    }

    case MSG_NO_PATH:
        printf("[PID=%d] ERROR: no path from node %d to destination\n",
               (int)a->child_pid, msg.current_node);
        fflush(stdout);
        a->phase = PHASE_ARRIVED;
        break;
    }
}

/* Original poll_pipe for M5/M6 (no scheduling, no admit_fds) */
static void poll_pipe(TrainAnim *a, int fd, const Vec2 *pos, int paused) {
    if (fd < 0 || a->phase == PHASE_TRAVELLING) return;

    IpcMsg msg;
    ssize_t n = read(fd, &msg, sizeof(IpcMsg));
    if (n != (ssize_t)sizeof(IpcMsg)) return;

    if (msg.type == MSG_NO_PATH) {
        printf("[PID=%d] ERROR: no path from node %d to destination\n",
               (int)a->child_pid, msg.current_node);
        fflush(stdout);
        a->phase = PHASE_ARRIVED;  /* remove traveler from animation */
        return;
    }

    if (msg.type == MSG_WAITING) {
        printf("[PID=%d] waiting outside node %d\n",
               (int)a->child_pid, msg.current_node);
        fflush(stdout);
        a->waiting_for_node = msg.current_node;
        a->phase            = PHASE_WAITING;
        return;
    }

    if (msg.type == MSG_LEAVING) return;  /* M6 doesn't use MSG_LEAVING */

    /* MSG_AT_NODE */
    if (msg.next_node == -1)
        printf("[PID=%d] arrived at node %d | DESTINATION\n",
               (int)a->child_pid, msg.current_node);
    else
        printf("[PID=%d] arrived at node %d | next node: %d\n",
               (int)a->child_pid, msg.current_node, msg.next_node);
    fflush(stdout);

    a->cur_node         = msg.current_node;
    a->next_node        = msg.next_node;
    a->waiting_for_node = -1;

    if (msg.next_node == -1) {
        a->train_x = pos[msg.current_node].x;
        a->train_y = pos[msg.current_node].y;
        a->phase   = PHASE_ARRIVED;
    } else if (!paused) {
        a->timer_ms = 0.0;
        a->phase    = PHASE_TRAVELLING;
    } else {
        a->train_x = pos[msg.current_node].x;
        a->train_y = pos[msg.current_node].y;
        a->phase   = PHASE_IDLE;
    }
}

/* ── Main window loop ────────────────────────────────────────── */

void renderer_run(const Graph  *g,
                  const char  **station_names,
                  TrainAnim    *anims,
                  int           num_travelers,
                  int          *read_fds,
                  int          *go_fds,
                  int          *admit_fds,
                  int           animate,
                  SchedAlgo     algo)
{
    Vec2 positions[MAX_NODES];
    renderer_compute_positions(g->num_vertices, positions);

    /* Make all pipe read-ends non-blocking */
    for (int t = 0; t < num_travelers; t++) {
        if (read_fds[t] >= 0) {
            int flags = fcntl(read_fds[t], F_GETFL, 0);
            fcntl(read_fds[t], F_SETFL, flags | O_NONBLOCK);
        }
    }

    /* Snap trains to source nodes */
    for (int t = 0; t < num_travelers; t++) {
        int s = anims[t].src_id;
        if (s >= 0 && s < g->num_vertices) {
            anims[t].train_x = positions[s].x;
            anims[t].train_y = positions[s].y;
        }
        anims[t].waiting_for_node = -1;
    }

    /* M7: parent-side scheduling state */
    NodeQueue *queues = NULL;
    long seq = 0;
    if (admit_fds != NULL) {
        queues = calloc((size_t)g->num_vertices, sizeof(NodeQueue));
        if (queues) sched_init(queues, g->num_vertices);
    }

    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
               "TrainOS - Railway Traffic Simulation");
    SetTargetFPS(60);

    int global_paused = 1;

    while (!WindowShouldClose()) {
        double delta_ms = GetFrameTime() * 1000.0;

        if (button_clicked()) {
            global_paused = !global_paused;
            if (!global_paused) {
                if (go_fds) {
                    for (int t = 0; t < num_travelers; t++) {
                        if (go_fds[t] > 0) {
                            char go = GO_SIGNAL;
                            (void)write(go_fds[t], &go, 1);
                            close(go_fds[t]);
                            go_fds[t] = -1;
                        }
                    }
                } else {
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

        if (animate) {
            for (int t = 0; t < num_travelers; t++) {
                if (admit_fds != NULL && queues != NULL) {
                    /* M7: always poll even if ARRIVED — the destination node
                     * sends MSG_LEAVING after its 1-second stay, and that
                     * message must be consumed so the parent can release the
                     * node and unblock any travelers waiting outside it. */
                    poll_pipe_m7(&anims[t], t, read_fds[t], positions,
                                 global_paused, queues, admit_fds, algo,
                                 &seq, anims);
                } else {
                    if (anims[t].phase == PHASE_ARRIVED) continue;
                    poll_pipe(&anims[t], read_fds[t], positions, global_paused);
                }
            }
            for (int t = 0; t < num_travelers; t++)
                anim_update(&anims[t], g, positions, delta_ms, global_paused);
        }

        BeginDrawing();
        renderer_draw_graph(g, positions, station_names, anims,
                            num_travelers, animate, algo);

        int any_active = 0;
        for (int t = 0; t < num_travelers; t++)
            if (anims[t].phase != PHASE_ARRIVED) { any_active = 1; break; }
        if (animate && any_active) draw_play_button(global_paused);

        if (animate) {
            for (int t = 0; t < num_travelers; t++)
                draw_train(&anims[t], t, positions);
        }

        int slot = 0;
        for (int t = 0; t < num_travelers; t++)
            if (anims[t].phase == PHASE_ARRIVED)
                draw_arrived_overlay(&anims[t], slot++);

        EndDrawing();

        int all_done = 1;
        for (int t = 0; t < num_travelers; t++)
            if (anims[t].phase != PHASE_ARRIVED) { all_done = 0; break; }
        if (all_done) { WaitTime(2.0f); break; }
    }

    CloseWindow();
    if (queues) free(queues);

    for (int t = 0; t < num_travelers; t++)
        if (anims[t].child_pid > 0)
            kill(anims[t].child_pid, SIGTERM);
}