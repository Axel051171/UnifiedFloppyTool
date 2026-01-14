/**
 * @file uft_platform.c
 * @brief Cross-Platform Implementation
 * 
 * P2-005: Cross-Platform Support
 */

#include "uft/uft_platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Platform-Specific Includes
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifdef UFT_PLATFORM_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <direct.h>
    #include <io.h>
    #include <shlobj.h>
    #define access _access
    #define F_OK 0
#else
    #include <sys/types.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/time.h>
    #include <dirent.h>
    #include <pthread.h>
    #include <time.h>
    #include <errno.h>
    #include <fcntl.h>
    #include <termios.h>
    #include <pwd.h>
#endif

#ifdef UFT_PLATFORM_MACOS
    #include <mach/mach_time.h>
    #include <sys/sysctl.h>
#endif

#ifdef UFT_PLATFORM_LINUX
    #include <sys/sysinfo.h>
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Path Handling
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_path_normalize(char *path)
{
    if (!path) return;
    
#ifdef UFT_PLATFORM_WINDOWS
    for (char *p = path; *p; p++) {
        if (*p == '/') *p = '\\';
    }
#else
    for (char *p = path; *p; p++) {
        if (*p == '\\') *p = '/';
    }
#endif
}

int uft_path_join(char *dest, size_t dest_size, const char *base, const char *rel)
{
    if (!dest || !dest_size) return -1;
    
    if (!base || !*base) {
        strncpy(dest, rel ? rel : "", dest_size - 1);
        dest[dest_size - 1] = '\0';
        return 0;
    }
    
    if (!rel || !*rel) {
        strncpy(dest, base, dest_size - 1);
        dest[dest_size - 1] = '\0';
        return 0;
    }
    
    size_t base_len = strlen(base);
    bool need_sep = (base[base_len - 1] != UFT_PATH_SEPARATOR &&
                     base[base_len - 1] != '/' && base[base_len - 1] != '\\');
    
    int ret = snprintf(dest, dest_size, "%s%s%s",
                       base, need_sep ? UFT_PATH_SEPARATOR_STR : "", rel);
    
    if (ret < 0 || (size_t)ret >= dest_size) return -1;
    
    uft_path_normalize(dest);
    return 0;
}

const char* uft_path_extension(const char *path)
{
    if (!path) return NULL;
    
    const char *dot = strrchr(path, '.');
    const char *sep = strrchr(path, UFT_PATH_SEPARATOR);
    
    if (!dot || (sep && dot < sep)) return NULL;
    return dot + 1;
}

const char* uft_path_basename(const char *path)
{
    if (!path) return NULL;
    
    const char *sep = strrchr(path, UFT_PATH_SEPARATOR);
#ifdef UFT_PLATFORM_WINDOWS
    const char *sep2 = strrchr(path, '/');
    if (sep2 && (!sep || sep2 > sep)) sep = sep2;
#endif
    
    return sep ? sep + 1 : path;
}

int uft_path_dirname(const char *path, char *dir, size_t dir_size)
{
    if (!path || !dir || !dir_size) return -1;
    
    const char *base = uft_path_basename(path);
    if (base == path) {
        strncpy(dir, ".", dir_size - 1);
    } else {
        size_t len = base - path - 1;
        if (len >= dir_size) len = dir_size - 1;
        strncpy(dir, path, len);
        dir[len] = '\0';
    }
    dir[dir_size - 1] = '\0';
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * File System
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool uft_file_exists(const char *path)
{
    if (!path) return false;
#ifdef UFT_PLATFORM_WINDOWS
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
#endif
}

bool uft_dir_exists(const char *path)
{
    if (!path) return false;
#ifdef UFT_PLATFORM_WINDOWS
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
#endif
}

int64_t uft_file_size(const char *path)
{
    if (!path) return -1;
#ifdef UFT_PLATFORM_WINDOWS
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data)) return -1;
    return ((int64_t)data.nFileSizeHigh << 32) | data.nFileSizeLow;
#else
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return st.st_size;
#endif
}

int uft_mkdir_p(const char *path)
{
    if (!path || !*path) return -1;
    
    char tmp[UFT_PATH_MAX];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    uft_path_normalize(tmp);
    
    size_t len = strlen(tmp);
    if (tmp[len - 1] == UFT_PATH_SEPARATOR) tmp[len - 1] = '\0';
    
    for (char *p = tmp + 1; *p; p++) {
        if (*p == UFT_PATH_SEPARATOR) {
            *p = '\0';
#ifdef UFT_PLATFORM_WINDOWS
            _mkdir(tmp);
#else
            mkdir(tmp, 0755);
#endif
            *p = UFT_PATH_SEPARATOR;
        }
    }
    
#ifdef UFT_PLATFORM_WINDOWS
    return _mkdir(tmp) == 0 || errno == EEXIST ? 0 : -1;
#else
    return mkdir(tmp, 0755) == 0 || errno == EEXIST ? 0 : -1;
#endif
}

