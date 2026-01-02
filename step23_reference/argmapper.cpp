#include "argmapper.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonParseError>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>

#include <algorithm>

static QJsonObject parseObjectOrEmpty(const QString &json)
{
    const QString t = json.trimmed();
    if (t.isEmpty()) return QJsonObject{};
    QJsonParseError pe{};
    const auto doc = QJsonDocument::fromJson(t.toUtf8(), &pe);
    if (pe.error == QJsonParseError::NoError && doc.isObject()) {
        return doc.object();
    }
    return QJsonObject{};
}

static QJsonValue coerceValue(const uft_param_def_t &d, const QJsonValue &v)
{
    switch (d.type) {
    case UFT_PARAM_BOOL:
        if (v.isBool()) return v;
        if (v.isDouble()) return QJsonValue(v.toDouble() != 0.0);
        if (v.isString()) {
            const auto s = v.toString().trimmed().toLower();
            return QJsonValue(s == "true" || s == "1" || s == "yes" || s == "on");
        }
        return QJsonValue(false);

    case UFT_PARAM_INT: {
        int iv = 0;
        if (v.isDouble()) iv = (int)v.toDouble();
        else if (v.isString()) iv = v.toString().toInt();
        // clamp
        if (d.max_value > d.min_value) {
            if (iv < (int)d.min_value) iv = (int)d.min_value;
            if (iv > (int)d.max_value) iv = (int)d.max_value;
        }
        return QJsonValue(iv);
    }

    case UFT_PARAM_FLOAT: {
        double fv = 0.0;
        if (v.isDouble()) fv = v.toDouble();
        else if (v.isString()) fv = v.toString().toDouble();
        if (d.max_value > d.min_value) {
            if (fv < d.min_value) fv = d.min_value;
            if (fv > d.max_value) fv = d.max_value;
        }
        return QJsonValue(fv);
    }

    case UFT_PARAM_ENUM:
    case UFT_PARAM_STRING:
    default:
        if (v.isString()) return v;
        if (v.isDouble()) return QJsonValue(QString::number(v.toDouble(), 'g', 8));
        if (v.isBool()) return QJsonValue(v.toBool() ? "true" : "false");
        return QJsonValue(QString());
    }
}

static QJsonObject objectFromSchemaJson(const uft_param_def_t *defs, size_t n, const QString &json)
{
    QJsonObject in = parseObjectOrEmpty(json);
    QJsonObject out;
    if (!defs || n == 0) return out;

    for (size_t i = 0; i < n; ++i) {
        const uft_param_def_t &d = defs[i];
        if (!d.key || !*d.key) continue;
        const QString k = QString::fromUtf8(d.key);

        // Prefer provided value, otherwise use schema default.
        QJsonValue v = in.contains(k) ? in.value(k) : QJsonValue(QString::fromUtf8(d.default_value ? d.default_value : ""));
        v = coerceValue(d, v);

        // For enums: enforce domain when possible.
        if (d.type == UFT_PARAM_ENUM && d.enum_values && d.enum_count > 0) {
            const QString s = v.toString();
            bool ok = false;
            for (size_t ei = 0; ei < d.enum_count; ++ei) {
                const char *ev = d.enum_values[ei];
                if (ev && s == QString::fromUtf8(ev)) {
                    ok = true;
                    break;
                }
            }
            if (!ok) {
                v = QJsonValue(QString::fromUtf8(d.default_value ? d.default_value : ""));
            }
        }

        out.insert(k, v);
    }

    return out;
}

static QStringList argsFromObject(const QString &prefix, const QJsonObject &obj)
{
    QStringList out;

    // Deterministic ordering is important: profiles should reproduce the same
    // arg list across runs/platforms.
    QStringList keys;
    keys.reserve((int)obj.size());
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        keys.push_back(it.key());
    }
    std::sort(keys.begin(), keys.end(), [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });

    for (const QString &k : keys) {
        const QString key = prefix + k;
        const QJsonValue v = obj.value(k);
        QString val;
        if (v.isBool()) val = v.toBool() ? "true" : "false";
        else if (v.isDouble()) val = QString::number(v.toDouble(), 'g', 10);
        else val = v.toString();
        out << QStringLiteral("--%1=%2").arg(key, val);
    }
    return out;
}

