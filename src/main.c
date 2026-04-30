#include <stdio.h>
#include <stdlib.h>
#include "graph.h"
#include "parser.h"
#include "dijkstra.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <graph_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int src, dst;
    Graph *g = parser_load(argv[1], &src, &dst);

    DijkstraResult res = dijkstra_run(g, src, dst);
    dijkstra_print_result(&res, src, dst);

    dijkstra_free_result(&res);
    graph_free(g);

    return EXIT_SUCCESS;
}
