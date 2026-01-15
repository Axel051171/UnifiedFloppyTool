/**
 * @file serial_stream.c
 * @brief Serial port streaming interface
 * @version 3.8.0
 */
/* Enable BSD extensions on macOS for cfmakeraw and higher baud rates */
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE 1
#endif

#include "uft/link/serial_stream.h"

#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static void set_last_err(uft_diag_t *diag, const char *prefix)
{
    if (!diag) return;
    DWORD e = GetLastError();
    (void)e;
    uft_diag_set(diag, prefix);
}

uft_rc_t uft_stream_open(uft_serial_t *s, const char *device, const uft_serial_opts_t *opt, uft_diag_t *diag)
{
    if (!s || !device || !opt) { uft_diag_set(diag, "serial: invalid args"); return UFT_EINVAL; }
    memset(s, 0, sizeof(*s));

    HANDLE h = CreateFileA(device, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { set_last_err(diag, "serial: CreateFile failed"); return UFT_EIO; }

    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(h, &dcb)) { CloseHandle(h); set_last_err(diag, "serial: GetCommState failed"); return UFT_EIO; }

    dcb.BaudRate = opt->baud;
    dcb.ByteSize = opt->databits ? opt->databits : 8;
    dcb.StopBits = (opt->stopbits == 2) ? TWOSTOPBITS : ONESTOPBIT;
    dcb.Parity   = (opt->parity=='E') ? EVENPARITY : (opt->parity=='O') ? ODDPARITY : NOPARITY;
    dcb.fOutxCtsFlow = opt->rtscts ? TRUE : FALSE;
    dcb.fRtsControl  = opt->rtscts ? RTS_CONTROL_HANDSHAKE : RTS_CONTROL_ENABLE;

    if (!SetCommState(h, &dcb)) { CloseHandle(h); set_last_err(diag, "serial: SetCommState failed"); return UFT_EIO; }

    COMMTIMEOUTS t;
    memset(&t, 0, sizeof(t));
    t.ReadIntervalTimeout = MAXDWORD;
    t.ReadTotalTimeoutConstant = opt->read_timeout_ms;
    t.ReadTotalTimeoutMultiplier = 0;
    if (!SetCommTimeouts(h, &t)) { CloseHandle(h); set_last_err(diag, "serial: SetCommTimeouts failed"); return UFT_EIO; }

    s->h = (void*)h;
    s->is_open = 1;
    uft_diag_set(diag, "serial: open ok");
    return UFT_OK;
}

void uft_stream_close(uft_serial_t *s)
{
    if (!s || !s->is_open) return;
    CloseHandle((HANDLE)s->h);
    s->h = NULL;
    s->is_open = 0;
}

uft_rc_t uft_stream_read(uft_serial_t *s, void *buf, size_t n, size_t *out_n, uft_diag_t *diag)
{
    if (out_n) *out_n = 0;
    if (!s || !s->is_open || !buf || n==0) { uft_diag_set(diag, "serial: invalid args"); return UFT_EINVAL; }
    DWORD got = 0;
    if (!ReadFile((HANDLE)s->h, buf, (DWORD)n, &got, NULL)) { set_last_err(diag, "serial: ReadFile failed"); return UFT_EIO; }
    if (out_n) *out_n = (size_t)got;
    if (got == 0) return UFT_ETIMEOUT;
    return UFT_OK;
}

uft_rc_t uft_stream_write_all(uft_serial_t *s, const void *buf, size_t n, uft_diag_t *diag)
{
    if (!s || !s->is_open || !buf) { uft_diag_set(diag, "serial: invalid args"); return UFT_EINVAL; }
    const uint8_t *p = (const uint8_t*)buf;
    size_t off = 0;
    while (off < n) {
        DWORD put = 0;
        if (!WriteFile((HANDLE)s->h, p + off, (DWORD)(n - off), &put, NULL)) { set_last_err(diag, "serial: WriteFile failed"); return UFT_EIO; }
        if (put == 0) { uft_diag_set(diag, "serial: write stalled"); return UFT_EIO; }
        off += (size_t)put;
    }
    return UFT_OK;
}

#else /* POSIX */
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/select.h>

/* Fallback baud rate definitions for platforms that don't have them */
#ifndef B57600
#define B57600 57600
#endif
#ifndef B115200
#define B115200 115200
#endif

