#ifndef GRAPH_H
#define GRAPH_H

/* Adjacency list node representing a directed edge */
typedef struct EdgeNode {
    int dest;
    int weight;
    struct EdgeNode *next;
} EdgeNode;

/* One entry in the adjacency list (one per vertex) */
typedef struct {
    EdgeNode *head;
} AdjList;

/* Directed weighted graph */
typedef struct {
    int num_vertices;
    AdjList *lists;
} Graph;

Graph   *graph_create(int num_vertices);
void     graph_add_edge(Graph *g, int src, int dest, int weight);
void     graph_free(Graph *g);
void     graph_print(const Graph *g);

#endif /* GRAPH_H */
