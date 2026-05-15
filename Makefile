CC     = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude -g

# raylib link flags (Milestone 2+)
RAYLIB_FLAGS = -lraylib -lm -lpthread -ldl -lrt -lX11

SRC_DIR = src
OBJ_DIR = obj

TARGET_M1 = dijkstra
TARGET_M2 = sim

# ── Source lists ──────────────────────────────────────────────
SRCS_M1 = $(SRC_DIR)/main.c \
           $(SRC_DIR)/graph.c \
           $(SRC_DIR)/dijkstra.c \
           $(SRC_DIR)/parser.c

SRCS_M4 = $(SRC_DIR)/main.c \
           $(SRC_DIR)/graph.c \
           $(SRC_DIR)/dijkstra.c \
           $(SRC_DIR)/parser.c \
           $(SRC_DIR)/renderer.c

OBJS_M1 = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/m1_%.o, $(SRCS_M1))
OBJS_M4 = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/m4_%.o, $(SRCS_M4))

# Default target
all: milestone4

# ── Milestone 1 (terminal only, no raylib) ────────────────────
milestone1: $(OBJ_DIR) $(TARGET_M1)

$(TARGET_M1): $(OBJS_M1)
	$(CC) $(CFLAGS) -o $@ $^ -lm

$(OBJ_DIR)/m1_%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# ── Milestones 2 / 3 / 4 (GUI + raylib) ──────────────────────
milestone2: $(OBJ_DIR) $(TARGET_M2)
milestone3: $(OBJ_DIR) $(TARGET_M2)
milestone4: $(OBJ_DIR) $(TARGET_M2)

$(TARGET_M2): $(OBJS_M4)
	$(CC) $(CFLAGS) -o $@ $^ $(RAYLIB_FLAGS)

$(OBJ_DIR)/m4_%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -DWITH_RAYLIB -c -o $@ $<

# ── Utility ───────────────────────────────────────────────────
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET_M1) $(TARGET_M2)

# ── Run shortcuts ─────────────────────────────────────────────
run: milestone4
	./$(TARGET_M2) tests/test_m4.txt

# ── Milestone 1 terminal tests ────────────────────────────────
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

# ── Milestone 4 GUI tests ─────────────────────────────────────
test-m4: milestone4
	./$(TARGET_M2) tests/test_m4.txt

test-m4-b: milestone4
	./$(TARGET_M2) tests/test_m4b.txt

# ── Valgrind (terminal binary, no raylib noise) ───────────────
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
        milestone1 milestone2 milestone3 milestone4 \
        test-m1 test-m4 test-m4-b valgrind