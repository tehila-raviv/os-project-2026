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

static const char *STATION_NAMES[] = {
    "Central", "North",    "East",     "South",    "West",
    "Airport", "Harbor",   "Uptown",   "Downtown", "Riverside",
    "Hillside","Lakeside", "Westgate", "Eastgate", "Junction",
};

/* ── Signal handling (M4 only) ───────────────────────────────── */
#if !defined(MILESTONE1) && !defined(MILESTONE2) && !defined(MILESTONE3)
static pid_t *g_child_pids    = NULL;
static int    g_num_travelers = 0;

static void parent_signal_handler(int sig) {
    (void)sig;
    if (g_child_pids)
        for (int i = 0; i < g_num_travelers; i++)
            if (g_child_pids[i] > 0)
                kill(g_child_pids[i], SIGTERM);
}
#endif

/* ════════════════════════════════════════════════════════════════
 * MILESTONE 1 — terminal only, single traveler, old format
 * ════════════════════════════════════════════════════════════════ */
#ifdef MILESTONE1
int main(int argc, char *argv[]) {
    (void)STATION_NAMES;
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <graph_file>\n", argv[0]);
        return EXIT_FAILURE;
    }
    int src, dst;
    Graph *g = parser_load_single(argv[1], &src, &dst);
    DijkstraResult res = dijkstra_run(g, src, dst);
    dijkstra_print_result(&res, src, dst);
    dijkstra_free_result(&res);
    graph_free(g);
    return EXIT_SUCCESS;
}

/* ════════════════════════════════════════════════════════════════
 * MILESTONE 2 — GUI static graph, single traveler
 * MILESTONE 3 — GUI + animation, single traveler
 * ════════════════════════════════════════════════════════════════ */
#elif defined(MILESTONE2) || defined(MILESTONE3)
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <graph_file>\n", argv[0]);
        return EXIT_FAILURE;
    }
    int src, dst;
    Graph *g = parser_load_single(argv[1], &src, &dst);
    DijkstraResult res = dijkstra_run(g, src, dst);
    dijkstra_print_result(&res, src, dst);

#ifdef WITH_RAYLIB
    renderer_run(g, STATION_NAMES, &res, 1, NULL);
#else
    (void)STATION_NAMES;
#endif

    dijkstra_free_result(&res);
    graph_free(g);
    return EXIT_SUCCESS;
}

/* ════════════════════════════════════════════════════════════════
 * MILESTONE 4 — multi-process, parent computes paths, children sleep
 * ════════════════════════════════════════════════════════════════ */
#else  /* MILESTONE4 (default) */
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <graph_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    TravelerQuery *travelers     = NULL;
    int            num_travelers = 0;
    Graph *g = parser_load(argv[1], &travelers, &num_travelers);

    /* Parent computes all paths */
    DijkstraResult *results = malloc(sizeof(DijkstraResult) *
                                     (size_t)num_travelers);
    if (!results) {
        fprintf(stderr, "Error: allocation failed\n");
        free(travelers); graph_free(g); return EXIT_FAILURE;
    }
    for (int i = 0; i < num_travelers; i++) {
        results[i] = dijkstra_run(g, travelers[i].src, travelers[i].dst);
        printf("Traveler %d: ", i + 1);
        dijkstra_print_result(&results[i], travelers[i].src, travelers[i].dst);
    }

    pid_t *child_pids = malloc(sizeof(pid_t) * (size_t)num_travelers);
    if (!child_pids) {
        fprintf(stderr, "Error: allocation failed\n");
        for (int i = 0; i < num_travelers; i++) dijkstra_free_result(&results[i]);
        free(results); free(travelers); graph_free(g); return EXIT_FAILURE;
    }

    g_child_pids    = child_pids;
    g_num_travelers = num_travelers;

    struct sigaction sa;
    sa.sa_handler = parent_signal_handler;
    sigemptyset(&sa.sa_mask); sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL); sigaction(SIGTERM, &sa, NULL);

    for (int i = 0; i < num_travelers; i++) {
        child_pids[i] = -1;
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            parent_signal_handler(SIGTERM);
            for (int j = 0; j < i; j++) waitpid(child_pids[j], NULL, 0);
            for (int j = 0; j < num_travelers; j++) dijkstra_free_result(&results[j]);
            free(results); free(child_pids); free(travelers); graph_free(g);
            return EXIT_FAILURE;
        }
        if (pid == 0) {
            /* Child: restore default signals, print started, sleep until SIGTERM */
            signal(SIGINT, SIG_DFL); signal(SIGTERM, SIG_DFL);
            for (int j = 0; j < num_travelers; j++) dijkstra_free_result(&results[j]);
            free(results); free(child_pids); free(travelers); graph_free(g);
            printf("[%d] started\n", (int)getpid());
            fflush(stdout);
            while (1) sleep(3600);
            return EXIT_SUCCESS;
        }
        child_pids[i] = pid;
    }

#ifdef WITH_RAYLIB
    renderer_run(g, STATION_NAMES, results, num_travelers, child_pids);
#else
    (void)STATION_NAMES;
    printf("(No GUI: compile with -DWITH_RAYLIB to enable animation)\n");
    parent_signal_handler(SIGTERM);
#endif

    for (int i = 0; i < num_travelers; i++)
        if (child_pids[i] > 0) waitpid(child_pids[i], NULL, 0);

    for (int i = 0; i < num_travelers; i++) dijkstra_free_result(&results[i]);
    free(results); free(child_pids); free(travelers); graph_free(g);
    return EXIT_SUCCESS;
}
#endif  /* milestone selector */