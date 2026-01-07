# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Wshadow -Wconversion -pthread -g -fsanitize=address

# ======================
# Main MapReduce program
# ======================
MAIN_SRCS = main.c mapreduce.c
MAIN_OBJS = $(MAIN_SRCS:.c=.o)
MAIN_TARGET = mapreduce_app

# ======================
# Test executable
# ======================
TEST_SRCS = test_mapreduce.c mapreduce.c
TEST_OBJS = $(TEST_SRCS:.c=.o)
TEST_TARGET = test_mapreduce

# Default target
all: $(MAIN_TARGET)

# Build main program
$(MAIN_TARGET): $(MAIN_OBJS)
	$(CC) $(CFLAGS) -o $@ $(MAIN_OBJS)

# Build test program
test: $(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJS)

# Compile .c to .o
%.o: %.c mapreduce.h
	$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	rm -f $(MAIN_OBJS) $(TEST_OBJS) $(MAIN_TARGET) $(TEST_TARGET)

.PHONY: all test clean

