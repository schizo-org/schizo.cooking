CC = gcc
CFLAGS = -Wall -Wextra -std=c99
LDFLAGS =
OUTPUT_LIBRARY = libmarker.a
OBJECTS = src/marker.o
SRC = src/marker.c
HEADERS = src/marker.h

BUILD_DIR = build
TEST_DIR = tests
TEST_OBJECTS = $(TEST_DIR)/test_marker.o
TEST_EXEC = $(TEST_DIR)/marker_test

$(OUTPUT_LIBRARY): $(OBJECTS)
	ar rcs $(OUTPUT_LIBRARY) $(OBJECTS)

$(TEST_DIR)/test_marker.o: $(TEST_DIR)/test_marker.c $(HEADERS) $(OBJECTS)
	$(CC) $(CFLAGS) -c $(TEST_DIR)/test_marker.c -o $(TEST_DIR)/test_marker.o

test: $(OUTPUT_LIBRARY) $(TEST_OBJECTS)
	$(CC) $(CFLAGS) $(TEST_OBJECTS) $(OUTPUT_LIBRARY) -o $(TEST_EXEC)
	./$(TEST_EXEC)

clean:
	rm -rf $(BUILD_DIR) $(OUTPUT_LIBRARY) $(TEST_EXEC)

.PHONY: clean test
