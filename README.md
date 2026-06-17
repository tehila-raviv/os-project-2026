# TrainOS - Railway Traffic Simulation

## Group Members & Roles

| Name | Scrum Role |
|------|------------|
| Tehila Raviv | Product Owner |
| Tal Zada | Scrum Master |
| Ori Azarzar | Dev Team |
| Orel Ben David | Dev Team |
 
## Background Story
 
TrainOS is a railway traffic control system for a network of stations connected
by tracks. Each train is an independent OS process that computes its own shortest
route using Dijkstra's algorithm and travels autonomously across the network.
The system enforces station capacity (at most one train per station at a time)
and lets operators choose the scheduling policy that controls which waiting train
enters a busy station next.

## Project Description

TrainOS simulates a railway traffic system modelled as a directed weighted graph.
Multiple trains (OS processes) travel simultaneously across a network of stations,
using mechanisms learned in the course: processes, inter-process communication,
synchronisation, and scheduling.

Each station is a graph node; each track is a directed weighted edge whose weight
represents travel time. Trains compute the shortest route using Dijkstra's algorithm.

## Project Structure
 
```
.
├── src/
│   ├── main.c        # Entry point - milestone selector, fork, IPC, semaphores, scheduler
│   ├── graph.c       # Adjacency-list directed weighted graph
│   ├── dijkstra.c    # Shortest path + custom min-heap
│   ├── parser.c      # Input file parsing (single and multi-traveler formats)
│   ├── renderer.c    # raylib GUI rendering & animation (M2+)
│   └── scheduler.c   # Parent-side node scheduling: FCFS and SJF (M7)
│
├── include/
│   ├── graph.h
│   ├── dijkstra.h
│   ├── parser.h
│   ├── ipc.h         # IpcMsg struct, message types, pipe signals, SchedAlgo enum
│   ├── renderer.h
│   └── scheduler.h   # NodeQueue struct, sched_init/request/release API (M7)
│
├── tests/
│   ├── test1.txt     # Single traveler - normal path
│   ├── test2.txt     # Single traveler - disconnected graph
│   ├── test3.txt     # Single traveler - src == dst
│   ├── test4.txt     # Single traveler - larger graph (10 nodes)
│   ├── test4b.txt    # Single traveler - 15 nodes
│   ├── test5.txt     # Long path, many stops
│   ├── test6.txt     # Heavy weights vs short path
│   ├── test7.txt     # Single edge, minimal animation
│   ├── test8.txt     # src == dst inside a larger graph
│   ├── test9.txt     # Disconnected graph
│   ├── test10.txt    # Graph with a cycle
│   ├── testm4.txt    # Milestone 4 - 3 travelers
│   ├── testm4b.txt   # Milestone 4 - 4 travelers (edge cases)
│   ├── testm5.txt    # Milestone 5 - 2 travelers
│   ├── testm5b.txt   # Milestone 5 - 3 travelers (edge cases)
│   ├── testm6.txt    # Milestone 6 - 4 travelers, general sync test
│   ├── testm6b.txt   # Milestone 6 - bottleneck demo (3 travelers forced through node 3)
│   └── testm7b.txt   # Milestone 7 - FCFS vs SJF clearly differ (different destinations)
│
├── Makefile
└── README.md
```

## Input File Format

### Milestones 1–3 (single traveler)
```
N M          # N stations, M tracks
src dst w    # directed edge with travel-time weight w
...
src dst      # Dijkstra query
```

### Milestone 4+ (multiple travelers)
```
# graph definition
N M
src dst w
...
# travelers
K            # number of travelers
src dst      # K traveler queries
...
```

## Dependencies