int uft_get_home_dir(char *path, size_t path_size)
{
    if (!path || !path_size) return -1;
    
#ifdef UFT_PLATFORM_WINDOWS
    if (SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, path) != S_OK) {
        const char *home = getenv("USERPROFILE");
        if (!home) return -1;
        strncpy(path, home, path_size - 1);
        path[path_size - 1] = '\0';
    }
#else
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (!pw) return -1;
        home = pw->pw_dir;
    }
    strncpy(path, home, path_size - 1);
    path[path_size - 1] = '\0';
#endif
    return 0;
}

int uft_get_app_data_dir(char *path, size_t path_size, const char *app_name)
{
    if (!path || !path_size) return -1;
    
    char base[UFT_PATH_MAX];
    
#ifdef UFT_PLATFORM_WINDOWS
    if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, base) != S_OK) {
        return -1;
    }
#elif defined(UFT_PLATFORM_MACOS)
    if (uft_get_home_dir(base, sizeof(base)) != 0) return -1;
    strncat(base, "/Library/Application Support", sizeof(base) - strlen(base) - 1);
#else
    const char *xdg = getenv("XDG_DATA_HOME");
    if (xdg) {
        strncpy(base, xdg, sizeof(base) - 1);
    } else {
        if (uft_get_home_dir(base, sizeof(base)) != 0) return -1;
        strncat(base, "/.local/share", sizeof(base) - strlen(base) - 1);
    }
#endif
    
    return uft_path_join(path, path_size, base, app_name ? app_name : "uft");
}

