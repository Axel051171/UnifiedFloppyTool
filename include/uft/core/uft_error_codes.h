/**
 * @file uft_error_codes.h
 * @brief Compatibility shim — redirects to UFT v4's SSOT error system.
 *
 * The v3.7 tree had a parallel error-code module at this path with its
 * own `UFT_E_*` constants. v4.x consolidated all error codes into the
 * SSOT-managed `uft/uft_error.h` (generated from `data/errors.tsv` —
 * see MF-003).
 *
 * This shim exists to let v3.7-era files (that still include
 * `uft/core/uft_error_codes.h`) compile without bringing back the
 * conflicting parallel system. New code should `#include "uft/uft_error.h"`
 * directly.
 *
 * No `UFT_E_*` aliases are emitted here. If a caller actually uses
 * those names, port them to the canonical `UFT_ERR_*` values or add
 * targeted aliases to `data/errors_legacy_aliases.tsv` so the SSOT
 * generator handles them centrally.
 */
#ifndef UFT_CORE_UFT_ERROR_CODES_H
#define UFT_CORE_UFT_ERROR_CODES_H

#include "uft/uft_error.h"

#endif /* UFT_CORE_UFT_ERROR_CODES_H */
