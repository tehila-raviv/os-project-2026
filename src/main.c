#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <time.h>
#include <signal.h>
#include <semaphore.h>
#include <fcntl.h>
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

/* ── Signal handling ─────────────────────────────────────────── */
__attribute__((unused)) static pid_t *g_child_pids    = NULL;
__attribute__((unused)) static int    g_num_travelers = 0;

__attribute__((unused))
static void parent_signal_handler(int sig) {
    (void)sig;
    if (g_child_pids)
        for (int i = 0; i < g_num_travelers; i++)
            if (g_child_pids[i] > 0)
                kill(g_child_pids[i], SIGTERM);
}

/* ── Semaphore name helpers ──────────────────────────────────── */

/* Node mutex: "/trainos_node_N" — binary semaphore, value=1 */
static void node_sem_name(int node, char *buf, size_t bufsz) {
    snprintf(buf, bufsz, "%s%d", NODE_SEM_PREFIX, node);
}

/*
 * Barrier semaphore: "/trainos_barrier"
 * Used as a countdown latch so all children depart simultaneously.
 * Initialized to num_travelers. Each child decrements it once after
 * receiving GO_SIGNAL. A child spins (sem_trywait on a "gate" semaphore)
 * until all peers have decremented, then all proceed together.
 *
 * Implementation uses TWO named semaphores:
 *   /trainos_bar_count  — countdown: initialized to num_travelers,
 *                         each child does sem_wait to decrement by 1.
 *                         When it hits 0, barrier is complete.
 *   /trainos_bar_gate   — gate: initialized to 0, the last child
 *                         (count hits 0) posts it num_travelers times
 *                         to release all waiting children.
 */
#define BAR_COUNT_SEM  "/trainos_bar_count"
#define BAR_GATE_SEM   "/trainos_bar_gate"

__attribute__((unused))
static int create_node_semaphores(int num_nodes) {
    char name[64];
    for (int i = 0; i < num_nodes; i++) {
        node_sem_name(i, name, sizeof(name));
        sem_unlink(name);
        sem_t *s = sem_open(name, O_CREAT | O_EXCL, 0600, 1);
        if (s == SEM_FAILED) {
            perror("sem_open node");
            for (int j = 0; j < i; j++) {
                node_sem_name(j, name, sizeof(name));
                sem_unlink(name);
            }
            return -1;
        }
        sem_close(s);
    }
    return 0;
}

__attribute__((unused))
static void unlink_node_semaphores(int num_nodes) {
    char name[64];
    for (int i = 0; i < num_nodes; i++) {
        node_sem_name(i, name, sizeof(name));
        sem_unlink(name);
    }
}

/* Create the barrier semaphores (called by parent before fork) */
__attribute__((unused))
static int create_barrier(int num_travelers) {
    sem_unlink(BAR_COUNT_SEM);
    sem_unlink(BAR_GATE_SEM);
    sem_t *cnt = sem_open(BAR_COUNT_SEM, O_CREAT|O_EXCL, 0600, num_travelers);
    if (cnt == SEM_FAILED) { perror("sem_open bar_count"); return -1; }
    sem_close(cnt);
    sem_t *gate = sem_open(BAR_GATE_SEM, O_CREAT|O_EXCL, 0600, 0);
    if (gate == SEM_FAILED) {
        perror("sem_open bar_gate");
        sem_unlink(BAR_COUNT_SEM);
        return -1;
    }
    sem_close(gate);
    return 0;
}

__attribute__((unused))
static void unlink_barrier(void) {
    sem_unlink(BAR_COUNT_SEM);
    sem_unlink(BAR_GATE_SEM);
}

/*
 * barrier_wait() — called by each child after GO_SIGNAL.
 * All children block here until every child has called barrier_wait(),
 * then all are released simultaneously.
 *
 * Algorithm (reusable sense-reversal barrier via two semaphores):
 *   1. Open count semaphore and decrement it (sem_wait = -1).
 *   2. Read current value:
 *      - If > 0: we are not the last — wait on gate semaphore.
 *      - If == 0: we are the last — post gate (num_travelers) times
 *                 to wake everyone (including ourselves).
 *   3. Wait on gate (released by the last child).
 */
