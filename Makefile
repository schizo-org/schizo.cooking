CC := gcc
CFLAGS := -Wall -Wextra -pedantic -O3
TARGET := iris
SRCS := $(wildcard src/*.c)
OBJS := $(SRCS:.c=.o)
PREFIX ?= /usr/local
BINDIR := $(PREFIX)/bin

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	install -D $(TARGET) $(BINDIR)/$(TARGET)

uninstall:
	rm -f $(BINDIR)/$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean install uninstall
