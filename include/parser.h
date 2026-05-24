#ifndef PARSER_H
#define PARSER_H

#include "graph.h"

/* One traveler query: source and destination node IDs */
typedef struct {
    int src;
    int dst;
} TravelerQuery;

/* Load a graph and traveler list from an extended input file.
   Format:
     [optional comment lines starting with #]
     N M              (vertices, edges)
     src dst weight   (M directed edges)
     ...
     # travelers
     K                (number of travelers)
     src dst          (K traveler queries)
     ...

   On success, returns a heap-allocated Graph and writes:
     *travelers   -> heap-allocated array of TravelerQuery (caller must free)
     *num_travelers -> number of entries in *travelers

   Exits on any parse or validation error. */
Graph *parser_load(const char *filename,
                   TravelerQuery **travelers,
                   int *num_travelers);

/* Legacy single-traveler loader kept for Milestone 1 compatibility.
   (Not used from Milestone 4 onward.) */
Graph *parser_load_single(const char *filename, int *src, int *dst);

#endif /* PARSER_H */

/* Legacy single-traveler loader for the Milestone 1 terminal binary.
   Used when compiled with -DMILESTONE1. */
Graph *parser_load_single(const char *filename, int *src, int *dst);