#ifndef RENDERER_H
#define RENDERER_H

#include "graph.h"
#include "dijkstra.h"

#define WINDOW_WIDTH  1200
#define WINDOW_HEIGHT 800
#define NODE_RADIUS   28
#define MAX_NODES     15

/* Milliseconds per one "jump" along an edge (weight unit) */
#define MS_PER_JUMP     300
/* Milliseconds the train waits at each intermediate station */
#define MS_STATION_WAIT 1000

/* 2D position for a graph node on screen */
typedef struct {
    float x;
    float y;
} Vec2;

/* Animation phases for the train */
typedef enum {
    PHASE_IDLE,       /* waiting at an intermediate station */
    PHASE_TRAVELLING, /* moving along an edge */
    PHASE_ARRIVED     /* reached the final destination */
} TrainPhase;

/* All state needed to animate one train along a precomputed path */
typedef struct {
    const int *path;     /* vertex array from DijkstraResult */
    int        path_len; /* number of vertices in path */

    int   seg;           /* current segment index (path[seg] -> path[seg+1]) */
    int   seg_weight;    /* weight of the current edge */

    float train_x;       /* current screen position */
    float train_y;

    double     timer_ms; /* accumulated ms for the current phase */
    TrainPhase phase;
    int        paused;   /* 1 = paused, 0 = running */
} TrainAnim;

/* Compute screen positions for all nodes (circular layout) */
void renderer_compute_positions(int num_nodes, Vec2 *positions);

/* Draw the full graph with optional path highlighting.
   Pass res=NULL to draw without any path highlight. */
void renderer_draw_graph(const Graph *g, const Vec2 *positions,
                         const char **station_names,
                         const DijkstraResult *res,
                         int src_id, int dst_id);

/* Open window and run the render + animation loop until user closes */
void renderer_run(const Graph *g, const char **station_names,
                  const DijkstraResult *res, int src_id, int dst_id);

#endif /* RENDERER_H */