# TrainOS - Railway Traffic Simulation

## Group Members & Roles

| Name | Scrum Role |
|------|------------|
| Tehila Raviv | Product Owner |
| Tal Zada | Scrum Master |
| Ori Azarzar | Dev Team |
| Orel Ben David | Dev Team |

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
│   ├── main.c        # Entry point - fork, IPC drain (select), waitpid
│   ├── graph.c       # Adjacency-list directed weighted graph
│   ├── dijkstra.c    # Shortest path + custom min-heap
│   ├── parser.c      # Input file parsing (multi-traveler format)
│   └── renderer.c    # raylib GUI rendering & animation (M2+)
│
├── include/
│   ├── graph.h
│   ├── dijkstra.h
│   ├── parser.h
│   ├── ipc.h         # IpcMsg struct, MAX_TRAVELERS, MS_PER_JUMP
│   └── renderer.h
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
│   ├── testm5.txt    # Milestone 5 - professor's example (2 travelers)
│   └── testm5b.txt   # Milestone 5 - 3 travelers (edge cases)
│
├── Makefile
└── README.md
```

## Input File Format

### Milestones 1–3 (single traveler - for ./dijkstra only)
```
N M          # N stations, M tracks
src dst w    # directed edge with travel-time weight w
...
src dst      # Dijkstra query
```

### Milestone 4+ (multiple travelers - for ./sim)
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
- [raylib](https://www.raylib.com/) - required from Milestone 2 onward
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
make clean
```

---

## Milestones

### Milestone 1 - Graph + Dijkstra

Directed weighted adjacency list (`graph.c`). Dijkstra with a custom min-heap
(`dijkstra.c`). Parser validates negative weights and out-of-range vertices.

```bash
make milestone1
./dijkstra tests/test1.txt
make test-m1      # run all M1 tests
make valgrind     # memory-leak check
```

---

### Milestone 2 - Static GUI

raylib window with circular node layout, directed-edge arrows, weight labels,
path highlighting, and a colour-coded legend.

```bash
make milestone2
./sim tests/testm4.txt
```

---

### Milestone 3 - Animation

Animated train circles travel along the Dijkstra path. Travel time proportional
to edge weight (300 ms/unit). 1-second pause at intermediate stations.
PLAY/STOP button, arrival overlays, live status bar.

```bash
make milestone3
./sim tests/testm4.txt
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
make test-m4
make test-m4-b
```

---

### Milestone 5 - Inter-Process Communication (IPC)

**IPC mechanism chosen: pipes (one anonymous pipe per child).**

**Why pipes?**
- Simple and portable - no setup beyond `pipe()`.
- Each child has its own dedicated pipe so the parent always knows which child
  sent a message without needing an ID field in the packet.
- File descriptors close automatically when a process exits - no cleanup needed.
- Works naturally with `select()` for fair interleaving of messages from
  multiple children in the terminal log loop.

**Architecture change from Milestone 4:**

In Milestone 4 the parent computed all paths and the children only slept.
In Milestone 5 each child is fully autonomous:
1. Re-reads the input file and builds its own graph independently.
2. Runs Dijkstra for its own `src->dst` pair.
3. Walks the path node by node, sleeping `edge_weight × 300 ms` between steps.
4. On every node arrival, writes one `IpcMsg` (current node, next node) to its pipe.
5. Exits normally when the destination is reached.

The parent reads these messages each frame (non-blocking), updates the GUI
animation, and prints the log. Children no longer print anything.

**Terminal log format (parent prints all output):**
```
[PID=1021] arrived at node 0 | next node: 2
[PID=1022] arrived at node 2 | next node: 1
[PID=1021] arrived at node 2 | next node: 1
[PID=1022] arrived at node 1 | next node: 3
[PID=1021] arrived at node 1 | next node: 4
[PID=1022] arrived at node 3 | DESTINATION
[PID=1021] arrived at node 4 | DESTINATION
[PID=1022] finished
[PID=1021] finished
```

```bash
make milestone5
./sim tests/testm5.txt     # professor's example (2 travelers)
./sim tests/testm5b.txt    # edge cases (3 travelers)
make test-m5
make test-m5-b
```

**Self-check:**
- Path data is never passed from parent to child - each child computes its own.
- Every node arrival generates exactly one log line in the terminal.
- `[PID=X] finished` appears for every child after `waitpid()`.
- No zombie processes - parent waits for all children.

---

### Milestone 6 - Synchronization
*Coming soon*

### Milestone 7 - Scheduling Algorithms
*Coming soon*

---

## Git Workflow

- Each milestone is developed on its own branch (`milestone1`, `milestone2`, …).
- Merge into `main` only when a milestone is complete and tested.
- Tag each submission:
  ```bash
  git tag milestone5
  git push origin milestone5 --tags
  ```