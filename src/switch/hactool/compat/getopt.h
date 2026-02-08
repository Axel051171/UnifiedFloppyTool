/**
 * @file getopt.h
 * @brief POSIX getopt/getopt_long shim for MSVC
 * 
 * Minimal BSD-licensed implementation for Windows builds.
 * Only included via win32 INCLUDEPATH - does not affect Linux/macOS.
 */
#ifndef UFT_COMPAT_GETOPT_H
#define UFT_COMPAT_GETOPT_H

#ifdef __cplusplus
extern "C" {
#endif

extern char *optarg;
extern int   optind, opterr, optopt;

#define no_argument        0
#define required_argument  1
#define optional_argument  2

struct option {
    const char *name;
    int         has_arg;
    int        *flag;
    int         val;
};

int getopt(int argc, char *const argv[], const char *optstring);
int getopt_long(int argc, char *const argv[], const char *optstring,
                const struct option *longopts, int *longindex);

#ifdef __cplusplus
}
#endif

#endif /* UFT_COMPAT_GETOPT_H */
