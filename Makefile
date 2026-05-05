CC     = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude -g

# raylib link flags (Milestone 2+)
RAYLIB_FLAGS = -lraylib -lm -lpthread -ldl -lrt -lX11

SRC_DIR = src
OBJ_DIR = obj

TARGET_M1 = dijkstra
TARGET_M2 = sim

# Milestone 1: no renderer.c
SRCS_M1 = $(SRC_DIR)/main.c $(SRC_DIR)/graph.c $(SRC_DIR)/dijkstra.c $(SRC_DIR)/parser.c
OBJS_M1 = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/m1_%.o, $(SRCS_M1))

# Milestone 2 & 3: all sources
SRCS_M2 = $(wildcard $(SRC_DIR)/*.c)
OBJS_M2 = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/m2_%.o, $(SRCS_M2))

all: milestone3

# ── Milestone 1 ──────────────────────────────────────────
milestone1: $(OBJ_DIR) $(TARGET_M1)

$(TARGET_M1): $(OBJS_M1)
	$(CC) $(CFLAGS) -o $@ $^ -lm

$(OBJ_DIR)/m1_%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# ── Milestone 2 ──────────────────────────────────────────
milestone2: $(OBJ_DIR) $(TARGET_M2)

$(TARGET_M2): $(OBJS_M2)
	$(CC) $(CFLAGS) -o $@ $^ $(RAYLIB_FLAGS)

$(OBJ_DIR)/m2_%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -DWITH_RAYLIB -c -o $@ $<

# ── Milestone 3 (same binary as milestone 2, explicit target for clarity) ──
milestone3: $(OBJ_DIR) $(TARGET_M2)

# ── Utility ──────────────────────────────────────────────
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET_M1) $(TARGET_M2)

# ── Run shortcuts ─────────────────────────────────────────
run: milestone3
	./$(TARGET_M2) tests/test1.txt

# Milestone 1 terminal-only tests (no GUI, fast, pipe-friendly)
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

# Milestone 3 GUI tests - launch each one individually so you can observe animation
test-m3-5: milestone3
	./$(TARGET_M2) tests/test5.txt

test-m3-6: milestone3
	./$(TARGET_M2) tests/test6.txt

test-m3-7: milestone3
	./$(TARGET_M2) tests/test7.txt

test-m3-8: milestone3
	./$(TARGET_M2) tests/test8.txt

test-m3-9: milestone3
	./$(TARGET_M2) tests/test9.txt

# Milestone 1 core logic — no raylib noise, clean output
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

# GUI binary — close the window after train arrives, then check the summary
# "definitely lost: 0" = your code is clean; ignore raylib/system noise
valgrind-gui: milestone3
	valgrind --leak-check=full --track-origins=yes \
	         ./$(TARGET_M2) tests/test1.txt 2>&1 | tail -20

.PHONY: all clean run \
        valgrind valgrind-gui \
        milestone1 milestone2 milestone3 \
        test-m1 \
        test-m3-5 test-m3-6 test-m3-7 test-m3-8 test-m3-9