#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <time.h>
#include <signal.h>
#include "graph.h"
#include "parser.h"
#include "dijkstra.h"
#include "ipc.h"

#ifdef WITH_RAYLIB
#include "renderer.h"
#endif

static const char *STATION_NAMES[] = {
    "Central", "North",    "East",     "South",    "West",
    "Airport", "Harbor",   "Uptown",   "Downtown", "Riverside",
    "Hillside","Lakeside", "Westgate", "Eastgate", "Junction",
};

/* ── Signal handling ──────────────────────────────────────────── */

/* Used by M4/M5 only */
__attribute__((unused)) static pid_t *g_child_pids    = NULL;
__attribute__((unused)) static int    g_num_travelers = 0;

/* Used by M4/M5 only */
__attribute__((unused))
static void parent_signal_handler(int sig) {
    (void)sig;
    if (g_child_pids)
        for (int i = 0; i < g_num_travelers; i++)
            if (g_child_pids[i] > 0)
                kill(g_child_pids[i], SIGTERM);
}

/* ════════════════════════════════════════════════════════════════
 * MILESTONE 1 — terminal only, single traveler, old format
 * ════════════════════════════════════════════════════════════════ */
#ifdef MILESTONE1
int main(int argc, char *argv[]) {
    (void)STATION_NAMES;   /* unused in M1 */
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
 * Both use parser_load_single and pass one DijkstraResult to renderer
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
    /* Build a single-traveler anim list for the renderer */
    TrainAnim anim;
    anim.child_pid   = -1;
    anim.color_index = 0;
    anim.src_id      = src;
    anim.dst_id      = dst;
    anim.cur_node    = src;
    anim.next_node   = (res.found && res.path_len > 1) ? res.path[1] : -1;
    anim.phase       = (!res.found || res.path_len <= 1) ? PHASE_ARRIVED : PHASE_IDLE;
    anim.train_x     = 0.0f;
    anim.train_y     = 0.0f;
    anim.timer_ms    = 0.0;
    /* Path-driven animation (no IPC in M2/M3) */
    anim.path     = res.found ? res.path : NULL;
    anim.path_len = res.found ? res.path_len : 0;
    anim.seg      = 0;

    /* For M2/M3 we reuse the M5 renderer but pass go_fds=NULL.
       The renderer will skip GO_SIGNAL logic and start immediately. */
    int dummy_read_fd = -1;
    #ifdef MILESTONE2
    renderer_run(g, STATION_NAMES, &anim, 1, &dummy_read_fd, NULL, 0);
#else
    renderer_run(g, STATION_NAMES, &anim, 1, &dummy_read_fd, NULL, 1);
#endif
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
#elif defined(MILESTONE4)
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

#ifdef WITH_RAYLIB
    TrainAnim *anims = calloc((size_t)num_travelers, sizeof(TrainAnim));
    if (!anims) {
        fprintf(stderr, "Error: allocation failed\n");
        free(child_pids);
        for (int i = 0; i < num_travelers; i++) dijkstra_free_result(&results[i]);
        free(results); free(travelers); graph_free(g); return EXIT_FAILURE;
    }
#endif

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
            signal(SIGINT, SIG_DFL); signal(SIGTERM, SIG_DFL);
            for (int j = 0; j < num_travelers; j++) dijkstra_free_result(&results[j]);
            free(results); free(child_pids);
#ifdef WITH_RAYLIB
            free(anims);
#endif
            free(travelers); graph_free(g);
            printf("[%d] started\n", (int)getpid());
            fflush(stdout);
            while (1) sleep(3600);
            return EXIT_SUCCESS;
        }
        child_pids[i] = pid;
#ifdef WITH_RAYLIB
        anims[i].child_pid   = pid;
        anims[i].color_index = i;
        anims[i].src_id      = travelers[i].src;
        anims[i].dst_id      = travelers[i].dst;
        anims[i].cur_node    = results[i].found ? results[i].path[0] : travelers[i].src;
        anims[i].next_node   = (results[i].found && results[i].path_len > 1)
                               ? results[i].path[1] : -1;
        anims[i].phase       = (!results[i].found || results[i].path_len <= 1)
                               ? PHASE_ARRIVED : PHASE_IDLE;
        anims[i].train_x     = 0.0f;
        anims[i].train_y     = 0.0f;
        anims[i].timer_ms    = 0.0;
        /* Path-driven animation (M4: parent drives, no IPC) */
        anims[i].path     = results[i].found ? results[i].path : NULL;
        anims[i].path_len = results[i].found ? results[i].path_len : 0;
        anims[i].seg      = 0;
