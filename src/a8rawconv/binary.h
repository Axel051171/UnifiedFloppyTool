#ifndef f_BINARY_H
#define f_BINARY_H

inline uint16_t swizzle_u16_from_le(uint16_t v) {
	return v;
}

inline uint32_t swizzle_u32_from_le(uint32_t v) {
	return v;
}

inline uint16_t swizzle_u16_to_be(uint16_t v) {
	return (uint16_t)((v << 8) + (v >> 8));
}

inline uint16_t swizzle_u16_from_be(uint16_t v) {
	return (uint16_t)((v << 8) + (v >> 8));
}

inline uint16_t read_u16_le(const uint8_t *p) {
	return swizzle_u16_from_le(*(const uint16_t *)p);
}

inline uint16_t read_u32_le(const uint8_t *p) {
	return swizzle_u32_from_le(*(const uint32_t *)p);
}

uint32_t read_u32(const uint8_t *p);

void read_raw(void *data, size_t len, FILE *f, const char *path);

void write_u8(uint8_t v, FILE *f);
void write_u16(uint16_t v, FILE *f);
void write_i16(int16_t v, FILE *f);
void write_u32(uint32_t v, FILE *f);
void write_pad(int n, FILE *f);

#endif