__attribute__((unused))
static void barrier_wait(int num_travelers) {
    sem_t *cnt  = sem_open(BAR_COUNT_SEM, 0);
    sem_t *gate = sem_open(BAR_GATE_SEM,  0);
    if (cnt == SEM_FAILED || gate == SEM_FAILED) {
        /* Barrier unavailable — proceed without synchronization */
        if (cnt  != SEM_FAILED) sem_close(cnt);
        if (gate != SEM_FAILED) sem_close(gate);
        return;
    }

    sem_wait(cnt);          /* decrement countdown */
    int val = 0;
    sem_getvalue(cnt, &val);

    if (val == 0) {
        /* Last child — open the gate for everyone */
        for (int i = 0; i < num_travelers; i++)
            sem_post(gate);
    }

    sem_wait(gate);         /* block until gate is opened */

    sem_close(cnt);
    sem_close(gate);
}

/* ════════════════════════════════════════════════════════════════
 * MILESTONE 1
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
 * MILESTONE 2 / 3
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
    TrainAnim anim;
    anim.child_pid        = -1;
    anim.color_index      = 0;
    anim.src_id           = src;
    anim.dst_id           = dst;
    anim.cur_node         = src;
    anim.next_node        = (res.found && res.path_len > 1) ? res.path[1] : -1;
    anim.phase            = (!res.found || res.path_len <= 1) ? PHASE_ARRIVED : PHASE_IDLE;
    anim.waiting_for_node = -1;
    anim.train_x          = 0.0f;
    anim.train_y          = 0.0f;
    anim.timer_ms         = 0.0;
    anim.path             = res.found ? res.path : NULL;
    anim.path_len         = res.found ? res.path_len : 0;
    anim.seg              = 0;
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
 * MILESTONE 4
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

    DijkstraResult *results = malloc(sizeof(DijkstraResult) * (size_t)num_travelers);
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
        anims[i].child_pid        = pid;
        anims[i].color_index      = i;
        anims[i].src_id           = travelers[i].src;
        anims[i].dst_id           = travelers[i].dst;
        anims[i].cur_node         = results[i].found ? results[i].path[0] : travelers[i].src;
        anims[i].next_node        = (results[i].found && results[i].path_len > 1)
                                    ? results[i].path[1] : -1;
        anims[i].phase            = (!results[i].found || results[i].path_len <= 1)
                                    ? PHASE_ARRIVED : PHASE_IDLE;
        anims[i].waiting_for_node = -1;
        anims[i].train_x          = 0.0f;
        anims[i].train_y          = 0.0f;
        anims[i].timer_ms         = 0.0;
        anims[i].path             = results[i].found ? results[i].path : NULL;
        anims[i].path_len         = results[i].found ? results[i].path_len : 0;
        anims[i].seg              = 0;
#endif
    }
#ifdef WITH_RAYLIB
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
 * MILESTONE 5
 * ════════════════════════════════════════════════════════════════ */
#elif defined(MILESTONE5)

