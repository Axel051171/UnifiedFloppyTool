#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct uft_note_line { const char *file; const char *line; } uft_note_line_t;
const uft_note_line_t *uft_casutil_notes_get(size_t *out_count);
#ifdef __cplusplus
}
#endif
