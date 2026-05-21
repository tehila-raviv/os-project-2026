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
│   ├── main.c        # Entry point - fork logic, signal handling, waitpid
│   ├── graph.c       # Adjacency-list directed weighted graph
│   ├── dijkstra.c    # Shortest path + custom min-heap
│   ├── parser.c      # Input file parsing (multi-traveler format)
│   └── renderer.c    # raylib GUI rendering & animation (M2+)
│
├── include/
│   ├── graph.h
│   ├── dijkstra.h
│   ├── parser.h
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
│   └── testm4b.txt   # Milestone 4 - 4 travelers (edge cases)
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
make milestone4    # builds ./sim       (GUI, animation, multi-process)
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

**Architecture:**

The parent process:
1. Reads the extended input file (graph + traveler list).
2. Computes the Dijkstra shortest path for every traveler.
3. `fork()`s one child process per traveler.
4. Runs the raylib GUI, animating all travelers simultaneously in distinct colours.
5. Sends `SIGTERM` to each child when its animation completes.
6. Calls `waitpid()` for every child before exiting.

Each child process:
1. Prints `[PID] started` immediately after creation.
2. Sleeps indefinitely (`sleep(3600)` loop) until `SIGTERM` arrives.

All traveler animations run in parallel - each train is drawn in a unique colour
(up to 16 distinct colours). The legend identifies each traveler by index and
shows its route. Arrived trains remain visible at their destination in a faded
style. Signal handling ensures clean shutdown on Ctrl+C with no zombie processes.

```bash
make milestone4
./sim tests/testm4.txt     # 3 travelers
./sim tests/testm4b.txt    # 4 travelers (includes src==dst and longer paths)
make test-m4
make test-m4-b
```

**Self-check:**
- `fork()` is called once per traveler - verify with `ps` or terminal output.
- Each child prints `[PID] started` before sleeping.
- All trains move concurrently (not sequentially).
- Parent calls `waitpid()` for every child - no zombie processes.
- Ctrl+C terminates all children cleanly.

---

### Milestone 5 - Inter-Process Communication
*Coming soon*

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
  git tag milestone4
  git push origin milestone4 --tags
  ```