#ifndef RENDERER_H
#define RENDERER_H

#include "graph.h"
#include "dijkstra.h"

#define WINDOW_WIDTH  1200
#define WINDOW_HEIGHT 800
#define NODE_RADIUS   28
#define MAX_NODES     15

/* 2D position for a graph node on screen */
typedef struct {
    float x;
    float y;
} Vec2;

/* Compute screen positions for all nodes (circular layout) */
void renderer_compute_positions(int num_nodes, Vec2 *positions);

/* Draw the full graph with optional path highlighting.
   Pass res=NULL to draw without any path highlight. */
void renderer_draw_graph(const Graph *g, const Vec2 *positions,
                         const char **station_names,
                         const DijkstraResult *res,
                         int src_id, int dst_id);

/* Open window and run the render loop until user closes */
void renderer_run(const Graph *g, const char **station_names,
                  const DijkstraResult *res, int src_id, int dst_id);

#endif /* RENDERER_H */