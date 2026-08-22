#include "stdafx.h"

void fatal(const char *msg) {
	puts(msg);
	exit(10);
}

void fatalf(const char *msg, ...) {
	va_list val;
	va_start(val, msg);
	vprintf(msg, val);
	va_end(val);
	exit(10);
}

void fatal_read() {
	fatalf("Unable to read from input file: %s.\n", g_inputPath.c_str());
}
