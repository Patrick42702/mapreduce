# Makefile for MapReduce project

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -pthread -g -fsanitize=address

# Source files
SRCS = main.c mapreduce.c

# Object files
OBJS = $(SRCS:.c=.o)

# Executable name
TARGET = mapreduce_app

# Default target
all: $(TARGET)

# Link objects into executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

# Compile .c into .o
%.o: %.c mapreduce.h
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean

