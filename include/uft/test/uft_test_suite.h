#pragma once
/* Minimal test suite header */
#include <stddef.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { int passed; int failed; int skipped; } uft_test_stats_t;
int uft_test_run_all(uft_test_stats_t *stats);
#ifdef __cplusplus
}
#endif
