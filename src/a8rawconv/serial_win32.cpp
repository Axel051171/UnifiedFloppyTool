#include "stdafx.h"
#include <windows.h>
#include <tchar.h>

HANDLE g_hSerialPort = INVALID_HANDLE_VALUE;
OVERLAPPED g_serialOp;

void serial_open(const char *path) {
	char namebuf[128];

	if (!path) {
		HKEY hkey;
		
		if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\Enum\\FTDIBUS\\VID_0403+PID_6015+SCP-JIMA\\0000\\Device Parameters"), 0, KEY_READ, &hkey)) {
			DWORD type = 0;
			DWORD namelen = (DWORD)sizeof(namebuf);
			if (ERROR_SUCCESS == RegQueryValueExA(hkey, "PortName", NULL, &type, (LPBYTE)namebuf, &namelen) && type == REG_SZ) {
				namebuf[namelen > 127 ? 127 : namelen] = 0;

				// Check if we have a path longer than COM1-COM9. If so, we must force escaping out
				// of the Win32 namespcae.
				if (strlen(namebuf) > 4) {
					memmove(namebuf + 4, namebuf, sizeof(namebuf) - 4);
					namebuf[127] = 0;

					namebuf[0] = '\\';
					namebuf[1] = '\\';
					namebuf[2] = '.';
					namebuf[3] = '\\';
				}

				path = namebuf;
			}

			RegCloseKey(hkey);
		}

		if (!path)
			fatal("Unable to autodetect SCP COM port. Make sure that the Virtual COM Port (VCP) driver is enabled, and specify the port directly if needed (example: scp0:96tpi:com1).");

		printf("Detected SCP port: %s.\n", namebuf);
	}

	g_serialOp.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
	if (!g_serialOp.hEvent)
		fatal("Unable to create serial op event\n");

	g_hSerialPort = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if (g_hSerialPort == INVALID_HANDLE_VALUE)
		fatalf("Unable to open serial port: %s", path);

	DCB dcb = {sizeof(DCB)};
	BuildCommDCB(_T("250000,n,8,1"), &dcb);
	SetCommState((HANDLE)g_hSerialPort, &dcb);
}

void serial_close() {
	if (g_hSerialPort != INVALID_HANDLE_VALUE) {
		CloseHandle(g_hSerialPort);
		g_hSerialPort = INVALID_HANDLE_VALUE;
	}

	if (g_serialOp.hEvent) {
		CloseHandle(g_serialOp.hEvent);
		g_serialOp.hEvent = NULL;
	}
}

void serial_write(const void *data, uint32_t len) {
	while(len) {
		ResetEvent(g_serialOp.hEvent);

		DWORD actual = 0;
		if (!WriteFile(g_hSerialPort, data, len, &actual, &g_serialOp)) {
			if (GetLastError() != ERROR_IO_PENDING) {
				CancelIo(g_hSerialPort);
				fatal("Error writing to serial port.");
			}

			if (WAIT_OBJECT_0 != WaitForSingleObject(g_serialOp.hEvent, 10000)) {
				CancelIo(g_hSerialPort);
				fatal("Timeout while writing to serial port.");
			}

			if (!GetOverlappedResult(g_hSerialPort, &g_serialOp, &actual, TRUE)) {
				CancelIo(g_hSerialPort);
				fatal("Error writing to serial port.");
			}
		}

		if (!actual)
			fatal("Error writing to serial port.");

		data = (const char *)data + actual;
		len -= actual;
	}
}

uint32_t serial_tryread(void *buf, uint32_t maxlen, uint32_t timeout) {
	uint32_t total = 0;

	while(maxlen) {
		ResetEvent(g_serialOp.hEvent);

		DWORD actual = 0;
		if (!ReadFile(g_hSerialPort, buf, maxlen, &actual, &g_serialOp)) {
			if (GetLastError() != ERROR_IO_PENDING) {
				CancelIo(g_hSerialPort);
				fatal("Error reading from serial port.");
			}

			if (WAIT_OBJECT_0 != WaitForSingleObject(g_serialOp.hEvent, timeout)) {
				CancelIo(g_hSerialPort);
				return total;
			}

			if (!GetOverlappedResult(g_hSerialPort, &g_serialOp, &actual, TRUE)) {
				CancelIo(g_hSerialPort);
				fatal("Error reading from serial port.");
			}
		}

		if (!actual)
			return total;

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