#endif
    }

#ifdef WITH_RAYLIB
    /* M4 renderer: pass go_fds=NULL so renderer starts immediately on PLAY
       (no GO_SIGNAL needed — children just sleep, parent drives animation) */
    int *null_read_fds = calloc((size_t)num_travelers, sizeof(int));
    for (int i = 0; i < num_travelers; i++) null_read_fds[i] = -1;
    renderer_run(g, STATION_NAMES, anims, num_travelers, null_read_fds, NULL, 1);
    free(null_read_fds);
    free(anims);
#else
    (void)STATION_NAMES;
    parent_signal_handler(SIGTERM);
#endif

    for (int i = 0; i < num_travelers; i++)
        if (child_pids[i] > 0) waitpid(child_pids[i], NULL, 0);

    for (int i = 0; i < num_travelers; i++) dijkstra_free_result(&results[i]);
    free(results); free(child_pids); free(travelers); graph_free(g);
    return EXIT_SUCCESS;
}

/* ════════════════════════════════════════════════════════════════
 * MILESTONE 5 — multi-process + IPC pipes, children autonomous
 * ════════════════════════════════════════════════════════════════ */
#else   /* MILESTONE5 (default) */

static void child_main(const char *filename, int src, int dst,
                       int write_fd, int go_fd)
{
    signal(SIGINT,  SIG_DFL);
    signal(SIGTERM, SIG_DFL);

    /* Wait for GO_SIGNAL from parent (user pressed PLAY) */
    char go = 0;
    while (read(go_fd, &go, 1) != 1)
        ;
    close(go_fd);

    TravelerQuery *tq = NULL; int nq = 0;
    Graph *g = parser_load(filename, &tq, &nq);
    free(tq);

    DijkstraResult res = dijkstra_run(g, src, dst);

    if (!res.found || res.path_len == 0) {
        IpcMsg msg = { src, -1 };
        (void)write(write_fd, &msg, sizeof(IpcMsg));
        dijkstra_free_result(&res); graph_free(g);
        close(write_fd); exit(EXIT_SUCCESS);
    }

    for (int i = 0; i < res.path_len; i++) {
        int cur  = res.path[i];
        int next = (i + 1 < res.path_len) ? res.path[i + 1] : -1;
        IpcMsg msg = { cur, next };
        if (write(write_fd, &msg, sizeof(IpcMsg)) != (ssize_t)sizeof(IpcMsg))
            break;
        if (next != -1) {
            EdgeNode *e = g->lists[cur].head;
            int w = 1;
            while (e) { if (e->dest == next) { w = e->weight; break; } e = e->next; }
            struct timespec ts;
            long ms = (long)w * MS_PER_JUMP;
            ts.tv_sec = ms / 1000; ts.tv_nsec = (ms % 1000) * 1000000L;
            nanosleep(&ts, NULL);
        }
    }

    dijkstra_free_result(&res); graph_free(g);
    close(write_fd); exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <graph_file>\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *filename = argv[1];

    TravelerQuery *travelers     = NULL;
    int            num_travelers = 0;
    Graph *g = parser_load(filename, &travelers, &num_travelers);

    pid_t *child_pids = malloc(sizeof(pid_t) * (size_t)num_travelers);
    int   *read_fds   = malloc(sizeof(int)   * (size_t)num_travelers);
    int   *go_fds     = malloc(sizeof(int)   * (size_t)num_travelers);
    if (!child_pids || !read_fds || !go_fds) {
        fprintf(stderr, "Error: allocation failed\n");
        free(travelers); graph_free(g); return EXIT_FAILURE;
    }

    g_child_pids    = child_pids;
    g_num_travelers = num_travelers;
    struct sigaction sa;
    sa.sa_handler = parent_signal_handler;
    sigemptyset(&sa.sa_mask); sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL); sigaction(SIGTERM, &sa, NULL);

#ifdef WITH_RAYLIB
    TrainAnim *anims = calloc((size_t)num_travelers, sizeof(TrainAnim));
    if (!anims) {
        fprintf(stderr, "Error: allocation failed\n");
        free(child_pids); free(read_fds); free(go_fds);
        free(travelers); graph_free(g); return EXIT_FAILURE;
    }
