CC ?= cc
AR ?= ar
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic
CPPFLAGS += -Iinclude
SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=build/%.o)

.PHONY: all test clean
all: build/libdtc_components.a build/dtc_tests
build:
	mkdir -p build
build/%.o: src/%.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/libdtc_components.a: $(OBJ)
	$(AR) rcs $@ $^
build/dtc_tests: tests/test_main.c build/libdtc_components.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $< build/libdtc_components.a -lm -o $@
test: build/dtc_tests
	./build/dtc_tests
clean:
	$(RM) $(OBJ) build/libdtc_components.a build/dtc_tests
