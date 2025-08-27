include ../common.mk

TARGET := libmarker.a
SRC_DIR := src
TEST_DIR := tests

MARKER_SRC := $(SRC_DIR)/marker.c
MARKER_HDR := $(SRC_DIR)/marker.h
MARKER_OBJ := $(SRC_DIR)/marker.o

TEST_SRC := $(TEST_DIR)/test_marker.c
TEST_OBJ := $(TEST_DIR)/test_marker.o
TEST_TARGET := $(TEST_DIR)/test_marker

all: $(TARGET)

$(TARGET): $(MARKER_OBJ)
	$(AR) $(ARFLAGS) $@ $^

$(MARKER_OBJ): $(MARKER_SRC) $(MARKER_HDR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_OBJ): $(TEST_SRC) $(MARKER_HDR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_TARGET): $(TEST_OBJ) $(TARGET)
	$(CC) $(CFLAGS) $< $(TARGET) -o $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -f $(MARKER_OBJ) $(TARGET) $(TEST_OBJ) $(TEST_TARGET)

.PHONY: all clean test
