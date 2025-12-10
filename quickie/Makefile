include ../common.mk

TARGET := quickie
SRC_DIR := .

QUICKIE_SRC := $(SRC_DIR)/quickie.c
QUICKIE_OBJ := $(SRC_DIR)/quickie.o

DEPS := $(IRIS_LIB) $(MARKER_LIB)

all: $(TARGET)

$(TARGET): $(QUICKIE_OBJ) $(DEPS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(QUICKIE_OBJ) $(IRIS_LIB) $(MARKER_LIB) -lpthread

$(QUICKIE_OBJ): $(QUICKIE_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(IRIS_LIB):
	$(MAKE) -C $(IRIS_DIR) libiris.a

$(MARKER_LIB):
	$(MAKE) -C $(MARKER_DIR) libmarker.a

install: $(TARGET)
	install -D $(TARGET) $(BINDIR)/$(TARGET)

uninstall:
	rm -f $(BINDIR)/$(TARGET)

clean:
	rm -f $(QUICKIE_OBJ) $(TARGET)

.PHONY: all clean install uninstall
