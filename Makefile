CC     = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude -g

RAYLIB_FLAGS = -lraylib -lm -lpthread -ldl -lrt -lX11

SRC_DIR = src
OBJ_DIR = obj

TARGET_M1 = dijkstra
TARGET_M2 = sim

SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/graph.c \
       $(SRC_DIR)/dijkstra.c \
       $(SRC_DIR)/parser.c \
       $(SRC_DIR)/renderer.c

SRCS_M1 = $(SRC_DIR)/main.c \
           $(SRC_DIR)/graph.c \
           $(SRC_DIR)/dijkstra.c \
           $(SRC_DIR)/parser.c

OBJS_M1 = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/m1_%.o,  $(SRCS_M1))
OBJS_M2 = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/m2_%.o,  $(SRCS))
OBJS_M3 = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/m3_%.o,  $(SRCS))
OBJS_M4 = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/m4_%.o,  $(SRCS))
OBJS_M5 = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/m5_%.o,  $(SRCS))

all: milestone5

# ── Milestone 1: terminal only, single traveler ───────────────
milestone1: $(OBJ_DIR) $(OBJS_M1)
	$(CC) $(CFLAGS) -o $(TARGET_M1) $(OBJS_M1) -lm

$(OBJ_DIR)/m1_%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -DMILESTONE1 -c -o $@ $<

# ── Milestone 2: GUI static graph, single traveler ────────────
milestone2: $(OBJ_DIR) $(OBJS_M2)
	$(CC) $(CFLAGS) -o $(TARGET_M2) $(OBJS_M2) $(RAYLIB_FLAGS)

$(OBJ_DIR)/m2_%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -DMILESTONE2 -DWITH_RAYLIB -c -o $@ $<

# ── Milestone 3: GUI + animation, single traveler ────────────
milestone3: $(OBJ_DIR) $(OBJS_M3)
	$(CC) $(CFLAGS) -o $(TARGET_M2) $(OBJS_M3) $(RAYLIB_FLAGS)

$(OBJ_DIR)/m3_%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -DMILESTONE3 -DWITH_RAYLIB -c -o $@ $<

# ── Milestone 4: multi-process, no IPC ───────────────────────
milestone4: $(OBJ_DIR) $(OBJS_M4)
	$(CC) $(CFLAGS) -o $(TARGET_M2) $(OBJS_M4) $(RAYLIB_FLAGS)

$(OBJ_DIR)/m4_%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -DMILESTONE4 -DWITH_RAYLIB -c -o $@ $<

# ── Milestone 5: multi-process + IPC pipes ───────────────────
milestone5: $(OBJ_DIR) $(OBJS_M5)
	$(CC) $(CFLAGS) -o $(TARGET_M2) $(OBJS_M5) $(RAYLIB_FLAGS)

$(OBJ_DIR)/m5_%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -DMILESTONE5 -DWITH_RAYLIB -c -o $@ $<

# ── Utility ───────────────────────────────────────────────────
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET_M1) $(TARGET_M2)

# ── Run shortcuts ─────────────────────────────────────────────
run: milestone5
	./$(TARGET_M2) tests/testm5.txt

# ── Per-milestone test shortcuts ──────────────────────────────
test-m1: milestone1
	@echo "=== test1: normal path ==="
	./$(TARGET_M1) tests/test1.txt
	@echo ""
	@echo "=== test2: disconnected graph ==="
	./$(TARGET_M1) tests/test2.txt
	@echo ""
	@echo "=== test3: src == dst ==="
	./$(TARGET_M1) tests/test3.txt
	@echo ""
	@echo "=== test4: larger graph ==="
	./$(TARGET_M1) tests/test4.txt

test-m2: milestone2
	./$(TARGET_M2) tests/test1.txt

test-m3: milestone3
	./$(TARGET_M2) tests/test1.txt

test-m4: milestone4
	./$(TARGET_M2) tests/testm4.txt

test-m4-b: milestone4
	./$(TARGET_M2) tests/testm4b.txt

test-m5: milestone5
	./$(TARGET_M2) tests/testm5.txt

test-m5-b: milestone5
	./$(TARGET_M2) tests/testm5b.txt

# ── Valgrind ──────────────────────────────────────────────────
valgrind: milestone1
	@echo "=== valgrind test1 ==="
	valgrind --leak-check=full --track-origins=yes --error-exitcode=1 \
	         ./$(TARGET_M1) tests/test1.txt
	@echo ""
	@echo "=== valgrind test2 ==="
	valgrind --leak-check=full --track-origins=yes --error-exitcode=1 \
	         ./$(TARGET_M1) tests/test2.txt
	@echo ""
	@echo "=== valgrind test3 ==="
	valgrind --leak-check=full --track-origins=yes --error-exitcode=1 \
	         ./$(TARGET_M1) tests/test3.txt
	@echo ""
	@echo "=== valgrind test4 ==="
	valgrind --leak-check=full --track-origins=yes --error-exitcode=1 \
	         ./$(TARGET_M1) tests/test4.txt

.PHONY: all clean run \
        milestone1 milestone2 milestone3 milestone4 milestone5 \
        test-m1 test-m2 test-m3 test-m4 test-m4-b test-m5 test-m5-b valgrind