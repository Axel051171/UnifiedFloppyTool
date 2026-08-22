#include "stdafx.h"

uint32_t read_u32(const uint8_t *p) {
	return ((uint32_t)p[0])
		+ ((uint32_t)p[1] << 8)
		+ ((uint32_t)p[2] << 16)
		+ ((uint32_t)p[3] << 24);
}

void read_raw(void *data, size_t len, FILE *f, const char *path) {
	if (1 != fread(data, len, 1, f))
		fatalf("Unable to read from file: %s\n", path);
}

void write_u8(uint8_t v, FILE *f) {
	putc(v, f);
}

void write_u16(uint16_t v, FILE *f) {
	putc(v & 0xff, f);
	putc(v >> 8, f);
}

void write_i16(int16_t v, FILE *f) {
	write_u16((uint16_t)v, f);
}

void write_u32(uint32_t v, FILE *f) {
	putc(v & 0xff, f);
	putc((v >> 8) & 0xff, f);
	putc((v >> 16) & 0xff, f);
	putc((v >> 24) & 0xff, f);
}

void write_pad(int n, FILE *f) {
	while(n--)
		putc(0, f);
}
