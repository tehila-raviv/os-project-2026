CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -Iinclude

# Uncomment when raylib is needed (Milestone 2+):
# RAYLIB_FLAGS = -lraylib -lm -lpthread -ldl -lrt -lX11

SRC_DIR = src
OBJ_DIR = obj

# Milestone 1 builds as ./dijkstra  (per technical requirements)
# Milestones 2+ build as ./sim
TARGET_M1 = dijkstra
TARGET_M2 = sim

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

all: milestone1

# Required target names (per professor's technical requirements doc)
milestone1: $(OBJ_DIR) $(TARGET_M1)

$(TARGET_M1): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET_M1) $(TARGET_M2)

run: milestone1
	./$(TARGET_M1) tests/test1.txt

valgrind: milestone1
	valgrind --leak-check=full --track-origins=yes ./$(TARGET_M1) tests/test1.txt

.PHONY: all clean run valgrind milestone1
