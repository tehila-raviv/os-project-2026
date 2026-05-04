# TrainOS - Railway Traffic Simulation

## Group Members & Roles

| Name | Scrum Role 
|------|------------
| Tehila Raviv | Product Owner 
| Tal Zada | Scrum Master 
| Ori Azarzar | Dev Team 
| Orel Ben David | Dev Team 

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
|   └── renderer.c      # Raylib-based graph visualization
│  
├── include/
│   ├── graph.h
│   ├── dijkstra.h
│   ├── parser.h
│   └── renderer.h
│   
├── tests/
│   ├── test1.txt       # Normal path
│   ├── test2.txt       # Disconnected graph (No path found)
│   ├── test3.txt       # Source == destination
|   └── test4.txt       # Large complex graph (10 stations)
├── Makefile            # Milestone-specific build targets
├── .gitignore
└── README.md
```

## How to Build & Run

### Dependencies
- GCC
- [raylib](https://www.raylib.com/) - required from Milestone 2 onward
  ```bash
  sudo apt install libraylib-dev
  ```

### Build & Run - Per Milestone

| Milestone | Build | Run |
|-----------|-------|-----|
| 1 | `make milestone1` | `./dijkstra <file_name>` |
| 2 | `make milestone2` | `./sim <file_name>` |
| 3 | `make milestone3` | `./sim <file_name>` |
| 4 | `make milestone4` | `./sim <file_name>` |
| 5 | `make milestone5` | `./sim <file_name>` |
| 6 | `make milestone6` | `./sim <file_name>` |
| 7 | `make milestone7` | `./sim -schd fcfs <file_name>` / `./sim -schd sjf <file_name>` |

### Example - Milestone 1
```bash
make milestone1
./dijkstra tests/test1.txt
```

### Example - Milestone 2
```bash
make milestone2
./sim tests/test4.txt
```

### Input File Format
```
N M          # N = number of stations, M = number of tracks
src dst w    # directed edge: station src -> dst with travel time w
...
src dst      # query: find shortest path from src to dst
```

### Clean
```bash
make clean
```

## Milestones

| Milestone | Topic | Tag |
|-----------|-------|-----|
| 1 | Graph representation + Dijkstra | `milestone1` |
| 2 | GUI - static graph display | `milestone2` |
| 3 | Movement animation | `milestone3` |
| 4 | Multiple processes + parent process | `milestone4` |
| 5 | Inter-process communication | `milestone5` |
| 6 | Synchronization | `milestone6` |
| 7 | Scheduling algorithms | `milestone7` |

## Git Workflow

- All work is done on the `milestone1` branch (and future milestone branches)
- Merge into `main` only when a milestone is complete and tested
- Each milestone must be tagged before submission: `git tag milestoneN && git push origin milestoneN --tags`
