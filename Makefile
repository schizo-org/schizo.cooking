CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -std=c99

CLANG_TIDY ?= clang-tidy
CPPCHECK ?= cppcheck
CLANG_FORMAT ?= clang-format

ROOT := $(CURDIR)
IRIS_DIR := $(ROOT)/iris
MARKER_DIR := $(ROOT)/marker
QUICKIE_DIR := $(ROOT)/quickie

IRIS_SRC := $(IRIS_DIR)/src/iris.c
IRIS_MAIN := $(IRIS_DIR)/src/main.c
IRIS_OBJ := $(IRIS_DIR)/src/iris.o
IRIS_BIN := $(IRIS_DIR)/iris

MARKER_SRC := $(MARKER_DIR)/src/marker.c
MARKER_OBJ := $(MARKER_DIR)/src/marker.o
MARKER_LIB := $(MARKER_DIR)/libmarker.a

QUICKIE_SRC := $(QUICKIE_DIR)/quickie.c
QUICKIE_BIN := $(QUICKIE_DIR)/quickie

INCLUDES := -I$(IRIS_DIR)/src -I$(MARKER_DIR)/src

.PHONY: all clean iris marker quickie test lint format

all: iris marker quickie

iris: $(IRIS_BIN)

$(IRIS_BIN): $(IRIS_SRC) $(IRIS_MAIN) $(IRIS_DIR)/src/iris.h
	$(CC) $(CFLAGS) -o $@ $(IRIS_SRC) $(IRIS_MAIN)

marker: $(MARKER_LIB)

$(MARKER_OBJ): $(MARKER_SRC) $(MARKER_DIR)/src/marker.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(MARKER_LIB): $(MARKER_OBJ)
	ar rcs $@ $^

quickie: $(QUICKIE_BIN)

$(QUICKIE_BIN): $(QUICKIE_SRC) $(IRIS_OBJ) $(MARKER_OBJ) $(IRIS_DIR)/src/iris.h $(MARKER_DIR)/src/marker.h
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(QUICKIE_SRC) $(IRIS_SRC) $(MARKER_SRC)

test:
	$(CC) $(CFLAGS) -I$(MARKER_DIR)/src -o $(MARKER_DIR)/tests/test_marker $(MARKER_DIR)/tests/test_marker.c $(MARKER_SRC)
	$(MARKER_DIR)/tests/test_marker

lint:
	$(CLANG_TIDY) iris/src/*.c marker/src/*.c quickie/*.c -- -Iiris/src -Imarker/src || true
	$(CPPCHECK) --enable=all --inconclusive --std=c99 iris/src marker/src quickie || true

format:
	$(CLANG_FORMAT) -i iris/src/*.c iris/src/*.h marker/src/*.c marker/src/*.h quickie/*.c

clean:
	rm -f $(IRIS_BIN) $(IRIS_OBJ)
	rm -f $(MARKER_OBJ) $(MARKER_LIB)
	rm -f $(QUICKIE_BIN)
	rm -f $(MARKER_DIR)/tests/test_marker
