CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -std=c11

all: uft_protection_test

uft_protection_test: protection_analyzer.c protection_analyzer.h uft_protection_test.c
	$(CC) $(CFLAGS) protection_analyzer.c uft_protection_test.c -o $@

clean:
	rm -f uft_protection_test
