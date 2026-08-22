#include "stdafx.h"

void serial_open(const char *path) {
	fatal("Serial I/O is not supported on this platform.");
}

void serial_close() {
}

void serial_write(const void *data, uint32_t len) {
}

uint32_t serial_tryread(void *buf, uint32_t maxlen, uint32_t timeout) {
	return 0;
}

void serial_read(void *buf, uint32_t len) {
}
