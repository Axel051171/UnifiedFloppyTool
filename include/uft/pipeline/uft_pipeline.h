#ifndef UFT_PIPELINE_H
#define UFT_PIPELINE_H
#include <stdint.h>
typedef struct uft_pipeline uft_pipeline_t;
int uft_pipeline_create(uft_pipeline_t **pipeline);
void uft_pipeline_destroy(uft_pipeline_t *pipeline);
#endif
