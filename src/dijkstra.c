#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "dijkstra.h"

/* Simple min-heap node */
typedef struct {
    int vertex;
    int dist;
} HeapNode;

/* Min-heap */
typedef struct {
    HeapNode *data;
    int size;
    int capacity;
} MinHeap;

/* ── heap helpers ─────────────────────────────────────── */

static MinHeap *heap_create(int capacity) {
    MinHeap *h = malloc(sizeof(MinHeap));
    h->data     = malloc(sizeof(HeapNode) * capacity);
    h->size     = 0;
    h->capacity = capacity;
    return h;
}

static void heap_free(MinHeap *h) {
    free(h->data);
    free(h);
}

static void heap_swap(HeapNode *a, HeapNode *b) {
    HeapNode tmp = *a;
    *a = *b;
    *b = tmp;
}

static void heap_push(MinHeap *h, int vertex, int dist) {
    if (h->size == h->capacity) {
        h->capacity *= 2;
        h->data = realloc(h->data, sizeof(HeapNode) * h->capacity);
    }
    int i = h->size++;
    h->data[i].vertex = vertex;
    h->data[i].dist   = dist;

    /* Bubble up */
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (h->data[parent].dist > h->data[i].dist) {
            heap_swap(&h->data[parent], &h->data[i]);
            i = parent;
        } else {
            break;
        }
    }
}

static HeapNode heap_pop(MinHeap *h) {
    HeapNode top = h->data[0];
    h->data[0]   = h->data[--h->size];

    /* Bubble down */
    int i = 0;
    while (1) {
        int left  = 2 * i + 1;
        int right = 2 * i + 2;
        int smallest = i;

        if (left  < h->size && h->data[left].dist  < h->data[smallest].dist) smallest = left;
        if (right < h->size && h->data[right].dist < h->data[smallest].dist) smallest = right;

        if (smallest == i) break;
        heap_swap(&h->data[i], &h->data[smallest]);
        i = smallest;
    }
    return top;
}

/* ── Dijkstra ─────────────────────────────────────────── */

DijkstraResult dijkstra_run(const Graph *g, int src, int dst) {
    int n = g->num_vertices;

    int *dist = malloc(sizeof(int) * n);
    int *prev = malloc(sizeof(int) * n);

    for (int i = 0; i < n; i++) {
        dist[i] = INT_MAX;
        prev[i] = -1;
    }
    dist[src] = 0;

    MinHeap *heap = heap_create(n + 1);
    heap_push(heap, src, 0);

    while (heap->size > 0) {
        HeapNode curr = heap_pop(heap);
        int u = curr.vertex;

        /* Skip stale entries */
        if (curr.dist > dist[u]) continue;

        /* Early exit once destination is settled */
        if (u == dst) break;

        EdgeNode *edge = g->lists[u].head;
        while (edge) {
            int v = edge->dest;
            int new_dist = dist[u] + edge->weight;
            if (new_dist < dist[v]) {
                dist[v] = new_dist;
                prev[v] = u;
                heap_push(heap, v, new_dist);
            }
            edge = edge->next;
        }
    }

    heap_free(heap);

    DijkstraResult res;
    res.found      = 0;
    res.path       = NULL;
    res.path_len   = 0;
    res.total_cost = 0;

    if (dist[dst] == INT_MAX) {
        free(dist);
        free(prev);
        return res;
    }

    /* Reconstruct path by tracing prev[] backwards */
    int count = 0;
    for (int v = dst; v != -1; v = prev[v]) count++;

    res.path     = malloc(sizeof(int) * count);
    res.path_len = count;
    res.total_cost = dist[dst];
    res.found    = 1;

    int idx = count - 1;
    for (int v = dst; v != -1; v = prev[v]) {
        res.path[idx--] = v;
    }

    free(dist);
    free(prev);
    return res;
}

/* Print exactly the format the professor requires */
void dijkstra_print_result(const DijkstraResult *res, int src, int dst) {
    /* Case: source == destination */
    if (src == dst) {
        printf("%d\n0\n", src);
        return;
    }

    /* Case: no path */
    if (!res->found) {
        printf("No path found\n");
        return;
    }

    /* Case: path found */
    for (int i = 0; i < res->path_len; i++) {
        if (i > 0) printf(" -> ");
        printf("%d", res->path[i]);
    }
    printf("\n%d\n", res->total_cost);
}

void dijkstra_free_result(DijkstraResult *res) {
    if (res && res->path) {
        free(res->path);
        res->path = NULL;
    }
}
