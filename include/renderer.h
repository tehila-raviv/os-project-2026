#ifndef RENDERER_H
#define RENDERER_H

#include <sys/types.h>   /* pid_t */
#include "graph.h"
#include "dijkstra.h"

/* ── Window / layout constants ─────────────────────────────── */
#define WINDOW_WIDTH   1200
#define WINDOW_HEIGHT   800
#define NODE_RADIUS      28
#define MAX_NODES        15
#define MAX_TRAVELERS    16   /* maximum number of simultaneous travelers */

/* Milliseconds per one weight unit of travel along an edge */
#define MS_PER_JUMP       300
/* Milliseconds the train waits at each intermediate station */
#define MS_STATION_WAIT  1000

/* 2D screen position for a graph node */
typedef struct {
    float x;
    float y;
} Vec2;

/* Animation phase for a single traveler */
typedef enum {
    PHASE_IDLE,       /* waiting at an intermediate station */
    PHASE_TRAVELLING, /* moving along an edge */
    PHASE_ARRIVED     /* reached the final destination */
} TrainPhase;

/* Per-traveler animation state */
typedef struct {
    /* Path data (points into a DijkstraResult owned by the caller) */
    const int *path;
    int        path_len;

    int   seg;           /* current segment index: path[seg]->path[seg+1] */
    int   seg_weight;    /* weight of the current edge */

    float train_x;       /* current screen position */
    float train_y;

    double     timer_ms; /* accumulated ms for the current phase */
    TrainPhase phase;
    int        paused;   /* 1 = paused, 0 = running */

    /* Child process ID associated with this traveler (set by parent) */
    pid_t child_pid;

    /* Index into the traveler palette (determines train color) */
    int color_index;

    /* Source / destination node IDs for the overlay */
    int src_id;
    int dst_id;
} TrainAnim;

/* Compute circular screen positions for all nodes */
void renderer_compute_positions(int num_nodes, Vec2 *positions);

/* Draw the graph (edges + nodes).  Pass NULL for res / anim_list to draw
   without any path or traveler highlighting. */
void renderer_draw_graph(const Graph          *g,
                         const Vec2           *positions,
                         const char          **station_names,
                         const DijkstraResult *results,   /* array, one per traveler */
                         const TrainAnim      *anim_list, /* array, one per traveler */
                         int                   num_travelers);

/* Open the raylib window and run the render + animation loop.
   Blocks until the user closes the window or all travelers arrive.
   child_pids[i] is sent SIGTERM when traveler i reaches its destination. */
void renderer_run(const Graph          *g,
                  const char          **station_names,
                  const DijkstraResult *results,
                  int                   num_travelers,
                  pid_t                *child_pids);

#endif /* RENDERER_H */