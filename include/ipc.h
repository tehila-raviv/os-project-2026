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
 * Why pipes?
 *   - Simple and portable — no setup beyond pipe().
 *   - Each child has its own dedicated pair so the parent always knows
 *     which child sent a message without an ID field in the packet.
 *   - FDs close automatically on process exit — no cleanup needed.
 *   - Works naturally with select() for fair message interleaving.
 *   - The GO_SIGNAL pattern elegantly solves the PLAY-button sync:
 *     children block on read() until the parent sends the signal,
 *     so no animation data is produced before the user presses PLAY.
 *
 * Synchronization mechanism (Milestone 6): POSIX named semaphores.
 *   One semaphore per graph node, named "/trainos_node_N".
 *   Each semaphore is initialized to 1 (binary mutex).
 *   A child calls sem_wait() before entering a node and sem_post()
 *   after the 1-second stay, enforcing mutual exclusion per node.
 *
 * Why named semaphores?
 *   - Work across unrelated processes (unlike unnamed sem_init with
 *     PTHREAD_PROCESS_SHARED which requires shared memory setup).
 *   - Persistent until explicitly unlinked — parent creates them
 *     before fork() and unlinks them after all children finish.
 *   - Blocking sem_wait() provides starvation-free waiting with no
 *     busy-spin, and the OS guarantees every waiter eventually wakes.
 */

#include <sys/types.h>   /* pid_t */

/* Maximum number of simultaneous travelers (shared with renderer) */
#define MAX_TRAVELERS  16

/* ms per weight unit — controls child sleep and animation slide duration */
#define MS_PER_JUMP   300

/* Duration (ms) a traveler holds a node (the critical section) */
#define MS_NODE_STAY  1000

/* Single byte sent from parent to child to start the journey */
#define GO_SIGNAL  ((char)1)

/* Named semaphore prefix for node locks (Milestone 6) */
#define NODE_SEM_PREFIX  "/trainos_node_"

/* Message types sent from child to parent */
typedef enum {
    MSG_AT_NODE = 0,  /* child arrived at and entered a node (holds the lock) */
    MSG_WAITING  = 1  /* child is waiting outside a node (blocked on semaphore) */
} IpcMsgType;

/* Message sent from child to parent on every node event */
typedef struct {
    IpcMsgType type;        /* MSG_AT_NODE or MSG_WAITING                   */
    int        current_node; /* node the child just arrived at / waiting for */
    int        next_node;    /* next node on the path (-1 = DESTINATION)     */
} IpcMsg;

#endif /* IPC_H */