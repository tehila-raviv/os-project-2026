#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "scheduler.h"
#include "ipc.h"

/* ── Init ─────────────────────────────────────────────────── */

void sched_init(NodeQueue *queues, int num_nodes)
{
    for (int i = 0; i < num_nodes; i++) {
        queues[i].occupied  = 0;
        queues[i].queue_len = 0;
    }
}

/* ── Request ──────────────────────────────────────────────── */

int sched_request(NodeQueue *queues, int node_id,
                  int traveler_idx, int remaining_hops,
                  long *seq_counter)
{
    NodeQueue *nq = &queues[node_id];

    if (!nq->occupied) {
        /* Node is free — admit immediately */
        nq->occupied = 1;
        return 1;
    }

    /* Node busy — enqueue the request */
    if (nq->queue_len < MAX_WAITERS) {
        WaitEntry *e   = &nq->queue[nq->queue_len++];
        e->traveler_idx   = traveler_idx;
        e->remaining_hops = remaining_hops;
        e->arrival_seq    = (*seq_counter)++;
    }
    return 0;
}

/* ── Release ──────────────────────────────────────────────── */

/*
 * Pick the next waiter based on algorithm.
 * FCFS: lowest arrival_seq.
 * SJF : lowest remaining_hops (ties broken by arrival_seq).
 */
static int pick_next(NodeQueue *nq, SchedAlgo algo)
{
    if (nq->queue_len == 0) return -1;

    int best = 0;
    for (int i = 1; i < nq->queue_len; i++) {
        WaitEntry *b = &nq->queue[best];
        WaitEntry *c = &nq->queue[i];

        if (algo == SCHED_SJF) {
            if (c->remaining_hops < b->remaining_hops ||
                (c->remaining_hops == b->remaining_hops &&
                 c->arrival_seq   <  b->arrival_seq)) {
                best = i;
            }
        } else { /* FCFS */
            if (c->arrival_seq < b->arrival_seq)
                best = i;
        }
    }
    return best;
}

int sched_release(NodeQueue *queues, int node_id,
                  SchedAlgo algo, int *admit_fds)
{
    NodeQueue *nq = &queues[node_id];
    nq->occupied  = 0;

    if (nq->queue_len == 0)
        return -1;   /* node is now free */

    int best_pos = pick_next(nq, algo);
    int tidx     = nq->queue[best_pos].traveler_idx;

    /* PRINT SCHEDULER DECISION */
    printf("\n[SCHEDULER DECISION]\n");
    printf("Waiting: ");
    for (int i = 0; i < nq->queue_len; i++) {
        printf("T%d ", nq->queue[i].traveler_idx);
    }
    printf("\n");
    
    printf("Chosen: T%d\n", tidx);
    
    if (algo == SCHED_SJF) {
        printf("Why: SJF - T%d has %d hops (shortest)\n", 
               tidx, nq->queue[best_pos].remaining_hops);
    } else {
        printf("Why: FCFS - T%d arrived first (seq=%ld)\n", 
               tidx, nq->queue[best_pos].arrival_seq);
    }
    printf("\n");
    fflush(stdout);

    /* Remove the chosen entry by shifting the rest left */
    for (int i = best_pos; i < nq->queue_len - 1; i++)
        nq->queue[i] = nq->queue[i + 1];
    nq->queue_len--;

    /* Admit the chosen traveler */
    nq->occupied = 1;
    char sig = ADMIT_SIGNAL;
    if (write(admit_fds[tidx], &sig, 1) != 1)
        perror("sched_release: write admit_fd");

    return tidx;
}

/* ── Name ─────────────────────────────────────────────────── */

const char *sched_algo_name(SchedAlgo algo)
{
    switch (algo) {
        case SCHED_NONE: return "none";
        case SCHED_FCFS: return "FCFS";
        case SCHED_SJF:  return "SJF";
        default:         return "UNKNOWN";
    }
}