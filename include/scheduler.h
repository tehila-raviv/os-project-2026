#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "ipc.h"   /* SchedAlgo, MAX_TRAVELERS */

/*
 * scheduler.h — Milestone 7: parent-side node scheduling
 *
 * Each graph node has a NodeQueue that tracks:
 *   - whether the node is currently occupied
 *   - an ordered list of waiting travelers (indexed by traveler slot)
 *
 * When a traveler finishes its critical section it sends MSG_LEAVING.
 * The parent calls sched_node_release(), which picks the next waiter
 * according to the chosen algorithm and writes ADMIT_SIGNAL to that
 * traveler's admit pipe.
 *
 * FCFS: admits in arrival order (queue head first).
 * SJF : admits the waiter with the fewest remaining hops.
 */

/* Maximum waiters per node = total travelers */
#define MAX_WAITERS  MAX_TRAVELERS

/* Entry in a node's wait queue */
typedef struct {
    int traveler_idx;    /* index into the parent's traveler/pipe arrays */
    int remaining_hops;  /* remaining path hops at time of request (SJF key) */
    long arrival_seq;    /* monotonically increasing sequence number (FCFS key) */
} WaitEntry;

/* Per-node scheduling state held by the parent */
typedef struct {
    int        occupied;              /* 1 if a traveler is inside, 0 if free */
    WaitEntry  queue[MAX_WAITERS];    /* waiting travelers                     */
    int        queue_len;             /* number of entries in queue            */
} NodeQueue;

/*
 * Initialise all node queues (all nodes start empty).
 * Call once before forking children.
 */
void sched_init(NodeQueue *queues, int num_nodes);

/*
 * A traveler wants to enter node_id.
 * Returns 1 if the node is free (traveler can enter immediately),
 * 0 if the node is occupied (traveler must wait; entry is queued).
 *
 * When returning 1, marks the node as occupied.
 * When returning 0, appends the request to the node's wait queue.
 */
int sched_request(NodeQueue *queues, int node_id,
                  int traveler_idx, int remaining_hops,
                  long *seq_counter);

/*
 * The traveler currently holding node_id has finished its critical
 * section.  Releases the node and, if there are waiters, picks the
 * next one according to `algo`, writes ADMIT_SIGNAL to
 * admit_fds[chosen_idx], and marks the node occupied again.
 *
 * Returns the index of the traveler that was admitted, or -1 if the
 * node is now free (no waiters).
 */
int sched_release(NodeQueue *queues, int node_id,
                  SchedAlgo algo, int *admit_fds);

/* Human-readable name of the algorithm (for logging / GUI) */
const char *sched_algo_name(SchedAlgo algo);

#endif /* SCHEDULER_H */