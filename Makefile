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

# Milestone 2: all sources
SRCS_M2 = $(wildcard $(SRC_DIR)/*.c)
OBJS_M2 = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/m2_%.o, $(SRCS_M2))

all: milestone2

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

# ── Utility ──────────────────────────────────────────────
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET_M1) $(TARGET_M2)

run: milestone2
	./$(TARGET_M2) tests/test1.txt

valgrind: milestone1
	valgrind --leak-check=full --track-origins=yes \
	         ./$(TARGET_M1) tests/test1.txt

.PHONY: all clean run valgrind milestone1 milestone2