#endif

    for (int i = 0; i < num_travelers; i++) {
        child_pids[i] = -1;
        int c2p[2], p2c[2];
        if (pipe(c2p) < 0 || pipe(p2c) < 0) {
            perror("pipe");
            parent_signal_handler(SIGTERM);
            for (int j = 0; j < i; j++) waitpid(child_pids[j], NULL, 0);
            free(child_pids); free(read_fds); free(go_fds);
            free(travelers); graph_free(g); return EXIT_FAILURE;
        }
        int src_i = travelers[i].src, dst_i = travelers[i].dst;
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(c2p[0]); close(c2p[1]); close(p2c[0]); close(p2c[1]);
            parent_signal_handler(SIGTERM);
            for (int j = 0; j < i; j++) waitpid(child_pids[j], NULL, 0);
            free(child_pids); free(read_fds); free(go_fds);
            free(travelers); graph_free(g); return EXIT_FAILURE;
        }
        if (pid == 0) {
            close(c2p[0]); close(p2c[1]);
            for (int j = 0; j < i; j++) { close(read_fds[j]); close(go_fds[j]); }
            free(child_pids); free(read_fds); free(go_fds);
#ifdef WITH_RAYLIB
            free(anims);
#endif
            free(travelers); graph_free(g);
            child_main(filename, src_i, dst_i, c2p[1], p2c[0]);
        }
        close(c2p[1]); close(p2c[0]);
        read_fds[i] = c2p[0];
        go_fds[i]   = p2c[1];
        child_pids[i] = pid;
#ifdef WITH_RAYLIB
        anims[i].child_pid   = pid;
        anims[i].color_index = i;
        anims[i].src_id      = travelers[i].src;
        anims[i].dst_id      = travelers[i].dst;
        anims[i].cur_node    = travelers[i].src;
        anims[i].next_node   = -1;
        anims[i].phase       = PHASE_IDLE;
        anims[i].train_x     = 0.0f;
        anims[i].train_y     = 0.0f;
        anims[i].timer_ms    = 0.0;
        /* M5: IPC-driven — no path stored in parent */
        anims[i].path        = NULL;
        anims[i].path_len    = 0;
        anims[i].seg         = 0;
#endif
    }

#ifdef WITH_RAYLIB
    renderer_run(g, STATION_NAMES, anims, num_travelers, read_fds, go_fds, 1);
    free(anims);
#else
    (void)STATION_NAMES;
    for (int i = 0; i < num_travelers; i++) {
        char go = GO_SIGNAL;
        (void)write(go_fds[i], &go, 1);
        close(go_fds[i]);
    }
    int done[MAX_TRAVELERS] = {0};
    int finished = 0;
    while (finished < num_travelers) {
        fd_set rfds; FD_ZERO(&rfds);
        int maxfd = 0;
        for (int i = 0; i < num_travelers; i++) {
            if (!done[i]) { FD_SET(read_fds[i], &rfds);
                if (read_fds[i] > maxfd) maxfd = read_fds[i]; }
        }
        if (select(maxfd + 1, &rfds, NULL, NULL, NULL) <= 0) break;
        for (int i = 0; i < num_travelers; i++) {
            if (done[i] || !FD_ISSET(read_fds[i], &rfds)) continue;
            IpcMsg msg;
            if (read(read_fds[i], &msg, sizeof(IpcMsg)) == (ssize_t)sizeof(IpcMsg)) {
                if (msg.next_node == -1)
                    printf("[PID=%d] arrived at node %d | DESTINATION\n",
                           (int)child_pids[i], msg.current_node);
                else
                    printf("[PID=%d] arrived at node %d | next node: %d\n",
                           (int)child_pids[i], msg.current_node, msg.next_node);
                fflush(stdout);
            } else { done[i] = 1; finished++; }
        }
    }
#endif

    for (int i = 0; i < num_travelers; i++) close(read_fds[i]);
    for (int i = 0; i < num_travelers; i++) {
        if (child_pids[i] > 0) {
            waitpid(child_pids[i], NULL, 0);
            printf("[PID=%d] finished\n", (int)child_pids[i]);
            fflush(stdout);
        }
    }

    free(child_pids); free(read_fds); free(go_fds);
    free(travelers); graph_free(g);
    return EXIT_SUCCESS;
}
#endif  /* milestone selector */