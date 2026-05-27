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
 */

#include <sys/types.h>   /* pid_t */

/* Maximum number of simultaneous travelers (shared with renderer) */
#define MAX_TRAVELERS  16

/* ms per weight unit — controls child sleep and animation slide duration */
#define MS_PER_JUMP   300

/* Single byte sent from parent to child to start the journey */
#define GO_SIGNAL  ((char)1)

/* Message sent from child to parent on every node arrival */
typedef struct {
    int current_node;   /* node the child just arrived at           */
    int next_node;      /* next node on the path (-1 = DESTINATION) */
} IpcMsg;

#endif /* IPC_H */