/**
 * @file getopt.c
 * @brief Minimal POSIX getopt/getopt_long for MSVC
 *
 * Based on public domain implementations.
 * Only compiled on Windows (gated by .pro win32 block).
 */

#include "getopt.h"
#include <string.h>
#include <stdio.h>

char *optarg = NULL;
int   optind = 1;
int   opterr = 1;
int   optopt = '?';

static int optwhere = 1;

static void permute(char *const argv[], int from, int to)
{
    char *tmp = argv[from];
    int i;
    for (i = from; i > to; i--)
        ((char **)argv)[i] = argv[i - 1];
    ((char **)argv)[to] = tmp;
}

int getopt(int argc, char *const argv[], const char *optstring)
{
    const char *p;

    optarg = NULL;

    if (optind >= argc || argv[optind] == NULL)
        return -1;

    if (argv[optind][0] != '-' || argv[optind][1] == '\0')
        return -1;

    if (strcmp(argv[optind], "--") == 0) {
        optind++;
        return -1;
    }

    optopt = argv[optind][optwhere];
    p = strchr(optstring, optopt);

    if (optopt == ':' || p == NULL) {
        if (opterr)
            fprintf(stderr, "%s: unknown option '-%c'\n", argv[0], optopt);
        if (argv[optind][++optwhere] == '\0') {
            optind++;
            optwhere = 1;
        }
        return '?';
    }

    if (p[1] == ':') {
        /* option requires argument */
        if (argv[optind][optwhere + 1] != '\0') {
            optarg = &((char *)argv[optind])[optwhere + 1];
            optind++;
            optwhere = 1;
        } else if (p[2] != ':') {
            /* required argument */
            if (++optind >= argc) {
                if (opterr)
                    fprintf(stderr, "%s: option '-%c' requires an argument\n",
                            argv[0], optopt);
                optwhere = 1;
                return (optstring[0] == ':') ? ':' : '?';
            }
            optarg = argv[optind++];
            optwhere = 1;
        } else {
            /* optional argument (not present) */
            optind++;
            optwhere = 1;
        }
    } else {
        /* no argument */
        if (argv[optind][++optwhere] == '\0') {
            optind++;
            optwhere = 1;
        }
    }

    return optopt;
}

int getopt_long(int argc, char *const argv[], const char *optstring,
                const struct option *longopts, int *longindex)
{
    int i;

    if (optind >= argc || argv[optind] == NULL)
        return -1;

    /* Check for -- long option */
    if (argv[optind][0] == '-' && argv[optind][1] == '-' && argv[optind][2] != '\0') {
        const char *name = &argv[optind][2];
        const char *eq = strchr(name, '=');
        size_t namelen = eq ? (size_t)(eq - name) : strlen(name);

        for (i = 0; longopts[i].name != NULL; i++) {
            if (strncmp(longopts[i].name, name, namelen) == 0 &&
                strlen(longopts[i].name) == namelen) {

                if (longindex)
                    *longindex = i;

                if (longopts[i].has_arg == required_argument ||
                    longopts[i].has_arg == optional_argument) {
                    if (eq) {
                        optarg = (char *)(eq + 1);
                    } else if (longopts[i].has_arg == required_argument) {
                        if (++optind >= argc) {
                            if (opterr)
                                fprintf(stderr, "%s: option '--%s' requires an argument\n",
                                        argv[0], longopts[i].name);
                            return '?';
                        }
                        optarg = argv[optind];
                    }
                }

                optind++;
                if (longopts[i].flag != NULL) {
                    *longopts[i].flag = longopts[i].val;
                    return 0;
                }
                return longopts[i].val;
            }
        }

        if (opterr)
            fprintf(stderr, "%s: unknown option '%s'\n", argv[0], argv[optind]);
        optind++;
        return '?';
    }

    /* Fall back to short option processing */
    return getopt(argc, argv, optstring);
}
