# Makefile for Network Slice PRB Allocator

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -g
LDFLAGS = -lm

# Source files
SRC = slice_prb_allocator.c
TEST_SRC = test_slice_prb_allocator.c
TEST_TARGET = test_slice_prb_allocator

# Default target
all: test

# Build test executable
test: $(TEST_TARGET)

$(TEST_TARGET): $(SRC) $(TEST_SRC)
	$(CC) $(CFLAGS) $(TEST_SRC) $(SRC) -o $(TEST_TARGET) $(LDFLAGS)

# Run tests
run: test
	./$(TEST_TARGET)

# Clean build artifacts
clean:
	rm -f $(TEST_TARGET)

# Phony targets
.PHONY: all test run clean
