#include <stdio.h>
#include <stdlib.h>
#include "graph.h"
#include "parser.h"
#include "dijkstra.h"

#ifdef WITH_RAYLIB
#include "renderer.h"
#endif

/* Human-readable station names for nodes 0..N-1.
   Extend or rename to match your input file. */
static const char *STATION_NAMES[] = {
    "Central",   /* 0 */
    "North",     /* 1 */
    "East",      /* 2 */
    "South",     /* 3 */
    "West",      /* 4 */
    "Airport",   /* 5 */
    "Harbor",    /* 6 */
    "Uptown",    /* 7 */
    "Downtown",  /* 8 */
    "Riverside", /* 9 */
    "Hillside",  /* 10 */
    "Lakeside",  /* 11 */
    "Westgate",  /* 12 */
    "Eastgate",  /* 13 */
    "Junction",  /* 14 */
};

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <graph_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int src, dst;
    Graph *g = parser_load(argv[1], &src, &dst);

    /* Always print Dijkstra result to terminal */
    DijkstraResult res = dijkstra_run(g, src, dst);
    dijkstra_print_result(&res, src, dst);

#ifdef WITH_RAYLIB
    /* Open GUI window with path highlighted */
    renderer_run(g, STATION_NAMES, &res, src, dst);
#endif

    dijkstra_free_result(&res);
    graph_free(g);
    return EXIT_SUCCESS;
}