/* cfmakeraw fallback for platforms without it */
#ifndef HAVE_CFMAKERAW
#if !defined(__APPLE__) && !defined(__linux__) && !defined(__FreeBSD__)
static void cfmakeraw(struct termios *t) {
    t->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    t->c_oflag &= ~OPOST;
    t->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    t->c_cflag &= ~(CSIZE | PARENB);
    t->c_cflag |= CS8;
}
#endif
#endif

static speed_t baud_to_speed(uint32_t baud)
{
    switch (baud) {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
#ifdef B230400
        case 230400: return B230400;
#endif
        default: return B115200;
    }
}

uft_rc_t uft_stream_open(uft_serial_t *s, const char *device, const uft_serial_opts_t *opt, uft_diag_t *diag)
{
    if (!s || !device || !opt) { uft_diag_set(diag, "serial: invalid args"); return UFT_EINVAL; }
    memset(s, 0, sizeof(*s));
    int fd = open(device, O_RDWR | O_NOCTTY);
    if (fd < 0) { uft_diag_set(diag, "serial: open failed"); return UFT_EIO; }

    struct termios tio;
    memset(&tio, 0, sizeof(tio));
    if (tcgetattr(fd, &tio) != 0) { close(fd); uft_diag_set(diag, "serial: tcgetattr failed"); return UFT_EIO; }

    cfmakeraw(&tio);
    speed_t sp = baud_to_speed(opt->baud);
    cfsetispeed(&tio, sp);
    cfsetospeed(&tio, sp);

    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cflag &= ~CSIZE;
    tio.c_cflag |= CS8;

    if (opt->parity == 'E') { tio.c_cflag |= PARENB; tio.c_cflag &= ~PARODD; }
    else if (opt->parity == 'O') { tio.c_cflag |= PARENB; tio.c_cflag |= PARODD; }
    else { tio.c_cflag &= ~PARENB; }

    if (opt->stopbits == 2) tio.c_cflag |= CSTOPB;
    else tio.c_cflag &= ~CSTOPB;

#ifdef CRTSCTS
    if (opt->rtscts) tio.c_cflag |= CRTSCTS;
    else tio.c_cflag &= ~CRTSCTS;
#endif

    if (tcsetattr(fd, TCSANOW, &tio) != 0) { close(fd); uft_diag_set(diag, "serial: tcsetattr failed"); return UFT_EIO; }

    s->fd = fd;
    s->is_open = 1;
    uft_diag_set(diag, "serial: open ok");
    return UFT_OK;
}

void uft_stream_close(uft_serial_t *s)
{
    if (!s || !s->is_open) return;
    close(s->fd);
    s->fd = -1;
    s->is_open = 0;
}

uft_rc_t uft_stream_read(uft_serial_t *s, void *buf, size_t n, size_t *out_n, uft_diag_t *diag)
{
    if (out_n) *out_n = 0;
    if (!s || !s->is_open || !buf || n==0) { uft_diag_set(diag, "serial: invalid args"); return UFT_EINVAL; }

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(s->fd, &rfds);

    struct timeval tv;
    tv.tv_sec = (int)(0);
    tv.tv_usec = (int)(0);

    /* If read_timeout_ms is unsupported here (no opts stored), we do a blocking read.
       Caller can wrap with select externally in UFT. */
    (void)tv;

    ssize_t r = read(s->fd, buf, n);
    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return UFT_ETIMEOUT;
        uft_diag_set(diag, "serial: read failed");
        return UFT_EIO;
    }
    if (r == 0) return UFT_ETIMEOUT;
    if (out_n) *out_n = (size_t)r;
    return UFT_OK;
}

uft_rc_t uft_stream_write_all(uft_serial_t *s, const void *buf, size_t n, uft_diag_t *diag)
{
    if (!s || !s->is_open || !buf) { uft_diag_set(diag, "serial: invalid args"); return UFT_EINVAL; }
    const uint8_t *p = (const uint8_t*)buf;
    size_t off = 0;
    while (off < n) {
        ssize_t w = write(s->fd, p + off, n - off);
        if (w < 0) { uft_diag_set(diag, "serial: write failed"); return UFT_EIO; }
        if (w == 0) { uft_diag_set(diag, "serial: write stalled"); return UFT_EIO; }
        off += (size_t)w;
    }
    return UFT_OK;
}
#endif
