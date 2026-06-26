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

static void node_sem_name(int node, char *buf, size_t bufsz) {
    snprintf(buf, bufsz, "%s%d", NODE_SEM_PREFIX, node);
}

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

__attribute__((unused))
static void barrier_wait(int num_travelers) {
    sem_t *cnt  = sem_open(BAR_COUNT_SEM, 0);
    sem_t *gate = sem_open(BAR_GATE_SEM,  0);
    if (cnt == SEM_FAILED || gate == SEM_FAILED) {
        if (cnt  != SEM_FAILED) sem_close(cnt);
        if (gate != SEM_FAILED) sem_close(gate);
        return;
    }
    sem_wait(cnt);
    int val = 0;
    sem_getvalue(cnt, &val);
    if (val == 0) {
        for (int i = 0; i < num_travelers; i++)
            sem_post(gate);
    }
    sem_wait(gate);
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
    renderer_run(g, STATION_NAMES, &anim, 1, &dummy_read_fd, NULL, NULL, 0, SCHED_NONE);
#else
    renderer_run(g, STATION_NAMES, &anim, 1, &dummy_read_fd, NULL, NULL, 1, SCHED_NONE);
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
    renderer_run(g, STATION_NAMES, anims, num_travelers, null_read_fds, NULL, NULL, 1, SCHED_NONE);
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

static int send_msg_and_wait_ack(int write_fd, int ack_fd, const IpcMsg *msg)
{
    char ack = 0;
    ssize_t n;

    if (write(write_fd, msg, sizeof(*msg)) != (ssize_t)sizeof(*msg))
        return 0;

    /*
     * Child sent a message to the parent.
     * It must wait until the parent reads it and sends ACK_SIGNAL back.
     */
    while (1) {
        n = read(ack_fd, &ack, 1);
        if (n == 1)
            return ack == ACK_SIGNAL;
        if (n == 0)
            return 0;
    }
}

static void child_main_m5(const char *filename, int src, int dst,
                           int write_fd, int go_fd)
{
    signal(SIGINT, SIG_DFL); signal(SIGTERM, SIG_DFL);
    char go = 0;
    while (read(go_fd, &go, 1) != 1) ;

    /*
     * Do not close go_fd here.
     * After GO_SIGNAL, the same parent->child pipe is used for ACK_SIGNAL.
     */

    TravelerQuery *tq = NULL; int nq = 0;
    Graph *g = parser_load(filename, &tq, &nq);
    free(tq);
    DijkstraResult res = dijkstra_run(g, src, dst);

    if (!res.found || res.path_len == 0) {
        IpcMsg msg = { MSG_AT_NODE, src, -1, 0 };
        (void)send_msg_and_wait_ack(write_fd, go_fd, &msg);

        dijkstra_free_result(&res);
        graph_free(g);
        close(go_fd);
        close(write_fd);
        exit(EXIT_SUCCESS);
    }
    for (int i = 0; i < res.path_len; i++) {
        int cur  = res.path[i];
        int next = (i + 1 < res.path_len) ? res.path[i + 1] : -1;
        int hops = res.path_len - 1 - i;
        IpcMsg msg = { MSG_AT_NODE, cur, next, hops };
        if (!send_msg_and_wait_ack(write_fd, go_fd, &msg))
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
    dijkstra_free_result(&res);
    graph_free(g);
    close(go_fd);
    close(write_fd);
    exit(EXIT_SUCCESS);
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
    renderer_run(g, STATION_NAMES, anims, num_travelers, read_fds, go_fds, NULL, 1, SCHED_NONE);
    free(anims);
#else
    (void)STATION_NAMES;
    for (int i = 0; i < num_travelers; i++) {
        char go = GO_SIGNAL;
        (void)write(go_fds[i], &go, 1);

        /*
         * Do not close go_fds[i] here.
         * It is used later to send ACK_SIGNAL back to the child.
         */
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

                /*
                 * Parent confirms it has read the message.
                 * Only after this ACK the child continues to the next node.
                 */
                char ack = ACK_SIGNAL;
                (void)write(go_fds[i], &ack, 1);

                fflush(stdout);
            } else { done[i] = 1; finished++; }
        }
    }
#endif
    for (int i = 0; i < num_travelers; i++) close(read_fds[i]);
    for (int i = 0; i < num_travelers; i++) close(go_fds[i]);
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
 * ════════════════════════════════════════════════════════════════ */
#elif defined(MILESTONE6)

static void child_main_m6(const char *filename, int src, int dst,
                           int write_fd, int go_fd, int num_travelers)
{
    signal(SIGINT,  SIG_DFL);
    signal(SIGTERM, SIG_DFL);

    char go = 0;
    while (read(go_fd, &go, 1) != 1) ;
    close(go_fd);

    barrier_wait(num_travelers);

    TravelerQuery *tq = NULL; int nq = 0;
    Graph *g = parser_load(filename, &tq, &nq);
    free(tq);

    DijkstraResult res = dijkstra_run(g, src, dst);

    if (!res.found || res.path_len == 0) {
        char sem_name[64];
        node_sem_name(src, sem_name, sizeof(sem_name));
        sem_t *s = sem_open(sem_name, 0);
        if (s != SEM_FAILED) {
            if (sem_trywait(s) != 0) {
                IpcMsg w = { MSG_WAITING, src, -1, 0 };
                (void)write(write_fd, &w, sizeof(IpcMsg));
                sem_wait(s);
            }
            IpcMsg msg = { MSG_AT_NODE, src, -1, 0 };
            (void)write(write_fd, &msg, sizeof(IpcMsg));
            struct timespec ts = { MS_NODE_STAY / 1000,
                                   (MS_NODE_STAY % 1000) * 1000000L };
            nanosleep(&ts, NULL);
            sem_post(s);
            sem_close(s);
        } else {
            IpcMsg msg = { MSG_AT_NODE, src, -1, 0 };
            (void)write(write_fd, &msg, sizeof(IpcMsg));
        }
        dijkstra_free_result(&res); graph_free(g);
        close(write_fd); exit(EXIT_SUCCESS);
    }

    for (int i = 0; i < res.path_len; i++) {
        int cur  = res.path[i];
        int next = (i + 1 < res.path_len) ? res.path[i + 1] : -1;
        int hops = res.path_len - 1 - i;

        char sem_name[64];
        node_sem_name(cur, sem_name, sizeof(sem_name));
        sem_t *s = sem_open(sem_name, 0);
        if (s == SEM_FAILED) { perror("sem_open (child)"); break; }

        if (sem_trywait(s) != 0) {
            IpcMsg wait_msg = { MSG_WAITING, cur, next, hops };
            (void)write(write_fd, &wait_msg, sizeof(IpcMsg));
            sem_wait(s);
        }

        IpcMsg at_msg = { MSG_AT_NODE, cur, next, hops };
        if (write(write_fd, &at_msg, sizeof(IpcMsg)) != (ssize_t)sizeof(IpcMsg)) {
            sem_post(s); sem_close(s); break;
        }

        struct timespec stay = { MS_NODE_STAY / 1000,
                                 (MS_NODE_STAY % 1000) * 1000000L };
        nanosleep(&stay, NULL);

        sem_post(s);
        sem_close(s);

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

    if (create_node_semaphores(g->num_vertices) < 0) {
        free(travelers); graph_free(g); return EXIT_FAILURE;
    }
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
    renderer_run(g, STATION_NAMES, anims, num_travelers, read_fds, go_fds, NULL, 1, SCHED_NONE);
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

    unlink_node_semaphores(g->num_vertices);
    unlink_barrier();

    free(child_pids); free(read_fds); free(go_fds);
    free(travelers); graph_free(g);
    return EXIT_SUCCESS;
}

/* ════════════════════════════════════════════════════════════════
 * MILESTONE 7 — parent-managed scheduling at each node.
 *
 * Architecture:
 *   Each child has THREE pipes:
 *     c2p  (child->parent): IpcMsg stream (MSG_AT_NODE, MSG_WAITING,
 *                           MSG_LEAVING)
 *     p2c  (parent->child): GO_SIGNAL byte to start journey
 *     admit(parent->child): ADMIT_SIGNAL byte to enter a node
 *
 *   When a child wants to enter a node:
 *     1. Sends MSG_WAITING to parent.
 *     2. Blocks on its admit pipe (read()).
 *     3. Parent queues the request in NodeQueue[node].
 *     4. If node is free, parent immediately writes ADMIT_SIGNAL.
 *     5. Otherwise parent waits; when holder sends MSG_LEAVING the
 *        parent picks the next waiter via the chosen SchedAlgo and
 *        writes ADMIT_SIGNAL to that child's admit pipe.
 *     6. Child unblocks, sends MSG_AT_NODE, does its stay (1 s),
 *        sends MSG_LEAVING, then travels to the next node.
 *
 *   The parent's select() loop monitors ALL c2p read ends and
 *   dispatches per message type.
 * ════════════════════════════════════════════════════════════════ */
#else  /* MILESTONE7 (default) */

#include "scheduler.h"

/* ── Child process for M7 ──────────────────────────────────── */

static void child_main_m7(const char *filename, int src, int dst,
                           int write_fd, int go_fd, int admit_fd,
                           int num_travelers)
{
    signal(SIGINT,  SIG_DFL);
    signal(SIGTERM, SIG_DFL);

    /* Wait for GO_SIGNAL */
    char go = 0;
    while (read(go_fd, &go, 1) != 1) ;
    close(go_fd);

    /* Barrier: all children start simultaneously */
    barrier_wait(num_travelers);

    TravelerQuery *tq = NULL; int nq_unused = 0;
    Graph *g = parser_load(filename, &tq, &nq_unused);
    free(tq);

    DijkstraResult res = dijkstra_run(g, src, dst);

    if (!res.found || res.path_len == 0) {
        /* No path: request src node, report, leave */
        int hops = 0;
        IpcMsg req = { MSG_WAITING, src, -1, hops };
        (void)write(write_fd, &req, sizeof(IpcMsg));

        char admit = 0;
        while (read(admit_fd, &admit, 1) != 1) ;  /* wait for parent admit */

        IpcMsg at = { MSG_AT_NODE, src, -1, hops };
        (void)write(write_fd, &at, sizeof(IpcMsg));

        struct timespec stay = { MS_NODE_STAY / 1000,
                                 (MS_NODE_STAY % 1000) * 1000000L };
        nanosleep(&stay, NULL);

        IpcMsg leaving = { MSG_LEAVING, src, -1, hops };
        (void)write(write_fd, &leaving, sizeof(IpcMsg));

        dijkstra_free_result(&res); graph_free(g);
        close(write_fd); close(admit_fd); exit(EXIT_SUCCESS);
    }

    for (int i = 0; i < res.path_len; i++) {
        int cur  = res.path[i];
        int next = (i + 1 < res.path_len) ? res.path[i + 1] : -1;
        int hops = res.path_len - 1 - i;  /* hops remaining after cur */

        /* ── Request node entry ── */
        IpcMsg req = { MSG_WAITING, cur, next, hops };
        if (write(write_fd, &req, sizeof(IpcMsg)) != (ssize_t)sizeof(IpcMsg))
            break;

        /* Block until parent admits us */
        char admit = 0;
        if (read(admit_fd, &admit, 1) != 1) break;

        /* ── Enter node (critical section) ── */
        IpcMsg at = { MSG_AT_NODE, cur, next, hops };
        if (write(write_fd, &at, sizeof(IpcMsg)) != (ssize_t)sizeof(IpcMsg))
            break;

        struct timespec stay = { MS_NODE_STAY / 1000,
                                 (MS_NODE_STAY % 1000) * 1000000L };
        nanosleep(&stay, NULL);

        /* ── Leave node ── */
        IpcMsg leaving = { MSG_LEAVING, cur, next, hops };
        if (write(write_fd, &leaving, sizeof(IpcMsg)) != (ssize_t)sizeof(IpcMsg))
            break;

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
    close(write_fd); close(admit_fd); exit(EXIT_SUCCESS);
}

/* ── Parent event loop (no raylib) ─────────────────────────── */

__attribute__((unused))
static void run_m7_terminal(int num_travelers, pid_t *child_pids,
                             int *read_fds, int *go_fds, int *admit_fds,
                             int num_nodes, SchedAlgo algo)
{
    NodeQueue *queues = calloc((size_t)num_nodes, sizeof(NodeQueue));
    if (!queues) { perror("calloc queues"); return; }
    sched_init(queues, num_nodes);

    long seq = 0;   /* monotonically increasing arrival sequence */

    /* Send GO_SIGNAL to all children */
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
            if (!done[i]) {
                FD_SET(read_fds[i], &rfds);
                if (read_fds[i] > maxfd) maxfd = read_fds[i];
            }
        }
        if (select(maxfd + 1, &rfds, NULL, NULL, NULL) <= 0) break;

        for (int i = 0; i < num_travelers; i++) {
            if (done[i] || !FD_ISSET(read_fds[i], &rfds)) continue;

            IpcMsg msg;
            ssize_t n = read(read_fds[i], &msg, sizeof(IpcMsg));
            if (n != (ssize_t)sizeof(IpcMsg)) {
                done[i] = 1; finished++;
                continue;
            }

            switch (msg.type) {

            case MSG_WAITING: {
                /* Child wants to enter msg.current_node */
                int admitted = sched_request(queues, msg.current_node,
                                             i, msg.remaining_hops, &seq);
                if (admitted) {
                    /* Node was free — admit immediately */
                    char sig = ADMIT_SIGNAL;
                    (void)write(admit_fds[i], &sig, 1);
                    printf("[PID=%d] admitted to node %d (%s, immediate)\n",
                           (int)child_pids[i], msg.current_node,
                           sched_algo_name(algo));
                } else {
                    printf("[PID=%d] waiting outside node %d (%s)\n",
                           (int)child_pids[i], msg.current_node,
                           sched_algo_name(algo));
                }
                fflush(stdout);
                break;
            }

            case MSG_AT_NODE:
                /* Child successfully entered; log it */
                if (msg.next_node == -1)
                    printf("[PID=%d] arrived at node %d | DESTINATION\n",
                           (int)child_pids[i], msg.current_node);
                else
                    printf("[PID=%d] arrived at node %d | next node: %d\n",
                           (int)child_pids[i], msg.current_node, msg.next_node);
                fflush(stdout);
                break;

            case MSG_LEAVING:
                /* Child leaving msg.current_node — wake next waiter */
                {
                    int next_tidx = sched_release(queues, msg.current_node,
                                                  algo, admit_fds);
                    if (next_tidx >= 0)
                        printf("[PID=%d] admitted to node %d (%s, scheduled)\n",
                               (int)child_pids[next_tidx], msg.current_node,
                               sched_algo_name(algo));
                }
                fflush(stdout);
                break;
            }
        }
    }

    free(queues);
}

/* ── M7 main ────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    /* Usage: ./sim -schd fcfs <file>
     *        ./sim -schd sjf  <file>   */
    if (argc != 4 || strcmp(argv[1], "-schd") != 0) {
        fprintf(stderr,
                "Usage: %s -schd <fcfs|sjf> <graph_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    SchedAlgo algo;
    if (strcmp(argv[2], "fcfs") == 0) {
        algo = SCHED_FCFS;
    } else if (strcmp(argv[2], "sjf") == 0) {
        algo = SCHED_SJF;
    } else {
        fprintf(stderr, "Error: unknown scheduler '%s' (use fcfs or sjf)\n",
                argv[2]);
        return EXIT_FAILURE;
    }

    const char *filename = argv[3];
    printf("TrainOS M7 | Scheduler: %s | File: %s\n",
           sched_algo_name(algo), filename);
    fflush(stdout);

    TravelerQuery *travelers     = NULL;
    int            num_travelers = 0;
    Graph *g = parser_load(filename, &travelers, &num_travelers);

    /* Barrier so all children depart simultaneously */
    if (create_barrier(num_travelers) < 0) {
        free(travelers); graph_free(g); return EXIT_FAILURE;
    }

    pid_t *child_pids = malloc(sizeof(pid_t) * (size_t)num_travelers);
    int   *read_fds   = malloc(sizeof(int)   * (size_t)num_travelers);
    int   *go_fds     = malloc(sizeof(int)   * (size_t)num_travelers);
    int   *admit_fds  = malloc(sizeof(int)   * (size_t)num_travelers);
    if (!child_pids || !read_fds || !go_fds || !admit_fds) {
        fprintf(stderr, "Error: allocation failed\n");
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
        free(child_pids); free(read_fds); free(go_fds); free(admit_fds);
        unlink_barrier();
        free(travelers); graph_free(g); return EXIT_FAILURE;
    }
#endif

    for (int i = 0; i < num_travelers; i++) {
        child_pids[i] = -1;
        int c2p[2], p2c[2], adm[2];
        if (pipe(c2p) < 0 || pipe(p2c) < 0 || pipe(adm) < 0) {
            perror("pipe");
            parent_signal_handler(SIGTERM);
            for (int j = 0; j < i; j++) waitpid(child_pids[j], NULL, 0);
            free(child_pids); free(read_fds); free(go_fds); free(admit_fds);
            unlink_barrier();
            free(travelers); graph_free(g); return EXIT_FAILURE;
        }

        int src_i = travelers[i].src, dst_i = travelers[i].dst;
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(c2p[0]); close(c2p[1]);
            close(p2c[0]); close(p2c[1]);
            close(adm[0]); close(adm[1]);
            parent_signal_handler(SIGTERM);
            for (int j = 0; j < i; j++) waitpid(child_pids[j], NULL, 0);
            free(child_pids); free(read_fds); free(go_fds); free(admit_fds);
            unlink_barrier();
            free(travelers); graph_free(g); return EXIT_FAILURE;
        }

        if (pid == 0) {
            /* Child: close parent-side ends */
            close(c2p[0]); close(p2c[1]); close(adm[1]);
            /* Close pipes from previous siblings */
            for (int j = 0; j < i; j++) {
                close(read_fds[j]);
                close(go_fds[j]);
                close(admit_fds[j]);
            }
            free(child_pids); free(read_fds); free(go_fds); free(admit_fds);
#ifdef WITH_RAYLIB
            free(anims);
#endif
            free(travelers); graph_free(g);
            child_main_m7(filename, src_i, dst_i,
                          c2p[1], p2c[0], adm[0], num_travelers);
            /* unreachable */
        }

        /* Parent: close child-side ends */
        close(c2p[1]); close(p2c[0]); close(adm[0]);
        read_fds[i]   = c2p[0];
        go_fds[i]     = p2c[1];
        admit_fds[i]  = adm[1];
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
    renderer_run(g, STATION_NAMES, anims, num_travelers,
                 read_fds, go_fds, admit_fds, 1, algo);
    free(anims);
#else
    (void)STATION_NAMES;
    run_m7_terminal(num_travelers, child_pids, read_fds, go_fds,
                    admit_fds, g->num_vertices, algo);
#endif

    for (int i = 0; i < num_travelers; i++) close(read_fds[i]);
    for (int i = 0; i < num_travelers; i++) close(admit_fds[i]);
    for (int i = 0; i < num_travelers; i++) {
        if (child_pids[i] > 0) {
            waitpid(child_pids[i], NULL, 0);
            printf("[PID=%d] finished\n", (int)child_pids[i]);
            fflush(stdout);
        }
    }

    unlink_barrier();
    free(child_pids); free(read_fds); free(go_fds); free(admit_fds);
    free(travelers); graph_free(g);
    return EXIT_SUCCESS;
}
#endif  /* milestone selector */
