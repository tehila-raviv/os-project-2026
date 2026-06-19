#ifndef RENDERER_H
#define RENDERER_H

#include <sys/types.h>   /* pid_t */
#include "graph.h"
#include "ipc.h"         /* MAX_TRAVELERS, MS_PER_JUMP, IpcMsg, SchedAlgo */

/* ── Window / layout constants ─────────────────────────────── */
#define WINDOW_WIDTH    1200
#define WINDOW_HEIGHT    800
#define NODE_RADIUS       28
#define MAX_NODES         15
#define MS_STATION_WAIT 1000   /* ms pause at intermediate stations */

/* 2D screen position for a graph node */
typedef struct {
    float x;
    float y;
} Vec2;

/* Animation phase for a single traveler */
typedef enum {
    PHASE_IDLE,        /* waiting at an intermediate station (holds node lock) */
    PHASE_TRAVELLING,  /* moving along an edge                                 */
    PHASE_WAITING,     /* blocked outside a node — waiting for the lock        */
    PHASE_ARRIVED      /* reached the final destination                        */
} TrainPhase;

/*
 * Per-traveler animation state.
 *
 * waiting_for_node: set when phase == PHASE_WAITING.
 */
typedef struct {
    pid_t child_pid;    /* PID of the child process              */
    int   color_index;  /* index into TRAVELER_COLORS palette    */
    int   src_id;       /* starting node (for legend / overlay)  */
    int   dst_id;       /* destination node                      */

    int   cur_node;     /* node the traveler just arrived at     */
    int   next_node;    /* next node (-1 = at destination)       */

    /* node this traveler is currently blocked outside of.
       Valid only when phase == PHASE_WAITING. */
    int   waiting_for_node;

    float train_x;      /* current screen position               */
    float train_y;

    double     timer_ms;
    TrainPhase phase;

    /* Optional full path for M2/M3/M4 (no IPC).
       NULL in M5/M6/M7 (IPC messages drive animation instead). */
    const int *path;
    int        path_len;
    int        seg;
} TrainAnim;

/* Compute circular screen positions for all nodes */
void renderer_compute_positions(int num_nodes, Vec2 *positions);

/* Draw the full graph with all current traveler positions */
void renderer_draw_graph(const Graph     *g,
                         const Vec2      *positions,
                         const char     **station_names,
                         const TrainAnim *anim_list,
                         int              num_travelers,
                         int              animate,
                         SchedAlgo        algo);

/*
 * Open the raylib window and run the render + animation loop.
 *
 * read_fds[i]  — read-end of child->parent pipe (IpcMsg stream).
 * go_fds[i]    — write-end of parent->child GO_SIGNAL pipe.
 * admit_fds[i] — write-end of parent->child ADMIT_SIGNAL pipe (M7, NULL for M2-M6).
 * algo         — scheduling algorithm (used by parent event loop in M7).
 */
void renderer_run(const Graph  *g,
                  const char  **station_names,
                  TrainAnim    *anim_list,
                  int           num_travelers,
                  int          *read_fds,
                  int          *go_fds,
                  int          *admit_fds,
                  int           animate,
                  SchedAlgo     algo);

#endif /* RENDERER_H */