static QString safeDefaultBaseName(uft_disk_format_id_t fmt, uft_output_format_t outFmt)
{
    const QString fmtName = QString::fromUtf8(uft_format_name(fmt));
    const QString outName = QString::fromUtf8(uft_output_format_name(outFmt));

    QString base = QStringLiteral("dump_%1_%2").arg(fmtName, outName);
    base.replace(' ', '_');
    base.replace('/', '_');
    base.replace('\\', '_');
    return base;
}

static QList<UftOutputArtifact> buildArtifacts(uft_output_format_t outFmt,
                                               const QJsonObject &recovery,
                                               const QJsonObject &output,
                                               const QString &outDir,
                                               const QString &baseName)
{
    QList<UftOutputArtifact> arts;

    const QString ext = QString::fromUtf8(uft_output_format_ext(outFmt));
    const QString dir = outDir.trimmed().isEmpty() ? QStringLiteral(".") : QDir(outDir).absolutePath();
    const QString mainPath = QDir(dir).filePath(baseName + "." + ext);

    arts.push_back({
        QStringLiteral("image"),
        QStringLiteral("Primary output"),
        mainPath,
        QStringLiteral("binary")
    });

    // Amiga-focused analysis sidecar (boot block checksum/DOS type).
    // This is cheap, deterministic, and gives the GUI a much better summary.
    if (outFmt == UFT_OUTPUT_AMIGA_ADF) {
        arts.push_back({
            QStringLiteral("amiga"),
            QStringLiteral("Amiga analysis"),
            QDir(dir).filePath(baseName + ".amiga.json"),
            QStringLiteral("json")
        });

        // Optional bootblock virus scan report. We keep this as a separate
        // artifact so the GUI can show it in a dedicated panel.
        const bool virusScan = output.value("virus_scan").toBool(true);
        if (virusScan) {
            arts.push_back({
                QStringLiteral("amiga_virus"),
                QStringLiteral("Amiga virus scan"),
                QDir(dir).filePath(baseName + ".amiga.virus.json"),
                QStringLiteral("json")
            });
        }
    }

    // GUI map: recovery.emit_map is a backend artifact used for visualization.
    const bool emitMap = recovery.value("emit_map").toBool(true);
    if (emitMap) {
        arts.push_back({
            QStringLiteral("map"),
            QStringLiteral("Sector status map (GUI)"),
            QDir(dir).filePath(baseName + ".map.json"),
            QStringLiteral("json")
        });
    }

    // Export sidecar maps (write_map) for various containers.
    const bool writeMap = output.value("write_map").toBool(true);
    if (writeMap) {
        arts.push_back({
            QStringLiteral("export_map"),
            QStringLiteral("Export status report"),
            QDir(dir).filePath(baseName + ".export.json"),
            QStringLiteral("json")
        });
    }

    // Metrics/log/profile are GUI-first artifacts: they let the GUI show a
    // credible summary and make runs reproducible.
    const bool emitMetrics = recovery.value("emit_metrics").toBool(true);
    if (emitMetrics) {
        arts.push_back({
            QStringLiteral("metrics"),
            QStringLiteral("Run metrics"),
            QDir(dir).filePath(baseName + ".metrics.json"),
            QStringLiteral("json")
        });
    }

    const bool emitLog = recovery.value("emit_log").toBool(true);
    if (emitLog) {
        arts.push_back({
            QStringLiteral("log"),
            QStringLiteral("Run log"),
            QDir(dir).filePath(baseName + ".log.txt"),
            QStringLiteral("text")
        });
    }

    const bool writeProfile = output.value("write_profile").toBool(true);
    if (writeProfile) {
        arts.push_back({
            QStringLiteral("profile"),
            QStringLiteral("Effective profile (reproducible)"),
            QDir(dir).filePath(baseName + ".profile.json"),
            QStringLiteral("json")
        });
    }

    return arts;
}

