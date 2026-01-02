/**
 * @file argmapper.h
 * @brief Backend argument mapper + GUI output plan.
 *
 * Weg B | Step 11: Backend Arg Mapper
 * ----------------------------------
 * The GUI stores format/recovery/export options as JSON blobs that are validated
 * against uft_param_def_t schemas (see ParamSchema). For an actual decode run we
 * need two things:
 *
 *  1) A typed configuration object (for internal backend or for persisting a
 *     reproducible "run profile").
 *  2) A stable, namespaced argument list to drive external CLI wrappers.
 *
 * Additionally, the GUI needs to know which output artifacts will be produced
 * (primary image + sidecars like status maps), to present them to the user.
 */

#pragma once

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>

extern "C" {
#include "uft/uft_formats.h"
#include "uft/uft_output.h"
#include "uft/uft_params.h"
}

/**
 * @brief Describes an output artifact produced by a run.
 */
struct UftOutputArtifact
{
    QString id;      ///< stable id (e.g. "image", "map", "log")
    QString label;   ///< human label for GUI
    QString path;    ///< absolute or relative path
    QString kind;    ///< "image", "json", "text", "binary"
};

/**
 * @brief A complete backend execution plan.
 */
struct UftRunPlan
{
    QJsonObject config;       ///< {format:{}, recovery:{}, output:{}, meta:{}}
    QStringList args;         ///< namespaced arguments: --format.key= --recovery.key=
    QList<UftOutputArtifact> artifacts;
};

class BackendArgMapper
{
public:
    /**
     * @brief Build a run plan from already-sanitized JSON blobs.
     *
     * @param fmt Disk format id.
     * @param outFmt Output container.
     * @param formatJson Sanitized JSON (format-specific).
     * @param recoveryJson Sanitized JSON (recovery/decode).
     * @param outputJson Sanitized JSON (export/output).
     * @param outputDir Output directory (may be empty).
     * @param baseName Base filename without extension (may be empty, a safe default is generated).
     */
    static UftRunPlan buildPlan(uft_disk_format_id_t fmt,
                               uft_output_format_t outFmt,
                               const QString &formatJson,
                               const QString &recoveryJson,
                               const QString &outputJson,
                               const QString &outputDir,
                               const QString &baseName);

    /**
     * @brief Convenience: produce only the args preview string.
     */
    static QString buildArgsPreview(uft_disk_format_id_t fmt,
                                   uft_output_format_t outFmt,
                                   const QString &formatJson,
                                   const QString &recoveryJson,
                                   const QString &outputJson);
};
