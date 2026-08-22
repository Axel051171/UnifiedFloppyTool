#include "stdafx.h"
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/types.h>

int g_fdSerialPort = -1;

void serial_open(const char *path) {
	if (!path) {
		path = "/dev/ttyUSB0";
	}

	g_fdSerialPort = open(path, O_RDWR | O_NOCTTY);
	if (g_fdSerialPort < 0)
		fatalf("Unable to open serial port: %s\n", path);

	termios newtio = {0};
	newtio.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR;

	tcflush(g_fdSerialPort, TCIFLUSH);
	tcsetattr(g_fdSerialPort, TCSANOW, &newtio);
}

void serial_close() {
	if (g_fdSerialPort >= 0) {
		close(g_fdSerialPort);
		g_fdSerialPort = -1;
	}
}

void serial_write(const void *data, uint32_t len) {
	fd_set fds_write;
	fd_set fds_error;

	while(len) {
		ssize_t actual = write(g_fdSerialPort, data, len);
		
		if (actual < 0) {
			if (errno == EAGAIN) {
				FD_ZERO(&fds_write);
				FD_ZERO(&fds_error);
				FD_SET(g_fdSerialPort, &fds_write);
				FD_SET(g_fdSerialPort, &fds_error);

				timeval tv;
				tv.tv_sec = 10;
				tv.tv_usec = 0;
				int result = select(g_fdSerialPort+1, NULL, &fds_write, &fds_error, &tv);

				if (result < 0 || FD_ISSET(g_fdSerialPort, &fds_error)) {
					fatal("Error writing to serial port.");
				}

				if (!result)
					fatal("Timeout writing to serial port.");

				continue;
			}

			fatal("Error writing to serial port.");
		}

		if (!actual)
			fatal("Error writing to serial port.");

		data = (const char *)data + actual;
		len -= actual;
	}
}

uint32_t serial_tryread(void *buf, uint32_t maxlen, uint32_t timeout) {
	uint32_t total = 0;
	fd_set fds_read;
	fd_set fds_error;

	while(maxlen) {
		ssize_t actual = read(g_fdSerialPort, buf, maxlen);
		
		if (actual <= 0) {
			if (errno == EAGAIN || actual == 0) {
				FD_ZERO(&fds_read);
				FD_ZERO(&fds_error);
				FD_SET(g_fdSerialPort, &fds_read);
				FD_SET(g_fdSerialPort, &fds_error);

				timeval tv;
				tv.tv_sec = timeout / 1000;
				tv.tv_usec = (timeout % 1000) * 1000;
				int result = select(g_fdSerialPort+1, &fds_read, NULL, &fds_error, &tv);

				if (result < 0 || FD_ISSET(g_fdSerialPort, &fds_error)) {
					fatal("Error reading from serial port.");
				}

				if (!result)
					return total;

				continue;
			}

			fatal("Error reading from serial port.");
		}

		total += actual;
		buf = (char *)buf + actual;
		maxlen -= actual;
	}

	return total;
}

void serial_read(void *buf, uint32_t len) {
	if (len != serial_tryread(buf, len, 10000)) {
		fatal("Timeout while reading from serial port.");
	}
}