static void child_main_m5(const char *filename, int src, int dst,
                           int write_fd, int go_fd)
{
    signal(SIGINT, SIG_DFL); signal(SIGTERM, SIG_DFL);
    char go = 0;
    while (read(go_fd, &go, 1) != 1) ;
    close(go_fd);

    TravelerQuery *tq = NULL; int nq = 0;
    Graph *g = parser_load(filename, &tq, &nq);
    free(tq);
    DijkstraResult res = dijkstra_run(g, src, dst);

    if (!res.found || res.path_len == 0) {
        IpcMsg msg = { MSG_AT_NODE, src, -1 };
        (void)write(write_fd, &msg, sizeof(IpcMsg));
        dijkstra_free_result(&res); graph_free(g);
        close(write_fd); exit(EXIT_SUCCESS);
    }
    for (int i = 0; i < res.path_len; i++) {
        int cur  = res.path[i];
        int next = (i + 1 < res.path_len) ? res.path[i + 1] : -1;
        IpcMsg msg = { MSG_AT_NODE, cur, next };
        if (write(write_fd, &msg, sizeof(IpcMsg)) != (ssize_t)sizeof(IpcMsg)) break;
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
            child_main_m5(filename, src_i, dst_i, c2p[1], p2c[0]);
        }
        close(c2p[1]); close(p2c[0]);
        read_fds[i] = c2p[0];
        go_fds[i]   = p2c[1];
        child_pids[i] = pid;
#ifdef WITH_RAYLIB
        anims[i].child_pid        = pid;
        anims[i].color_index      = i;
        anims[i].src_id           = travelers[i].src;
        anims[i].dst_id           = travelers[i].dst;
        anims[i].cur_node         = travelers[i].src;
        anims[i].next_node        = -1;
        anims[i].phase            = PHASE_IDLE;
        anims[i].waiting_for_node = -1;
        anims[i].train_x          = 0.0f;
        anims[i].train_y          = 0.0f;
        anims[i].timer_ms         = 0.0;
        anims[i].path             = NULL;
        anims[i].path_len         = 0;
        anims[i].seg              = 0;
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
                if (msg.type == MSG_WAITING)
                    printf("[PID=%d] waiting outside node %d\n",
                           (int)child_pids[i], msg.current_node);
                else if (msg.next_node == -1)
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

/* ════════════════════════════════════════════════════════════════
 * MILESTONE 6 — synchronization: at most one traveler per node.
 *
 * Node mutex:  POSIX named semaphore per node ("/trainos_node_N"),
 *              binary (value=1). Ensures mutual exclusion.
 *
 * Start barrier: two named semaphores ("/trainos_bar_count" and
 *              "/trainos_bar_gate") implement a countdown latch.
 *              All children call barrier_wait() after GO_SIGNAL so
 *              they all begin travelling at the exact same instant,
 *              guaranteeing simultaneous arrival at shared nodes.
 * ════════════════════════════════════════════════════════════════ */
#else   /* MILESTONE6 (default) */

static void child_main_m6(const char *filename, int src, int dst,
                           int write_fd, int go_fd, int num_travelers)
{
    signal(SIGINT,  SIG_DFL);
    signal(SIGTERM, SIG_DFL);

    /* Wait for GO_SIGNAL from parent (user pressed PLAY) */
    char go = 0;
    while (read(go_fd, &go, 1) != 1) ;
    close(go_fd);

    /*
     * Barrier: block here until ALL children have received GO_SIGNAL.
     * This guarantees every child starts travelling at the same instant,
     * so they arrive at shared (bottleneck) nodes simultaneously and
     * the semaphore contention — and the "waiting" visual — is clearly
     * visible for every traveler.
     */
    barrier_wait(num_travelers);

    TravelerQuery *tq = NULL; int nq = 0;
    Graph *g = parser_load(filename, &tq, &nq);
    free(tq);

    DijkstraResult res = dijkstra_run(g, src, dst);

    if (!res.found || res.path_len == 0) {
        /* No path: lock source, report, stay 1 s, release */
        char sem_name[64];
        node_sem_name(src, sem_name, sizeof(sem_name));
        sem_t *s = sem_open(sem_name, 0);
        if (s != SEM_FAILED) {
            if (sem_trywait(s) != 0) {
                IpcMsg w = { MSG_WAITING, src, -1 };
                (void)write(write_fd, &w, sizeof(IpcMsg));
                sem_wait(s);
            }
            IpcMsg msg = { MSG_AT_NODE, src, -1 };
            (void)write(write_fd, &msg, sizeof(IpcMsg));
            struct timespec ts = { MS_NODE_STAY / 1000,
                                   (MS_NODE_STAY % 1000) * 1000000L };
            nanosleep(&ts, NULL);
            sem_post(s);
            sem_close(s);
        } else {
            IpcMsg msg = { MSG_AT_NODE, src, -1 };
            (void)write(write_fd, &msg, sizeof(IpcMsg));
        }
        dijkstra_free_result(&res); graph_free(g);
        close(write_fd); exit(EXIT_SUCCESS);
    }

    for (int i = 0; i < res.path_len; i++) {
        int cur  = res.path[i];
        int next = (i + 1 < res.path_len) ? res.path[i + 1] : -1;

        /* ── Acquire node lock ── */
        char sem_name[64];
        node_sem_name(cur, sem_name, sizeof(sem_name));
        sem_t *s = sem_open(sem_name, 0);
        if (s == SEM_FAILED) { perror("sem_open (child)"); break; }

        if (sem_trywait(s) != 0) {
            /* Node occupied: tell parent we are waiting outside */
            IpcMsg wait_msg = { MSG_WAITING, cur, next };
            (void)write(write_fd, &wait_msg, sizeof(IpcMsg));
            sem_wait(s);   /* block until the node is free */
        }

        /* ── Inside the node (critical section) ── */
        IpcMsg at_msg = { MSG_AT_NODE, cur, next };
        if (write(write_fd, &at_msg, sizeof(IpcMsg)) != (ssize_t)sizeof(IpcMsg)) {
            sem_post(s); sem_close(s); break;
        }

        struct timespec stay = { MS_NODE_STAY / 1000,
                                 (MS_NODE_STAY % 1000) * 1000000L };
        nanosleep(&stay, NULL);

        /* ── Release node lock ── */
        sem_post(s);
        sem_close(s);

        /* Travel to next node */
        if (next != -1) {
            EdgeNode *e = g->lists[cur].head;
            int w = 1;
            while (e) { if (e->dest == next) { w = e->weight; break; } e = e->next; }
            long ms = (long)w * MS_PER_JUMP;
            struct timespec travel = { ms / 1000, (ms % 1000) * 1000000L };
            nanosleep(&travel, NULL);
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

    /* Create node mutex semaphores (one per node) */
    if (create_node_semaphores(g->num_vertices) < 0) {
        free(travelers); graph_free(g); return EXIT_FAILURE;
    }

    /* Create start barrier (released when all children have GO_SIGNAL) */
    if (create_barrier(num_travelers) < 0) {
        unlink_node_semaphores(g->num_vertices);
        free(travelers); graph_free(g); return EXIT_FAILURE;
    }

    pid_t *child_pids = malloc(sizeof(pid_t) * (size_t)num_travelers);
    int   *read_fds   = malloc(sizeof(int)   * (size_t)num_travelers);
    int   *go_fds     = malloc(sizeof(int)   * (size_t)num_travelers);
    if (!child_pids || !read_fds || !go_fds) {
        fprintf(stderr, "Error: allocation failed\n");
        unlink_node_semaphores(g->num_vertices);
        unlink_barrier();
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
        unlink_node_semaphores(g->num_vertices);
        unlink_barrier();
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
            unlink_node_semaphores(g->num_vertices);
            unlink_barrier();
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
            unlink_node_semaphores(g->num_vertices);
            unlink_barrier();
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
            child_main_m6(filename, src_i, dst_i, c2p[1], p2c[0], num_travelers);
            /* child_main_m6 calls exit() — unreachable */
        }
        close(c2p[1]); close(p2c[0]);
        read_fds[i]   = c2p[0];
        go_fds[i]     = p2c[1];
        child_pids[i] = pid;
#ifdef WITH_RAYLIB
        anims[i].child_pid        = pid;
        anims[i].color_index      = i;
        anims[i].src_id           = travelers[i].src;
        anims[i].dst_id           = travelers[i].dst;
        anims[i].cur_node         = travelers[i].src;
        anims[i].next_node        = -1;
        anims[i].phase            = PHASE_IDLE;
        anims[i].waiting_for_node = -1;
        anims[i].train_x          = 0.0f;
        anims[i].train_y          = 0.0f;
        anims[i].timer_ms         = 0.0;
        anims[i].path             = NULL;
        anims[i].path_len         = 0;
        anims[i].seg              = 0;
#endif
    }

#ifdef WITH_RAYLIB
    renderer_run(g, STATION_NAMES, anims, num_travelers, read_fds, go_fds, 1);
    free(anims);
#else
    (void)STATION_NAMES;
    for (int i = 0; i < num_travelers; i++) {
        char go_sig = GO_SIGNAL;
        (void)write(go_fds[i], &go_sig, 1);
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
                if (msg.type == MSG_WAITING)
                    printf("[PID=%d] waiting outside node %d\n",
                           (int)child_pids[i], msg.current_node);
                else if (msg.next_node == -1)
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

    /* Parent cleans up all semaphores */
    unlink_node_semaphores(g->num_vertices);
    unlink_barrier();

    free(child_pids); free(read_fds); free(go_fds);
    free(travelers); graph_free(g);
    return EXIT_SUCCESS;
}
#endif  /* milestone selector */