UftRunPlan BackendArgMapper::buildPlan(uft_disk_format_id_t fmt,
                                       uft_output_format_t outFmt,
                                       const QString &formatJson,
                                       const QString &recoveryJson,
                                       const QString &outputJson,
                                       const QString &outputDir,
                                       const QString &baseName)
{
    UftRunPlan plan;

    // Build typed objects (schema-driven)
    {
        size_t n = 0;
        const uft_param_def_t *defs = uft_format_param_defs(fmt, &n);
        plan.config.insert(QStringLiteral("format"), objectFromSchemaJson(defs, n, formatJson));
    }
    {
        size_t n = 0;
        const uft_param_def_t *defs = uft_recovery_param_defs(&n);
        plan.config.insert(QStringLiteral("recovery"), objectFromSchemaJson(defs, n, recoveryJson));
    }
    {
        size_t n = 0;
        const uft_param_def_t *defs = uft_output_param_defs(outFmt, &n);
        plan.config.insert(QStringLiteral("output"), objectFromSchemaJson(defs, n, outputJson));
    }

    // Meta
    QJsonObject meta;
    meta.insert(QStringLiteral("format_id"), (int)fmt);
    meta.insert(QStringLiteral("format_name"), QString::fromUtf8(uft_format_name(fmt)));
    meta.insert(QStringLiteral("output_id"), (int)outFmt);
    meta.insert(QStringLiteral("output_name"), QString::fromUtf8(uft_output_format_name(outFmt)));
    meta.insert(QStringLiteral("created_utc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    // IO hints for wrappers (GUI can still decide whether it uses a wrapper or
    // an internal backend). These are not part of the schema, but are critical
    // for reproducible runs.
    const QString outDirAbs = outputDir.trimmed().isEmpty() ? QString() : QDir(outputDir).absolutePath();
    const QString base = baseName.trimmed().isEmpty() ? safeDefaultBaseName(fmt, outFmt) : baseName.trimmed();
    meta.insert(QStringLiteral("output_dir"), outDirAbs);
    meta.insert(QStringLiteral("base_name"), base);
    plan.config.insert(QStringLiteral("meta"), meta);

    // Namespaced args: prevent collisions between sections.
    const QJsonObject fmtObj = plan.config.value("format").toObject();
    const QJsonObject recObj = plan.config.value("recovery").toObject();
    const QJsonObject outObj = plan.config.value("output").toObject();

    plan.args << argsFromObject(QStringLiteral("format."), fmtObj);
    plan.args << argsFromObject(QStringLiteral("recovery."), recObj);
    plan.args << argsFromObject(QStringLiteral("output."), outObj);

    // Dedicated IO args (stable names). This avoids consumers parsing meta.
    if (!outDirAbs.isEmpty()) {
        plan.args << QStringLiteral("--io.output_dir=%1").arg(outDirAbs);
    }
    plan.args << QStringLiteral("--io.base_name=%1").arg(base);

    // Output artifacts
    plan.artifacts = buildArtifacts(outFmt, recObj, outObj, outputDir, base);

    // Dedicated IO paths for wrappers (stable keys, no GUI parsing required).
    // We only emit keys for artifacts that are planned for this run.
    for (const auto &a : plan.artifacts) {
        if (a.id == QLatin1String("image")) {
            plan.args << QStringLiteral("--io.output_path=%1").arg(a.path);
        } else if (a.id == QLatin1String("map")) {
            plan.args << QStringLiteral("--io.map_path=%1").arg(a.path);
        } else if (a.id == QLatin1String("export_map")) {
            plan.args << QStringLiteral("--io.export_map_path=%1").arg(a.path);
        } else if (a.id == QLatin1String("metrics")) {
            plan.args << QStringLiteral("--io.metrics_path=%1").arg(a.path);
        } else if (a.id == QLatin1String("log")) {
            plan.args << QStringLiteral("--io.log_path=%1").arg(a.path);
        } else if (a.id == QLatin1String("profile")) {
            plan.args << QStringLiteral("--io.profile_path=%1").arg(a.path);
        }
    }

    // Add output file paths to meta for consumers.
    QJsonArray arts;
    for (const auto &a : plan.artifacts) {
        QJsonObject o;
        o.insert("id", a.id);
        o.insert("label", a.label);
        o.insert("path", a.path);
        o.insert("kind", a.kind);
        arts.push_back(o);
    }
    meta.insert("artifacts", arts);
    plan.config.insert(QStringLiteral("meta"), meta);

    return plan;
}

QString BackendArgMapper::buildArgsPreview(uft_disk_format_id_t fmt,
                                          uft_output_format_t outFmt,
                                          const QString &formatJson,
                                          const QString &recoveryJson,
                                          const QString &outputJson)
{
    // No output dir / baseName needed for preview
    const UftRunPlan p = buildPlan(fmt, outFmt, formatJson, recoveryJson, outputJson, QString(), QString());
    return p.args.join(' ');
}
