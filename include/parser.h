#ifndef PARSER_H
#define PARSER_H

#include "graph.h"

/* Load a graph from a text file.
   Format:
     Line 1  : N M   (vertices, edges)
     Lines 2..M+1 : src dst weight
     Last line: src dst   (Dijkstra query)
   Sets *src and *dst to the query pair.
   Returns a heap-allocated Graph, or exits on error. */
Graph *parser_load(const char *filename, int *src, int *dst);

#endif /* PARSER_H */