- GCC
- [raylib](https://www.raylib.com/) - required from Milestone 2 onward, version: 4.5
  ```bash
  sudo apt install libraylib-dev
  ```

---

## Build & Run

```bash
make milestone1    # builds ./dijkstra  (terminal, single traveler)
make milestone2    # builds ./sim       (GUI, static graph)
make milestone3    # builds ./sim       (GUI, animation)
make milestone4    # builds ./sim       (GUI, multi-process)
make milestone5    # builds ./sim       (GUI, multi-process + IPC)
make milestone6    # builds ./sim       (GUI, multi-process + IPC + node sync)
make milestone7    # builds ./sim       (GUI, multi-process + IPC + scheduling)
make clean
```

### Run shortcuts
```bash
./dijkstra tests/test1.txt          # M1
./sim tests/testm4.txt              # M2/M3/M4
./sim tests/testm5.txt              # M5
./sim tests/testm6b.txt             # M6 bottleneck demo
./sim -schd fcfs tests/testm7b.txt       # M7 FCFS demo
./sim -schd sjf  tests/testm7b.txt       # M7 SJF demo
```

### Valgrind (M1 only - no raylib)
```bash
make valgrind
# or manually:
valgrind --leak-check=full --track-origins=yes --error-exitcode=1 \
         ./dijkstra tests/test1.txt
valgrind --leak-check=full --track-origins=yes --error-exitcode=1 \
         ./dijkstra tests/test2.txt
valgrind --leak-check=full --track-origins=yes --error-exitcode=1 \
         ./dijkstra tests/test3.txt
valgrind --leak-check=full --track-origins=yes --error-exitcode=1 \
         ./dijkstra tests/test4.txt
```

---

## Milestones

### Milestone 1 - Graph + Dijkstra

Directed weighted adjacency list (`graph.c`). Dijkstra with a custom min-heap
(`dijkstra.c`). Parser validates negative weights and out-of-range vertices.

```bash
make milestone1
./dijkstra tests/test1.txt
make test-m1
make valgrind
```

---

### Milestone 2 - Static GUI

raylib window with circular node layout, directed-edge arrows, weight labels,
path highlighting, and a colour-coded legend.

```bash
make milestone2
./sim tests/test1.txt
```

---

### Milestone 3 - Animation

Animated train circles travel along the Dijkstra path. Travel time proportional
to edge weight (300 ms/unit). 1-second pause at intermediate stations.
PLAY/STOP button, arrival overlays, live status bar.

```bash
make milestone3
./sim tests/test1.txt
```

---

### Milestone 4 - Multiple Processes

The parent computes all Dijkstra paths, forks one child per traveler, runs the
GUI showing all trains simultaneously in distinct colours. Each child prints
`[PID] started`, then sleeps until the parent sends SIGTERM when its animation
finishes. Parent calls `waitpid()` for every child before exiting.

```bash
make milestone4
./sim tests/testm4.txt
./sim tests/testm4b.txt
```

---

### Milestone 5 - Inter-Process Communication (IPC)

**IPC mechanism: anonymous pipes (one pair per child).**

Each child is fully autonomous:
1. Re-reads the input file and builds its own graph independently.
2. Runs Dijkstra for its own `src→dst` pair.
3. Walks the path node by node, sleeping `edge_weight × 300 ms` between steps.
4. On every node arrival writes one `IpcMsg` (current node, next node) to its pipe.
5. Exits normally when the destination is reached.

The parent reads these messages each frame (non-blocking via `select()`),
updates the GUI animation, and prints the log.

**Why pipes?**
- Simple and portable - no setup beyond `pipe()`.
- Each child has its own dedicated pair, so the parent always knows which child
  sent a message without needing an ID field.
- File descriptors close automatically on process exit.
- Works naturally with `select()` for fair interleaving.

```bash
make milestone5
./sim tests/testm5.txt
./sim tests/testm5b.txt
```

---

### Milestone 6 - Node Synchronization

**Requirement:** at most one traveler inside a node at any time.
Others wait outside the node. The 1-second station stay is the critical section.

**Synchronization mechanism: POSIX named semaphores.**

One binary semaphore (mutex, value=1) per graph node, named `/trainos_node_N`.

**Why named semaphores?**
- Work across independent processes without shared memory setup.
- `sem_wait()` / `sem_post()` provide atomic mutual exclusion.
- Persistent until explicitly unlinked - parent creates them before `fork()`
  and unlinks them after all children finish, so no leaks on clean exit.
- POSIX guarantees every blocked `sem_wait()` caller eventually wakes
  (no starvation), satisfying the professor's "every request eventually
  granted" requirement.

**Start barrier:** two additional named semaphores (`/trainos_bar_count`,
`/trainos_bar_gate`) implement a countdown latch. All children call
`barrier_wait()` after receiving `GO_SIGNAL`, so they all begin travelling
at the exact same instant. This guarantees simultaneous arrival at shared
bottleneck nodes and makes the waiting behaviour clearly visible.

**Protocol per node visit (child):**
1. `sem_trywait()` - if the node is free, enter immediately.
2. If locked: send `MSG_WAITING` to parent (GUI shows orange "W" badge),
   then `sem_wait()` - blocks until the current holder releases.
3. Send `MSG_AT_NODE` to parent, sleep 1 second (critical section).
4. `sem_post()` - release the lock for the next waiter.

**Entry order** among waiting travelers is non-deterministic (OS scheduling
decides). The professor's requirements state order is not enforced.

**GUI changes:**
- Waiting trains drawn in **orange** with a **"W" badge** outside the target node.
- Locked nodes get an **orange outer ring**.
- Legend shows **"[waiting]"** status.

```bash
make milestone6
./sim tests/testm6b.txt    # bottleneck demo - 3 travelers queued at node 3
./sim tests/testm6.txt     # general sync test
```

**Milestone 6 example terminal output (`testm6b.txt`):**
```
[PID=45194] arrived at node 0 | next node: 3
[PID=45195] arrived at node 1 | next node: 3
[PID=45196] arrived at node 2 | next node: 3
[PID=45194] arrived at node 3 | next node: 4    <- first in, holds lock
[PID=45196] waiting outside node 3              <- mutual exclusion working
[PID=45195] waiting outside node 3              <- 2 travelers queued outside
[PID=45196] arrived at node 3 | next node: 4    <- enters after 1 second
[PID=45194] arrived at node 4 | DESTINATION
[PID=45195] arrived at node 3 | next node: 4    <- enters after another second
[PID=45196] arrived at node 4 | DESTINATION
[PID=45195] arrived at node 4 | DESTINATION
[PID=45194] finished
[PID=45195] finished
[PID=45196] finished
```

**Self-check:**
- No two travelers are ever in the same node simultaneously.
- Every waiting traveler is eventually granted entry (no starvation).
- GUI reflects waiting state in real time with orange colour and "W" badge.
- No zombie processes - parent `waitpid()`s every child.
- All semaphores unlinked by parent on exit - no resource leaks.

---
 
### Milestone 7 - Scheduling Algorithms
 
**Goal:** Replace non-deterministic semaphore ordering (M6) with a
parent-managed scheduling policy. When multiple travelers queue outside the
same node, the parent decides who enters next based on the chosen algorithm.
 
**Run commands:**
```bash
make milestone7
./sim -schd fcfs tests/testm7b.txt    # FCFS demo
./sim -schd sjf  tests/testm7b.txt    # SJF demo - different admission order!
```
 
**Architecture change from M6:**
In M6, `sem_wait()` inside each child determined admission order
non-deterministically. In M7 the parent owns a `NodeQueue` per node
(`scheduler.c`) and controls admission explicitly.
 
Each child now has **three pipes**:
- `c2p` (child→parent): `IpcMsg` stream - `MSG_WAITING`, `MSG_AT_NODE`, `MSG_LEAVING`
- `p2c` (parent→child): `GO_SIGNAL` byte to start the journey
- `adm` (parent→child): `ADMIT_SIGNAL` byte to enter a specific node
**Protocol per node visit (M7 child):**
1. Send `MSG_WAITING` to parent and block on `read(admit_fd)`.
2. Parent calls `sched_request()` - if node is free, sends `ADMIT_SIGNAL` immediately;
   otherwise queues the request.
3. Child unblocks, sends `MSG_AT_NODE`, sleeps 1 second (critical section).
4. Child sends `MSG_LEAVING` and travels to the next node.
5. Parent calls `sched_release()` - picks the next waiter per the algorithm
   and sends it `ADMIT_SIGNAL`.
**New files:**
- `src/scheduler.c` / `include/scheduler.h` - `NodeQueue` struct,
  `sched_init()`, `sched_request()`, `sched_release()`, `sched_algo_name()`.
**Algorithms implemented:**
 
| Flag | Algorithm | Selection key | Tie-breaking |
|------|-----------|---------------|--------------|
| `fcfs` | First Come First Served | Earliest arrival sequence | - |
| `sjf`  | Shortest Job First | Fewest remaining hops | Earlier arrival |
 
**`testm7b.txt` demo - why FCFS and SJF differ:**
 
3 travelers all arrive at bottleneck node 3 at roughly the same time
(all start edges have weight 3 = 900 ms travel). After node 3 their
remaining paths to their destinations differ:
 
| Traveler | Route | Hops after node 3 |
|----------|-------|-------------------|
| T1 (0→7) | 0→3→4→5→6→7 | 4 |
| T2 (1→6) | 1→3→4→5→6   | 3 |
| T3 (2→4) | 2→3→4       | 1 |
 
- **FCFS at node 3:** T1 → T2 → T3 (wall-clock arrival order)
- **SJF  at node 3:** T1 → **T3** → T2 (T3 has fewest hops, jumps ahead of T2)
**How scheduling affects total wait time:**
- Under FCFS, T3 (1 hop left) waits behind T2 (3 hops) unnecessarily.
- Under SJF, T3 enters before T2, finishing ~2 seconds sooner. Longer jobs
  wait more, but overall throughput improves for short jobs.
**GUI change:** active scheduler name displayed in top-left corner.
Label is only shown in Milestone 7; earlier milestones show no label.
 
**Self-check:**
- Both algorithms compile and run without warnings.
- `make test-m7` demonstrates different admission orders back-to-back.
- No deadlocks - `MSG_LEAVING` is sent even at the destination node,
  ensuring waiters are never permanently blocked.
- No zombie processes, no resource leaks.
---
 
### Git Workflow
 
- Each milestone is developed on its own branch (`milestone1`, `milestone2`, …).
- Merge into `main` only when a milestone is complete and tested.
- Tag each milestone submission in the format `milestone1`, `milestone2`, etc.
- The **final submission** requires two tags: `milestone7` and `final`.
```bash
# After final commit:
git tag milestone7
git tag final
git push origin milestone7 final --tags
```
 