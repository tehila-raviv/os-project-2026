#ifndef IPC_H
#define IPC_H

/*
 * IPC mechanism: PIPES (two pipes per child).
 *
 *   child_to_parent pipe: child writes IpcMsg on every node arrival.
 *   parent_to_child pipe: parent writes one GO_SIGNAL byte when the
 *                         user presses PLAY, allowing the child to
 *                         start its journey.
 *
 * Synchronization mechanism (Milestone 6): POSIX named semaphores.
 *   One semaphore per graph node, named "/trainos_node_N".
 *
 * Scheduling (Milestone 7): parent-managed wait queues per node.
 *   When a child wants to enter a busy node it sends MSG_WAITING.
 *   The parent queues the request and, when the node is free, picks
 *   the next child according to the chosen scheduling algorithm
 *   (FCFS or SJF) and sends it an ADMIT_SIGNAL via a dedicated
 *   admit pipe (one per child, parent->child direction).
 *
 * Scheduling algorithms supported:
 *   SCHED_FCFS  - First Come First Served (arrival order)
 *   SCHED_SJF   - Shortest Job First (fewest remaining path hops)
 */

#include <sys/types.h>   /* pid_t */

/* Maximum number of simultaneous travelers (shared with renderer) */
#define MAX_TRAVELERS  16

/* ms per weight unit — controls child sleep and animation slide duration */
#define MS_PER_JUMP   300

/* Duration (ms) a traveler holds a node (the critical section) */
#define MS_NODE_STAY  1000

/* Single byte sent from parent to child to start the journey */
#define GO_SIGNAL     ((char)1)

/* Single byte sent from parent to child to admit it into a node (M7) */
#define ADMIT_SIGNAL  ((char)2)

/* Named semaphore prefix for node locks (Milestone 6) */
#define NODE_SEM_PREFIX  "/trainos_node_"

/* ── Scheduling algorithm selector (Milestone 7) ── */
typedef enum {
    SCHED_NONE = -1,  /* No scheduling — used by milestones 1-6     */
    SCHED_FCFS = 0,   /* First Come First Served                    */
    SCHED_SJF  = 1    /* Shortest Job First (remaining hops)        */
} SchedAlgo;

/* Message types sent from child to parent */
typedef enum {
    MSG_AT_NODE  = 0,  /* child arrived at and entered a node        */
    MSG_WAITING  = 1,  /* child is waiting outside a node            */
    MSG_LEAVING  = 2,  /* child is leaving a node (node now free)    */
    MSG_NO_PATH  = 3   /* child found no route to destination        */
} IpcMsgType;

/* Message sent from child to parent on every node event */
typedef struct {
    IpcMsgType type;          /* MSG_AT_NODE, MSG_WAITING, MSG_LEAVING, or MSG_NO_PATH */
    int        current_node;  /* node the child arrived at / waiting for  */
    int        next_node;     /* next node on path (-1 = DESTINATION)     */
    int        remaining_hops;/* hops left in path after current node (SJF) */
} IpcMsg;

#endif /* IPC_H */