/**
 * @file presets.h
 * @brief Built-in per-format preset bundles for GUI.
 *
 * These presets exist to reduce "JSON knob" friction for users.
 * They apply a consistent set of recovery + format + output option overrides
 * on top of schema defaults.
 */

#pragma once

#include <QList>
#include <QHash>
#include <QString>

extern "C" {
#include "uft/uft_formats.h"
#include "uft/uft_output.h"
}

struct UftPreset
{
    QString id;          ///< stable id ("fast", "balanced", "aggressive")
    QString name;        ///< UI name
    QString description; ///< short UI hint

    // JSON option blobs (compact JSON object strings)
    QString recoveryJson;
    QString formatJson;
    QHash<uft_output_format_t, QString> outputJsonByFmt;
};

class UftPresets
{
public:
    /**
     * @brief Presets available for a disk format.
     * Always includes at least: Custom (no-op), Fast, Balanced, Aggressive.
     */
    static QList<UftPreset> forFormat(uft_disk_format_id_t fmt);

    /**
     * @brief Find a preset by id inside a list.
     */
    static const UftPreset* findById(const QList<UftPreset> &list, const QString &id);
};