int uft_get_temp_dir(char *path, size_t path_size)
{
    if (!path || !path_size) return -1;
    
#ifdef UFT_PLATFORM_WINDOWS
    DWORD len = GetTempPathA((DWORD)path_size, path);
    if (len == 0 || len > path_size) return -1;
#else
    const char *tmp = getenv("TMPDIR");
    if (!tmp) tmp = getenv("TMP");
    if (!tmp) tmp = getenv("TEMP");
    if (!tmp) tmp = "/tmp";
    strncpy(path, tmp, path_size - 1);
    path[path_size - 1] = '\0';
#endif
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * High Resolution Timing
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifdef UFT_PLATFORM_WINDOWS

static LARGE_INTEGER _freq = {0};
static bool _freq_init = false;

static void _init_freq(void) {
    if (!_freq_init) {
        QueryPerformanceFrequency(&_freq);
        _freq_init = true;
    }
}

uint64_t uft_time_ns(void)
{
    _init_freq();
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (uint64_t)(now.QuadPart * 1000000000ULL / _freq.QuadPart);
}

uint64_t uft_time_us(void)
{
    _init_freq();
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (uint64_t)(now.QuadPart * 1000000ULL / _freq.QuadPart);
}

uint64_t uft_time_ms(void)
{
    return GetTickCount64();
}

void uft_sleep_ms(uint32_t ms)
{
    Sleep(ms);
}

void uft_sleep_us(uint32_t us)
{
    /* Windows doesn't have microsecond sleep, use busy wait for small values */
    if (us < 1000) {
        uint64_t end = uft_time_us() + us;
        while (uft_time_us() < end) {
            /* Spin */
        }
    } else {
        Sleep(us / 1000);
    }
}

#elif defined(UFT_PLATFORM_MACOS)

static mach_timebase_info_data_t _timebase = {0};
static bool _timebase_init = false;

static void _init_timebase(void) {
    if (!_timebase_init) {
        mach_timebase_info(&_timebase);
        _timebase_init = true;
    }
}

uint64_t uft_time_ns(void)
{
    _init_timebase();
    return mach_absolute_time() * _timebase.numer / _timebase.denom;
}

uint64_t uft_time_us(void)
{
    return uft_time_ns() / 1000;
}

uint64_t uft_time_ms(void)
{
    return uft_time_ns() / 1000000;
}

void uft_sleep_ms(uint32_t ms)
{
    usleep(ms * 1000);
}

void uft_sleep_us(uint32_t us)
{
    usleep(us);
}

#else /* Linux/POSIX */

uint64_t uft_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

uint64_t uft_time_us(void)
{
    return uft_time_ns() / 1000;
}

uint64_t uft_time_ms(void)
{
    return uft_time_ns() / 1000000;
}

void uft_sleep_ms(uint32_t ms)
{
    usleep(ms * 1000);
}

void uft_sleep_us(uint32_t us)
{
    usleep(us);
}

#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Serial Port
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifdef UFT_PLATFORM_WINDOWS

struct uft_serial {
    HANDLE handle;
    DCB dcb;
    COMMTIMEOUTS timeouts;
};

uft_serial_t* uft_serial_open(const char *port, const uft_serial_config_t *config)
{
    if (!port) return NULL;
    
    uft_serial_config_t cfg = UFT_SERIAL_CONFIG_DEFAULT;
    if (config) cfg = *config;
    
    char portname[32];
    snprintf(portname, sizeof(portname), "\\\\.\\%s", port);
    
    HANDLE h = CreateFileA(portname, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                          OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    
    uft_serial_t *serial = calloc(1, sizeof(uft_serial_t));
    if (!serial) {
        CloseHandle(h);
        return NULL;
    }
    serial->handle = h;
    
    /* Configure port */
    serial->dcb.DCBlength = sizeof(DCB);
    GetCommState(h, &serial->dcb);
    
    serial->dcb.BaudRate = cfg.baud_rate;
    serial->dcb.ByteSize = cfg.data_bits;
    serial->dcb.StopBits = (cfg.stop_bits == 2) ? TWOSTOPBITS : ONESTOPBIT;
    serial->dcb.Parity = (cfg.parity == 'E') ? EVENPARITY :
                         (cfg.parity == 'O') ? ODDPARITY : NOPARITY;
    serial->dcb.fBinary = TRUE;
    serial->dcb.fDtrControl = DTR_CONTROL_ENABLE;
    serial->dcb.fRtsControl = cfg.flow_control ? RTS_CONTROL_HANDSHAKE : RTS_CONTROL_ENABLE;
    
    if (!SetCommState(h, &serial->dcb)) {
        CloseHandle(h);
        free(serial);
        return NULL;
    }
    
    /* Set timeouts */
    serial->timeouts.ReadIntervalTimeout = 50;
    serial->timeouts.ReadTotalTimeoutConstant = cfg.timeout_ms;
    serial->timeouts.ReadTotalTimeoutMultiplier = 0;
    serial->timeouts.WriteTotalTimeoutConstant = cfg.timeout_ms;
    serial->timeouts.WriteTotalTimeoutMultiplier = 0;
    SetCommTimeouts(h, &serial->timeouts);
    
    return serial;
}

void uft_serial_close(uft_serial_t *serial)
{
    if (serial) {
        CloseHandle(serial->handle);
        free(serial);
    }
}

int uft_serial_read(uft_serial_t *serial, void *buffer, size_t size)
{
    if (!serial || !buffer) return -1;
    DWORD read = 0;
    if (!ReadFile(serial->handle, buffer, (DWORD)size, &read, NULL)) return -1;
    return (int)read;
}

int uft_serial_write(uft_serial_t *serial, const void *buffer, size_t size)
{
    if (!serial || !buffer) return -1;
    DWORD written = 0;
    if (!WriteFile(serial->handle, buffer, (DWORD)size, &written, NULL)) return -1;
    return (int)written;
}

int uft_serial_flush(uft_serial_t *serial)
{
    if (!serial) return -1;
    return FlushFileBuffers(serial->handle) ? 0 : -1;
}

int uft_serial_set_timeout(uft_serial_t *serial, uint32_t timeout_ms)
{
    if (!serial) return -1;
    serial->timeouts.ReadTotalTimeoutConstant = timeout_ms;
    serial->timeouts.WriteTotalTimeoutConstant = timeout_ms;
    return SetCommTimeouts(serial->handle, &serial->timeouts) ? 0 : -1;
}

int uft_serial_enumerate(char **ports, int max_ports)
{
    if (!ports || max_ports <= 0) return 0;
    
    int count = 0;
    for (int i = 1; i <= 255 && count < max_ports; i++) {
        char name[16];
        snprintf(name, sizeof(name), "COM%d", i);
        
        char path[32];
        snprintf(path, sizeof(path), "\\\\.\\%s", name);
        
        HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                              OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
            ports[count] = strdup(name);
            count++;
        }
    }
    return count;
}

#else /* POSIX */

struct uft_serial {
    int fd;
    struct termios orig_termios;
    struct termios termios;
};

uft_serial_t* uft_serial_open(const char *port, const uft_serial_config_t *config)
{
    if (!port) return NULL;
    
    uft_serial_config_t cfg = UFT_SERIAL_CONFIG_DEFAULT;
    if (config) cfg = *config;
    
    int fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) return NULL;
    
    /* Clear non-blocking */
    fcntl(fd, F_SETFL, 0);
    
    uft_serial_t *serial = calloc(1, sizeof(uft_serial_t));
    if (!serial) {
        close(fd);
        return NULL;
    }
    serial->fd = fd;
    
    /* Save original settings */
    tcgetattr(fd, &serial->orig_termios);
    
    /* Configure port */
    struct termios *t = &serial->termios;
    memset(t, 0, sizeof(*t));
    
    /* Baud rate */
    speed_t speed;
    switch (cfg.baud_rate) {
        case 9600:   speed = B9600; break;
        case 19200:  speed = B19200; break;
        case 38400:  speed = B38400; break;
#ifdef B57600
        case 57600:  speed = B57600; break;
#endif
#ifdef B115200
        case 115200: speed = B115200; break;
#endif
#ifdef B230400
        case 230400: speed = B230400; break;
#endif
#ifdef B460800
        case 460800: speed = B460800; break;
#endif
#ifdef B921600
        case 921600: speed = B921600; break;
#endif
        default: speed = B38400;
    }
    cfsetispeed(t, speed);
    cfsetospeed(t, speed);
    
    /* 8N1 raw mode */
    t->c_cflag |= (CLOCAL | CREAD);
    t->c_cflag &= ~PARENB;
    t->c_cflag &= ~CSTOPB;
    t->c_cflag &= ~CSIZE;
    t->c_cflag |= CS8;
    
    if (cfg.flow_control) {
        t->c_cflag |= CRTSCTS;
    } else {
        t->c_cflag &= ~CRTSCTS;
    }
    
    t->c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    t->c_iflag &= ~(IXON | IXOFF | IXANY);
    t->c_oflag &= ~OPOST;
    
    /* Timeout */
    t->c_cc[VMIN] = 0;
    t->c_cc[VTIME] = cfg.timeout_ms / 100;
    if (t->c_cc[VTIME] == 0) t->c_cc[VTIME] = 1;
    
    tcsetattr(fd, TCSANOW, t);
    tcflush(fd, TCIOFLUSH);
    
    return serial;
}

