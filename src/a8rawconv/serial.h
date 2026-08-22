#ifndef f_SERIAL_H
#define f_SERIAL_H

void serial_open(const char *path);
void serial_close();
void serial_write(const void *data, uint32_t len);
uint32_t serial_tryread(void *buf, uint32_t maxlen, uint32_t timeout);
void serial_read(void *buf, uint32_t len);

#endif
