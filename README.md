# TrainOS - Railway Traffic Simulation

## Group Members & Roles

| Name | Scrum Role |
|------|------------|
| Tehila Raviv | Product Owner |
| Tal Zada | Scrum Master |
| Ori Azarzar | Dev Team |
| Orel Ben David | Dev Team |

## Project Description

TrainOS is a simulation of a railway traffic system modeled as a directed weighted graph.
Multiple trains (processes) travel simultaneously across a network of stations,
using OS mechanisms learned in the course: processes, inter-process communication,
synchronization, and scheduling.

Each station is a node in the graph and each railway track is a directed weighted edge,
where the weight represents the travel time between stations.
Trains compute the shortest route using Dijkstra's algorithm and travel along it,
stopping briefly at intermediate stations before reaching their destination.

## Project Structure

```
.
├── src/
│   ├── main.c          # Entry point
│   ├── graph.c         # Graph data structure (adjacency list)
│   ├── dijkstra.c      # Shortest path algorithm + min-heap
│   ├── parser.c        # Input file parsing & validation
│   └── renderer.c      # GUI rendering & animation (Milestone 2+)
│
├── include/
│   ├── graph.h
│   ├── dijkstra.h
│   ├── parser.h
│   └── renderer.h      # GUI types & constants (Milestone 2+)
│
├── tests/
│   ├── test1.txt       # Normal path
│   ├── test2.txt       # Disconnected graph (no path found)
│   ├── test3.txt       # Source == destination
│   ├── test4.txt       # Larger graph (10 nodes)
│   ├── test4b.txt      # Large graph (15 nodes, max supported)
│   ├── test5.txt       # Long path, many intermediate stops
│   ├── test6.txt       # Heavy weights vs short path
│   ├── test7.txt       # Single edge, minimal animation
│   ├── test8.txt       # src == dst inside a larger graph
│   └── test9.txt       # Disconnected graph, no animation
│
├── Makefile
├── .gitignore
└── README.md
```

## Dependencies

- GCC
- [raylib](https://www.raylib.com/) — required from Milestone 2 onward
  ```bash
  sudo apt install libraylib-dev
  ```

## Input File Format

```
N M          # N = number of stations, M = number of tracks
src dst w    # directed edge: station src -> dst with travel time w
...
src dst      # query: find shortest path from src to dst
```

---

## Milestones

### Milestone 1 — Graph Representation + Dijkstra

**Implementation:**
The graph is represented as a directed weighted adjacency list (`graph.c`).
Each node is a station and each edge is a track with a travel-time weight.
Dijkstra's algorithm is implemented using a custom min-heap (`dijkstra.c`) for efficient
shortest-path computation. The parser (`parser.c`) reads the input file, validates all
fields (negative weights, out-of-range vertices), builds the graph, and returns the
source/destination query. Results are printed to stdout in the required format.

**Build & Run:**
```bash
make milestone1
./dijkstra tests/test1.txt
```

**Test all Milestone 1 cases:**
```bash
make test-m1
```

---

### Milestone 2 — GUI: Static Graph Display

**Implementation:**
A graphical window is opened using raylib (`renderer.c`).
All stations are laid out in a circular arrangement on screen.
Directed edges are drawn as arrows with weight labels.
The shortest path computed by Dijkstra is highlighted in green.
Source and destination nodes are colour-coded (white and amber).
A legend and route info bar are displayed.

**Build & Run:**
```bash
make milestone2
./sim tests/test1.txt
```

---

### Milestone 3 — Movement Animation

**Implementation:**
A train (red circle labelled "T") animates along the Dijkstra shortest path.
Each edge with weight W is traversed in W × 300ms, giving travel time proportional
to edge weight. The train pauses for 1 second at each intermediate station before
continuing. A PLAY/STOP button (top-right) controls the animation — the train starts
paused and only moves after the user presses PLAY. When the train reaches the
destination a centred "Train has arrived!" overlay is shown with the route and total
cost. A live status bar (bottom-right) shows the current phase and jump progress.
Edge cases handled: disconnected graph (no animation), src == dst (arrived immediately).

**Build & Run:**
```bash
make milestone3
./sim tests/test1.txt
```

**Run individual animation tests:**
```bash
make test-m3-5   # long path, many stops
make test-m3-6   # heavy weights
make test-m3-7   # single edge
make test-m3-8   # src == dst
make test-m3-9   # disconnected graph
```

---

### Milestone 4 — Multiple Processes + Parent Process
*Coming soon*

**Build & Run:**
```bash
make milestone4
./sim tests/test1.txt
```

---

### Milestone 5 — Inter-Process Communication
*Coming soon*

**Build & Run:**
```bash
make milestone5
./sim tests/test1.txt
```

---

### Milestone 6 — Synchronization
*Coming soon*

**Build & Run:**
```bash
make milestone6
./sim tests/test1.txt
```

---

### Milestone 7 — Scheduling Algorithms
*Coming soon*

**Build & Run:**
```bash
make milestone7
./sim -schd fcfs tests/test1.txt
./sim -schd sjf  tests/test1.txt
```

---

## Clean

```bash
make clean
```

## Git Workflow

- Each milestone is developed on its own branch (`milestone1`, `milestone2`, etc.)
- Merge into `main` only when a milestone is complete and tested
- Each milestone is tagged before submission:
  ```bash
  git tag milestoneN
  git push origin milestoneN --tags
  ```