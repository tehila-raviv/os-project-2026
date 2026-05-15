# TrainOS - Railway Traffic Simulation

## Group Members & Roles

| Name | Scrum Role |
|------|------------|
| Tehila Raviv | Product Owner |
| Tal Zada | Scrum Master |
| Ori Azarzar | Dev Team |
| Orel Ben David | Dev Team |

## Project Description

TrainOS is a simulation of a railway traffic system modelled as a directed weighted graph.
Multiple trains (processes) travel simultaneously across a network of stations,
using OS mechanisms learned in the course: processes, inter-process communication,
synchronisation, and scheduling.

Each station is a node in the graph and each railway track is a directed weighted edge
whose weight represents travel time. Trains compute the shortest route using Dijkstra's
algorithm and travel along it, pausing briefly at intermediate stations.

## Project Structure

```
.
├── src/
│   ├── main.c          # Entry point (fork logic from Milestone 4)
│   ├── graph.c         # Adjacency-list graph
│   ├── dijkstra.c      # Shortest path + min-heap
│   ├── parser.c        # Input file parsing (single & multi-traveler)
│   └── renderer.c      # GUI rendering & animation (Milestone 2+)
│
├── include/
│   ├── graph.h
│   ├── dijkstra.h
│   ├── parser.h        # Extended with TravelerQuery from Milestone 4
│   └── renderer.h      # Extended for multiple travelers from Milestone 4
│
├── tests/
│   ├── test1.txt       # Single traveler – normal path
│   ├── test2.txt       # Single traveler – disconnected graph
│   ├── test3.txt       # Single traveler – src == dst
│   ├── test4.txt       # Single traveler – larger graph (10 nodes)
│   ├── test4b.txt      # Single traveler – 15 nodes
│   ├── test5.txt       # Long path, many stops
│   ├── test6.txt       # Heavy weights vs short path
│   ├── test7.txt       # Single edge, minimal animation
│   ├── test8.txt       # src == dst inside a larger graph
│   ├── test9.txt       # Disconnected graph
│   ├── test10.txt      # Graph with a cycle
│   ├── testm4.txt     # Milestone 4 – 3 travelers
│   └── tesm4b.txt    # Milestone 4 – 4 travelers (incl. edge cases)
│
├── Makefile
└── README.md
```

## Dependencies

- GCC
- [raylib](https://www.raylib.com/) -required from Milestone 2 onward
  ```bash
  sudo apt install libraylib-dev
  ```

## Input File Format

### Milestones 1–3 (single traveler)
```
N M          # N = stations, M = tracks
src dst w    # directed edge with travel-time weight w
...
src dst      # Dijkstra query
```

### Milestone 4+ (multiple travelers)
```
# graph definition   ← optional comment
N M
src dst w
...
# travelers          ← optional comment
K                    # number of travelers
src dst              # K traveler queries
...
```

---

## Milestones

### Milestone 1 -Graph Representation + Dijkstra

Directed weighted adjacency list (`graph.c`). Dijkstra with a custom min-heap
(`dijkstra.c`). Parser validates negative weights and out-of-range vertices.

```bash
make milestone1
./dijkstra tests/test1.txt
make test-m1          # run all terminal tests
```

---

### Milestone 2 -GUI: Static Graph Display

raylib window with circular node layout, directed-edge arrows, weight labels,
path highlighting, and a colour-coded legend.

```bash
make milestone2
./sim tests/test1.txt
```

---

### Milestone 3 -Movement Animation

Animated train (red circle "T") travels along the Dijkstra path. Travel time is
proportional to edge weight (300 ms per weight unit). 1-second pause at each
intermediate station. PLAY/STOP button, arrival overlay, live status bar.

```bash
make milestone3
./sim tests/test1.txt
```

---

### Milestone 4 -Multiple Processes + Parent Process

**Architecture:**

The parent process:
1. Reads the extended input file (graph + traveler list).
2. Computes the Dijkstra shortest path for every traveler.
3. `fork()`s one child process per traveler.
4. Runs the raylib GUI, animating all travelers simultaneously in distinct colours.
5. Sends `SIGTERM` to a child process when its traveler's animation completes.
6. Calls `waitpid()` for every child before exiting.

Each child process:
1. Prints `[PID] started` immediately after creation.
2. Sleeps indefinitely (`sleep(3600)` loop) until `SIGTERM` arrives.

All traveler animations run in parallel -each train is drawn in a unique colour
(up to 16 distinct colours). The legend identifies each traveler by index and
shows its route. "Train N arrived" overlays appear in the bottom-left corner as
each traveler finishes.

```bash
make milestone4
./sim tests/test_m4.txt    # 3 travelers
./sim tests/test_m4b.txt   # 4 travelers (includes src==dst and longer paths)
```

**Self-check:**
- `fork()` is called once per traveler -verify with `ps` or terminal output.
- Each child prints `[PID] started` before sleeping.
- All trains move concurrently (not sequentially).
- Parent calls `waitpid()` for every child -no zombie processes.

---

### Milestone 5 -Inter-Process Communication
*Coming soon*

---

### Milestone 6 -Synchronization
*Coming soon*

---

### Milestone 7 -Scheduling Algorithms
*Coming soon*

---

## Build & Clean

```bash
make            # builds milestone4 (default)
make clean      # removes obj/ and binaries
```

## Git Workflow

- Each milestone is developed on its own branch (`milestone1`, `milestone2`, …).
- Merge into `main` only when a milestone is complete and tested.
- Tag each submission:
  ```bash
  git tag milestone4
  git push origin milestone4 --tags
  ```