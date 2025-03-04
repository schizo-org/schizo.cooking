CC := gcc
CFLAGS := -Wall -Wextra -pedantic -O3

TARGET := iris
SRC_DIR := src
MAIN_SRC := $(SRC_DIR)/main.c
IRIS_SRC := $(SRC_DIR)/iris.c
IRIS_HDR := $(SRC_DIR)/iris.h

OBJS := $(IRIS_SRC:.c=.o) $(MAIN_SRC:.c=.o)

TEST_SRCS := $(wildcard tests/*.c)
TEST_OBJS := $(TEST_SRCS:.c=.o)
TEST_TARGET := test_iris

PREFIX ?= /usr/local
BINDIR := $(PREFIX)/bin

all: $(TARGET) $(TEST_TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_TARGET): $(OBJS) $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	install -D $(TARGET) $(BINDIR)/$(TARGET)

uninstall:
	rm -f $(BINDIR)/$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) $(TEST_OBJS) $(TEST_TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

.PHONY: all clean install uninstall test
