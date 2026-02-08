/**
 * @file uft_ownership.h
 * @brief Ownership Annotations for C Memory Management
 * 
 * Stub header - defines macros for documentation purposes.
 */

#ifndef UFT_OWNERSHIP_H
#define UFT_OWNERSHIP_H

/* Ownership annotations (documentation only, no runtime effect) */
#define UFT_OWNS           /* Caller owns the memory, must free */
#define UFT_BORROWS        /* Callee borrows, must not free */
#define UFT_TRANSFERS      /* Ownership transfers to callee */
#define UFT_NULLABLE       /* May be NULL */
#define UFT_NONNULL        /* Must not be NULL */

#endif /* UFT_OWNERSHIP_H */