void uft_serial_close(uft_serial_t *serial)
{
    if (serial) {
        tcsetattr(serial->fd, TCSANOW, &serial->orig_termios);
        close(serial->fd);
        free(serial);
    }
}

int uft_serial_read(uft_serial_t *serial, void *buffer, size_t size)
{
    if (!serial || !buffer) return -1;
    return (int)read(serial->fd, buffer, size);
}

int uft_serial_write(uft_serial_t *serial, const void *buffer, size_t size)
{
    if (!serial || !buffer) return -1;
    return (int)write(serial->fd, buffer, size);
}

int uft_serial_flush(uft_serial_t *serial)
{
    if (!serial) return -1;
    return tcdrain(serial->fd);
}

int uft_serial_set_timeout(uft_serial_t *serial, uint32_t timeout_ms)
{
    if (!serial) return -1;
    serial->termios.c_cc[VTIME] = timeout_ms / 100;
    if (serial->termios.c_cc[VTIME] == 0) serial->termios.c_cc[VTIME] = 1;
    return tcsetattr(serial->fd, TCSANOW, &serial->termios);
}

int uft_serial_enumerate(char **ports, int max_ports)
{
    if (!ports || max_ports <= 0) return 0;
    
    int count = 0;
    const char *patterns[] = {
#ifdef UFT_PLATFORM_MACOS
        "/dev/cu.usbserial",
        "/dev/cu.usbmodem",
        "/dev/tty.usbserial",
        "/dev/tty.usbmodem",
#else
        "/dev/ttyUSB",
        "/dev/ttyACM",
        "/dev/ttyS",
#endif
        NULL
    };
    
    DIR *dir = opendir("/dev");
    if (!dir) return 0;
    
    struct dirent *entry;
    while ((entry = readdir(dir)) && count < max_ports) {
        for (int i = 0; patterns[i]; i++) {
            const char *base = strrchr(patterns[i], '/') + 1;
            if (strncmp(entry->d_name, base, strlen(base)) == 0) {
                char path[256];
                snprintf(path, sizeof(path), "/dev/%s", entry->d_name);
                ports[count++] = strdup(path);
                break;
            }
        }
    }
    closedir(dir);
    
    return count;
}

