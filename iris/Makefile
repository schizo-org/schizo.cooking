include ../common.mk

TARGET := iris
LIB_TARGET := libiris.a
SRC_DIR := src

MAIN_SRC := $(SRC_DIR)/main.c
IRIS_SRC := $(SRC_DIR)/iris.c
IRIS_HDR := $(SRC_DIR)/iris.h

MAIN_OBJ := $(SRC_DIR)/main.o
IRIS_OBJ := $(SRC_DIR)/iris.o

all: $(TARGET) $(LIB_TARGET)

$(TARGET): $(MAIN_OBJ) $(IRIS_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

$(LIB_TARGET): $(IRIS_OBJ)
	$(AR) $(ARFLAGS) $@ $^

$(MAIN_OBJ): $(MAIN_SRC) $(IRIS_HDR)
	$(CC) $(CFLAGS) -c $< -o $@

$(IRIS_OBJ): $(IRIS_SRC) $(IRIS_HDR)
	$(CC) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	install -D $(TARGET) $(BINDIR)/$(TARGET)

uninstall:
	rm -f $(BINDIR)/$(TARGET)

clean:
	rm -f $(MAIN_OBJ) $(IRIS_OBJ) $(TARGET) $(LIB_TARGET)

.PHONY: all clean install uninstall
