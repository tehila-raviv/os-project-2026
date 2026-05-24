#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "graph.h"
#include "parser.h"
#include "dijkstra.h"

#ifdef WITH_RAYLIB
#include "renderer.h"
#endif

/* Human-readable station names for nodes 0..N-1 */
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

/* ── Global state for signal handler ─────────────────────────
   The signal handler needs access to the child PID array so it
   can terminate all children before the parent exits.          */
static pid_t *g_child_pids    = NULL;
static int    g_num_travelers = 0;

/* Called on SIGINT (Ctrl+C) or SIGTERM sent to the parent.
   Sends SIGTERM to every child, then lets main() waitpid() them. */
static void parent_signal_handler(int sig) {
    (void)sig;
    if (g_child_pids) {
        for (int i = 0; i < g_num_travelers; i++) {
            if (g_child_pids[i] > 0)
                kill(g_child_pids[i], SIGTERM);
        }
    }
    /* Do NOT call exit() here — fall through so main() waitpid()s cleanly */
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <graph_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* ── Parse input file ── */
#ifdef MILESTONE1
    /* Milestone 1 binary: single traveler, old file format.
       Runs Dijkstra, prints result, and exits immediately. */
    {
        int src_m1, dst_m1;
        Graph *g_m1 = parser_load_single(argv[1], &src_m1, &dst_m1);
        DijkstraResult res_m1 = dijkstra_run(g_m1, src_m1, dst_m1);
        dijkstra_print_result(&res_m1, src_m1, dst_m1);
        dijkstra_free_result(&res_m1);
        graph_free(g_m1);
        return EXIT_SUCCESS;
    }
#endif

    /* ── Milestone 4+: multi-traveler path ── */
    TravelerQuery *travelers     = NULL;
    int            num_travelers = 0;
    Graph *g = parser_load(argv[1], &travelers, &num_travelers);

    /* ── Parent computes Dijkstra for every traveler ── */
    DijkstraResult *results = malloc(sizeof(DijkstraResult) *
                                     (size_t)num_travelers);
    if (!results) {
        fprintf(stderr, "Error: failed to allocate results array\n");
        free(travelers);
        graph_free(g);
        return EXIT_FAILURE;
    }

    for (int i = 0; i < num_travelers; i++) {
        results[i] = dijkstra_run(g, travelers[i].src, travelers[i].dst);
        printf("Traveler %d: ", i + 1);
        dijkstra_print_result(&results[i], travelers[i].src, travelers[i].dst);
    }

    /* ── Fork one child process per traveler ── */
    pid_t *child_pids = malloc(sizeof(pid_t) * (size_t)num_travelers);
    if (!child_pids) {
        fprintf(stderr, "Error: failed to allocate pid array\n");
        for (int i = 0; i < num_travelers; i++) dijkstra_free_result(&results[i]);
        free(results);
        free(travelers);
        graph_free(g);
        return EXIT_FAILURE;
    }

    /* Expose PID array to the signal handler before any fork() */
    g_child_pids    = child_pids;
    g_num_travelers = num_travelers;

    /* Register handler for Ctrl+C and external SIGTERM */
    struct sigaction sa;
    sa.sa_handler = parent_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    for (int i = 0; i < num_travelers; i++) {
        /* Mark slot invalid so the handler skips uninitialised entries */
        child_pids[i] = -1;

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            parent_signal_handler(SIGTERM);
            for (int j = 0; j < i; j++) waitpid(child_pids[j], NULL, 0);
            for (int j = 0; j < num_travelers; j++) dijkstra_free_result(&results[j]);
            free(results);
            free(child_pids);
            free(travelers);
            graph_free(g);
            return EXIT_FAILURE;
        }

        if (pid == 0) {
            /* ── Child process ──
               Restore default signal handling so SIGTERM actually kills it. */
            signal(SIGINT,  SIG_DFL);
            signal(SIGTERM, SIG_DFL);

            for (int j = 0; j < num_travelers; j++) dijkstra_free_result(&results[j]);
            free(results);
            free(child_pids);
            free(travelers);
            graph_free(g);

            printf("[%d] started\n", (int)getpid());
            fflush(stdout);

            while (1) {
                sleep(3600);   /* interrupted by SIGTERM -> child exits cleanly */
            }
            return EXIT_SUCCESS;   /* unreachable */
        }

        /* Parent records the child PID */
        child_pids[i] = pid;
    }

    /* ── Parent: run the raylib GUI ── */
    #ifdef WITH_RAYLIB
        renderer_run(g, STATION_NAMES, results, num_travelers, child_pids);
    #else
        (void)STATION_NAMES;
        printf("(No GUI: compile with -DWITH_RAYLIB to enable animation)\n");
    #endif

    /* Terminate children before waitpid() so the parent does not wait forever. */
    parent_signal_handler(SIGTERM);

    /* ── Parent: wait for every child (normal exit or Ctrl+C) ── */
    for (int i = 0; i < num_travelers; i++) {
        if (child_pids[i] > 0)
            waitpid(child_pids[i], NULL, 0);
    }

    /* ── Cleanup ── */
    for (int i = 0; i < num_travelers; i++) dijkstra_free_result(&results[i]);
    free(results);
    free(child_pids);
    free(travelers);
    graph_free(g);

    return EXIT_SUCCESS;
}