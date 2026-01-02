/**
 * @file paramschema.h
 * @brief JSON option sanitizer/normalizer for GUI-editable option blobs.
 *
 * Weg B | Block 3
 * -------------
 * The GUI stores exporter/recovery/format options as JSON strings. Users can edit
 * those blobs (advanced mode), so we must:
 *   - accept partial JSON (missing keys)
 *   - reject malformed JSON
 *   - clamp numeric ranges
 *   - enforce enum domains
 *   - fill defaults for missing keys
 *
 * Output: A compact, normalized JSON object string.
 */

#pragma once

#include <QString>
#include <QStringList>

extern "C" {
#include "uft/uft_params.h"
}

class ParamSchema
{
public:
    /**
     * @brief Sanitize a JSON options blob against a uft_param_def_t schema.
     *
     * @param defs Schema definitions.
     * @param count Number of definitions.
     * @param jsonInput Raw user JSON.
     * @param ok (optional) Receives true if input was valid JSON object.
     * @param warnings (optional) Receives non-fatal warnings (clamps, unknown keys removed).
     * @return Normalized JSON object (compact). If input is invalid, returns a default JSON.
     */
    static QString sanitize(const uft_param_def_t *defs,
                            size_t count,
                            const QString &jsonInput,
                            bool *ok = nullptr,
                            QStringList *warnings = nullptr);

    /**
     * @brief Build a default JSON blob from schema defaults.
     */
    static QString defaults(const uft_param_def_t *defs, size_t count);
};
