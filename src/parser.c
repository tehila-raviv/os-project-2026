#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

/* Skip comment lines (starting with '#') and blank lines.
   Returns 0 on success, EOF if end of file reached. */
static int skip_comments(FILE *fp)
{
    int c;
    while ((c = fgetc(fp)) != EOF)
    {
        if (c == '#')
        {
            /* Consume the rest of the comment line */
            while ((c = fgetc(fp)) != EOF && c != '\n')
                ;
        }
        else if (c != '\n' && c != '\r' && c != ' ' && c != '\t')
        {
            /* Put back the first meaningful character */
            ungetc(c, fp);
            return 0;
        }
    }
    return EOF;
}

/* ── Multi-traveler loader (Milestone 4+) ──────────────────── */

Graph *parser_load(const char *filename,
                   TravelerQuery **travelers,
                   int *num_travelers)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        fprintf(stderr, "Error: cannot open file '%s'\n", filename);
        exit(EXIT_FAILURE);
    }

    /* Skip any leading comments */
    skip_comments(fp);

    /* Read graph header: N vertices, M edges */
    int n, m;
    if (fscanf(fp, "%d %d", &n, &m) != 2 || n <= 0 || m < 0)
    {
        fprintf(stderr, "Error: invalid graph header in '%s'\n", filename);
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    Graph *g = graph_create(n);

    /* Read M edges */
    for (int i = 0; i < m; i++)
    {
        int s, d, w;
        if (fscanf(fp, "%d %d %d", &s, &d, &w) != 3)
        {
            fprintf(stderr, "Error: malformed edge on line %d\n", i + 2);
            graph_free(g);
            fclose(fp);
            exit(EXIT_FAILURE);
        }
        if (w < 0)
        {
            printf("Error: negative weight (%d) on edge %d->%d\n",
                   w, s, d);
            graph_free(g);
            fclose(fp);
            exit(EXIT_FAILURE);
        }
        if (s < 0 || s >= n || d < 0 || d >= n)
        {
            fprintf(stderr, "Error: vertex out of range on edge %d->%d\n",
                    s, d);
            graph_free(g);
            fclose(fp);
            exit(EXIT_FAILURE);
        }
        graph_add_edge(g, s, d, w);
    }

    /* Skip whitespace / comment lines to reach the "# travelers" section */
    skip_comments(fp);

    /* Read number of travelers */
    int k;
    if (fscanf(fp, "%d", &k) != 1 || k <= 0)
    {
        fprintf(stderr, "Error: missing or invalid traveler count\n");
        graph_free(g);
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    TravelerQuery *tq = malloc(sizeof(TravelerQuery) * (size_t)k);
    if (!tq)
    {
        fprintf(stderr, "Error: failed to allocate traveler array\n");
        graph_free(g);
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    /* Read K traveler queries */
    for (int i = 0; i < k; i++)
    {
        if (fscanf(fp, "%d %d", &tq[i].src, &tq[i].dst) != 2)
        {
            fprintf(stderr, "Error: malformed traveler entry %d\n", i + 1);
            free(tq);
            graph_free(g);
            fclose(fp);
            exit(EXIT_FAILURE);
        }
        if (tq[i].src < 0 || tq[i].src >= n ||
            tq[i].dst < 0 || tq[i].dst >= n)
        {
            fprintf(stderr,
                    "Error: traveler %d has out-of-range node (%d -> %d)\n",
                    i + 1, tq[i].src, tq[i].dst);
            free(tq);
            graph_free(g);
            fclose(fp);
            exit(EXIT_FAILURE);
        }
    }

    fclose(fp);

    *travelers = tq;
    *num_travelers = k;
    return g;
}

/* ── Legacy single-traveler loader (Milestone 1 compatibility) ─ */

Graph *parser_load_single(const char *filename, int *src, int *dst)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        fprintf(stderr, "Error: cannot open file '%s'\n", filename);
        exit(EXIT_FAILURE);
    }

    int n, m;
    if (fscanf(fp, "%d %d", &n, &m) != 2 || n <= 0 || m < 0)
    {
        fprintf(stderr, "Error: invalid graph header in '%s'\n", filename);
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    Graph *g = graph_create(n);

    for (int i = 0; i < m; i++)
    {
        int s, d, w;
        if (fscanf(fp, "%d %d %d", &s, &d, &w) != 3)
        {
            fprintf(stderr, "Error: malformed edge on line %d\n", i + 2);
            graph_free(g);
            fclose(fp);
            exit(EXIT_FAILURE);
        }
        if (w < 0)
        {
            printf("Error: negative weight (%d) on edge %d->%d\n",
            w, s, d);
            graph_free(g);
            fclose(fp);
            exit(EXIT_FAILURE);
        }
        if (s < 0 || s >= n || d < 0 || d >= n)
        {
            fprintf(stderr, "Error: vertex out of range on edge %d->%d\n",
                    s, d);
            graph_free(g);
            fclose(fp);
            exit(EXIT_FAILURE);
        }
        graph_add_edge(g, s, d, w);
    }

    if (fscanf(fp, "%d %d", src, dst) != 2)
    {
        fprintf(stderr, "Error: missing source/destination query\n");
        graph_free(g);
        fclose(fp);
        exit(EXIT_FAILURE);
    }
    if (*src < 0 || *src >= n || *dst < 0 || *dst >= n)
    {
        fprintf(stderr, "Error: query vertices out of range\n");
        graph_free(g);
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    fclose(fp);
    return g;
}