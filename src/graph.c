#include <stdio.h>
#include <stdlib.h>
#include "graph.h"

/* Allocate a graph with num_vertices vertices and empty adjacency lists */
Graph *graph_create(int num_vertices) {
    Graph *g = malloc(sizeof(Graph));
    if (!g) {
        fprintf(stderr, "Error: failed to allocate graph\n");
        exit(EXIT_FAILURE);
    }

    g->num_vertices = num_vertices;
    g->lists = calloc(num_vertices, sizeof(AdjList));
    if (!g->lists) {
        fprintf(stderr, "Error: failed to allocate adjacency lists\n");
        free(g);
        exit(EXIT_FAILURE);
    }

    return g;
}

/* Add a directed edge src -> dest with the given weight */
void graph_add_edge(Graph *g, int src, int dest, int weight) {
    EdgeNode *node = malloc(sizeof(EdgeNode));
    if (!node) {
        fprintf(stderr, "Error: failed to allocate edge node\n");
        exit(EXIT_FAILURE);
    }

    node->dest   = dest;
    node->weight = weight;
    node->next   = g->lists[src].head;
    g->lists[src].head = node;
}

/* Free all memory used by the graph */
void graph_free(Graph *g) {
    if (!g) return;

    for (int i = 0; i < g->num_vertices; i++) {
        EdgeNode *curr = g->lists[i].head;
        while (curr) {
            EdgeNode *tmp = curr;
            curr = curr->next;
            free(tmp);
        }
    }

    free(g->lists);
    free(g);
}

/* Print the adjacency list (useful for debugging) */
void graph_print(const Graph *g) {
    for (int i = 0; i < g->num_vertices; i++) {
        printf("  %d:", i);
        EdgeNode *curr = g->lists[i].head;
        while (curr) {
            printf(" -> %d(w:%d)", curr->dest, curr->weight);
            curr = curr->next;
        }
        printf("\n");
    }
}
