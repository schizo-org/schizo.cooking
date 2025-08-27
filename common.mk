CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=c99 -O2
AR ?= ar
CLANG_TIDY ?= clang-tidy
CPPCHECK ?= cppcheck
CLANG_FORMAT ?= clang-format

PREFIX ?= /usr/local
BINDIR := $(PREFIX)/bin
LIBDIR := $(PREFIX)/lib
INCLUDEDIR := $(PREFIX)/include

ARFLAGS := rcs

ROOT_DIR := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))
IRIS_DIR := $(ROOT_DIR)/iris
MARKER_DIR := $(ROOT_DIR)/marker
QUICKIE_DIR := $(ROOT_DIR)/quickie

IRIS_LIB := $(IRIS_DIR)/libiris.a
MARKER_LIB := $(MARKER_DIR)/libmarker.a

INCLUDES := -I$(IRIS_DIR)/src -I$(MARKER_DIR)/src

.PHONY: all clean install uninstall test lint format
