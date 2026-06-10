#ifndef RENDERER_H
#define RENDERER_H

#include <sys/types.h>   /* pid_t */
#include "graph.h"
#include "ipc.h"         /* MAX_TRAVELERS, MS_PER_JUMP, IpcMsg */

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
    PHASE_WAITING,     /* blocked outside a node — waiting for the lock (M6)   */
    PHASE_ARRIVED      /* reached the final destination                        */
} TrainPhase;

/*
 * Per-traveler animation state.
 *
 * waiting_for_node: set when phase == PHASE_WAITING.
 *   The train icon is drawn near the destination node but outside it,
 *   with a distinct colour, until the lock is acquired and MSG_AT_NODE
 *   arrives to transition the phase to PHASE_IDLE / PHASE_TRAVELLING.
 */
typedef struct {
    pid_t child_pid;    /* PID of the child process              */
    int   color_index;  /* index into TRAVELER_COLORS palette    */
    int   src_id;       /* starting node (for legend / overlay)  */
    int   dst_id;       /* destination node                      */

    int   cur_node;     /* node the traveler just arrived at     */
    int   next_node;    /* next node (-1 = at destination)       */

    /* M6: node this traveler is currently blocked outside of.
       Valid only when phase == PHASE_WAITING. */
    int   waiting_for_node;

    float train_x;      /* current screen position               */
    float train_y;

    double     timer_ms;
    TrainPhase phase;

    /* Optional full path for M2/M3/M4 (no IPC — renderer drives animation).
       NULL in M5/M6 (IPC messages drive animation instead). */
    const int *path;      /* array of node IDs src->dst */
    int        path_len;  /* total nodes in path        */
    int        seg;       /* current segment index      */
} TrainAnim;

/* Compute circular screen positions for all nodes */
void renderer_compute_positions(int num_nodes, Vec2 *positions);

/* Draw the full graph with all current traveler positions */
/* animate=0: highlight full path statically (M2)
   animate=1: highlight only current segment (M3/M4/M5/M6) */
void renderer_draw_graph(const Graph     *g,
                         const Vec2      *positions,
                         const char     **station_names,
                         const TrainAnim *anim_list,
                         int              num_travelers,
                         int              animate);

/*
 * Open the raylib window and run the render + animation loop.
 *
 * read_fds[i] is the read-end of the pipe from child i.
 * Each frame the parent reads all pending IpcMsg structs non-blocking,
 * prints the log line, and updates the animation state.
 * Blocks until the user closes the window or all children finish.
 */
/* animate=0: static display only (M2); animate=1: train movement (M3/M4/M5/M6) */
void renderer_run(const Graph  *g,
                  const char  **station_names,
                  TrainAnim    *anim_list,
                  int           num_travelers,
                  int          *read_fds,
                  int          *go_fds,
                  int           animate);

#endif /* RENDERER_H */