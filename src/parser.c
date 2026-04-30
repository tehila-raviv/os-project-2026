#include <stdio.h>
#include <stdlib.h>
#include "parser.h"

Graph *parser_load(const char *filename, int *src, int *dst) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: cannot open file '%s'\n", filename);
        exit(EXIT_FAILURE);
    }

    int n, m;
    if (fscanf(fp, "%d %d", &n, &m) != 2 || n <= 0 || m < 0) {
        fprintf(stderr, "Error: invalid graph header in '%s'\n", filename);
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    Graph *g = graph_create(n);

    for (int i = 0; i < m; i++) {
        int s, d, w;
        if (fscanf(fp, "%d %d %d", &s, &d, &w) != 3) {
            fprintf(stderr, "Error: malformed edge on line %d\n", i + 2);
            graph_free(g);
            fclose(fp);
            exit(EXIT_FAILURE);
        }

        /* Negative weights are not valid for Dijkstra */
        if (w < 0) {
            fprintf(stderr, "Error: negative weight (%d) on edge %d -> %d\n", w, s, d);
            graph_free(g);
            fclose(fp);
            exit(EXIT_FAILURE);
        }

        /* Validate vertex indices */
        if (s < 0 || s >= n || d < 0 || d >= n) {
            fprintf(stderr, "Error: vertex index out of range on edge %d -> %d\n", s, d);
            graph_free(g);
            fclose(fp);
            exit(EXIT_FAILURE);
        }

        graph_add_edge(g, s, d, w);
    }

    /* Read the Dijkstra query */
    if (fscanf(fp, "%d %d", src, dst) != 2) {
        fprintf(stderr, "Error: missing source/destination query\n");
        graph_free(g);
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    if (*src < 0 || *src >= n || *dst < 0 || *dst >= n) {
        fprintf(stderr, "Error: query vertices out of range\n");
        graph_free(g);
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    fclose(fp);
    return g;
}
