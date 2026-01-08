#ifndef UFF_CONTAINER_H
#define UFF_CONTAINER_H
#include <stdint.h>
typedef struct uff_container uff_container_t;
int uff_container_create(uff_container_t **container);
void uff_container_destroy(uff_container_t *container);
#endif