#endif

void uft_serial_free_list(char **ports, int count)
{
    for (int i = 0; i < count; i++) {
        free(ports[i]);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Mutex
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifdef UFT_PLATFORM_WINDOWS

struct uft_mutex {
    CRITICAL_SECTION cs;
};

uft_mutex_t* uft_mutex_create(void)
{
    uft_mutex_t *m = malloc(sizeof(uft_mutex_t));
    if (m) InitializeCriticalSection(&m->cs);
    return m;
}

void uft_mutex_destroy(uft_mutex_t *mutex)
{
    if (mutex) {
        DeleteCriticalSection(&mutex->cs);
        free(mutex);
    }
}

void uft_mutex_lock(uft_mutex_t *mutex)
{
    if (mutex) EnterCriticalSection(&mutex->cs);
}

bool uft_mutex_trylock(uft_mutex_t *mutex)
{
    return mutex ? TryEnterCriticalSection(&mutex->cs) : false;
}

void uft_mutex_unlock(uft_mutex_t *mutex)
{
    if (mutex) LeaveCriticalSection(&mutex->cs);
}

#else /* POSIX */

struct uft_mutex {
    pthread_mutex_t pm;
};

uft_mutex_t* uft_mutex_create(void)
{
    uft_mutex_t *m = malloc(sizeof(uft_mutex_t));
    if (m) pthread_mutex_init(&m->pm, NULL);
    return m;
}

void uft_mutex_destroy(uft_mutex_t *mutex)
{
    if (mutex) {
        pthread_mutex_destroy(&mutex->pm);
        free(mutex);
    }
}

void uft_mutex_lock(uft_mutex_t *mutex)
{
    if (mutex) pthread_mutex_lock(&mutex->pm);
}

bool uft_mutex_trylock(uft_mutex_t *mutex)
{
    return mutex ? (pthread_mutex_trylock(&mutex->pm) == 0) : false;
}

void uft_mutex_unlock(uft_mutex_t *mutex)
{
    if (mutex) pthread_mutex_unlock(&mutex->pm);
}

#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Platform Info
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_platform_get_info(uft_platform_info_t *info)
{
    if (!info) return;
    
    info->os_name = UFT_PLATFORM_NAME;
    info->arch_name = UFT_ARCH_NAME;
    info->compiler_name = UFT_COMPILER_NAME;
    info->compiler_version = UFT_COMPILER_VERSION;
    
#ifdef UFT_LITTLE_ENDIAN
    info->is_little_endian = true;
#else
    info->is_little_endian = false;
#endif

#ifdef UFT_PLATFORM_WINDOWS
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    info->cpu_count = si.dwNumberOfProcessors;
    
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    info->total_memory = ms.ullTotalPhys;
    
    OSVERSIONINFOEXA vi;
    vi.dwOSVersionInfoSize = sizeof(vi);
    /* GetVersionExA is deprecated but still works */
    info->os_version = "Windows";
    
#elif defined(UFT_PLATFORM_MACOS)
    int ncpu;
    size_t len = sizeof(ncpu);
    sysctlbyname("hw.ncpu", &ncpu, &len, NULL, 0);
    info->cpu_count = ncpu;
    
    int64_t mem;
    len = sizeof(mem);
    sysctlbyname("hw.memsize", &mem, &len, NULL, 0);
    info->total_memory = mem;
    
    info->os_version = "macOS";
    
#elif defined(UFT_PLATFORM_LINUX)
    info->cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    
    struct sysinfo si;
    sysinfo(&si);
    info->total_memory = si.totalram * si.mem_unit;
    
    info->os_version = "Linux";
#else
    info->cpu_count = 1;
    info->total_memory = 0;
    info->os_version = "Unknown";
#endif
}

void uft_platform_print_info(void)
{
    uft_platform_info_t info;
    uft_platform_get_info(&info);
    
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  UFT Platform Information\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  OS:       %s (%s)\n", info.os_name, info.os_version);
    printf("  Arch:     %s (%d-bit)\n", info.arch_name, UFT_ARCH_BITS);
    printf("  Compiler: %s (v%d)\n", info.compiler_name, info.compiler_version);
    printf("  CPUs:     %d\n", info.cpu_count);
    printf("  Memory:   %llu MB\n", (unsigned long long)(info.total_memory / (1024*1024)));
    printf("  Endian:   %s\n", info.is_little_endian ? "Little" : "Big");
    printf("═══════════════════════════════════════════════════════════════\n");
}
