#ifndef DIJKSTRA_H
#define DIJKSTRA_H

#include "graph.h"

/* Result of a Dijkstra query */
typedef struct {
    int *path;       /* Array of vertex IDs from src to dst */
    int  path_len;   /* Number of vertices in the path */
    int  total_cost; /* Sum of edge weights along the path */
    int  found;      /* 1 if a path exists, 0 otherwise */
} DijkstraResult;

/* Run Dijkstra from src to dst on graph g.
   Caller must free the result with dijkstra_free_result(). */
DijkstraResult dijkstra_run(const Graph *g, int src, int dst);

/* Print the result in the format required by the professor */
void dijkstra_print_result(const DijkstraResult *res, int src, int dst);

/* Free memory inside a DijkstraResult */
void dijkstra_free_result(DijkstraResult *res);

#endif /* DIJKSTRA_H */
