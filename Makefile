include common.mk

SUBDIRS := marker iris quickie

all: $(SUBDIRS)

marker:
	$(MAKE) -C marker

iris: marker
	$(MAKE) -C iris

quickie: iris marker
	$(MAKE) -C quickie

install: all
	$(MAKE) -C iris install
	$(MAKE) -C quickie install

uninstall:
	$(MAKE) -C iris uninstall
	$(MAKE) -C quickie uninstall

test:
	$(MAKE) -C marker test

lint:
	$(CLANG_TIDY) iris/src/*.c marker/src/*.c quickie/*.c -- $(INCLUDES) || true
	$(CPPCHECK) --enable=all --inconclusive --std=c99 iris/src marker/src quickie || true

format:
	$(CLANG_FORMAT) -i iris/src/*.c iris/src/*.h marker/src/*.c marker/src/*.h quickie/*.c

clean:
	$(MAKE) -C marker clean
	$(MAKE) -C iris clean
	$(MAKE) -C quickie clean

.PHONY: all install uninstall test lint format clean $(SUBDIRS)
