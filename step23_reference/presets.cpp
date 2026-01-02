#include "presets.h"

#include "paramschema.h"

#include <QJsonDocument>
#include <QJsonObject>

extern "C" {
#include "uft/uft_params.h"
}

static QJsonObject parseObj(const QString &j)
{
    const QByteArray b = j.trimmed().toUtf8();
    if (b.isEmpty()) return QJsonObject();
    const QJsonDocument doc = QJsonDocument::fromJson(b);
    if (!doc.isObject()) return QJsonObject();
    return doc.object();
}

static QString toCompact(const QJsonObject &o)
{
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

static QString defaultsForRecovery()
{
    size_t n = 0;
    const uft_param_def_t *defs = uft_recovery_param_defs(&n);
    return ParamSchema::defaults(defs, n);
}

static QString defaultsForFormat(uft_disk_format_id_t fmt)
{
    size_t n = 0;
    const uft_param_def_t *defs = uft_format_param_defs(fmt, &n);
    return ParamSchema::defaults(defs, n);
}

static QString defaultsForOutput(uft_output_format_t fmt)
{
    size_t n = 0;
    const uft_param_def_t *defs = uft_output_param_defs(fmt, &n);
    return ParamSchema::defaults(defs, n);
}

static QString merge(const QString &baseJson, const QJsonObject &overrides)
{
    QJsonObject o = parseObj(baseJson);
    for (auto it = overrides.begin(); it != overrides.end(); ++it) {
        o.insert(it.key(), it.value());
    }
    return toCompact(o);
}

static QHash<uft_output_format_t, QString> outputDefaultsForSpec(const uft_format_spec_t *spec)
{
    QHash<uft_output_format_t, QString> out;
    if (!spec) return out;

    // For UI presets we only care about output formats that are actually offered.
    uft_output_format_t list[16];
    const size_t n = uft_output_mask_to_list(spec->output_mask, list, sizeof(list)/sizeof(list[0]));
    for (size_t i = 0; i < n; ++i) {
        out.insert(list[i], defaultsForOutput(list[i]));
    }
    // Always keep RAW around.
    if (!out.contains(UFT_OUTPUT_RAW_IMG)) {
        out.insert(UFT_OUTPUT_RAW_IMG, defaultsForOutput(UFT_OUTPUT_RAW_IMG));
    }
    return out;
}

static void applyCommonFast(QJsonObject &r)
{
    r.insert("passes", 1);
    r.insert("offset_steps", 0);
    r.insert("pll_bandwidth", 0.20);
    r.insert("jitter_ns", 120);
    r.insert("splice_mode", "best-crc");
}

static void applyCommonBalanced(QJsonObject &r)
{
    r.insert("passes", 3);
    r.insert("offset_steps", 3);
    r.insert("pll_bandwidth", 0.25);
    r.insert("jitter_ns", 150);
    r.insert("vote_threshold", 0.55);
    r.insert("splice_mode", "vote");
}

static void applyCommonAggressive(QJsonObject &r)
{
    r.insert("passes", 8);
    r.insert("offset_steps", 6);
    r.insert("pll_bandwidth", 0.55);
    r.insert("jitter_ns", 250);
    r.insert("vote_threshold", 0.52);
    r.insert("splice_mode", "hybrid");
    r.insert("emit_map", true);
    r.insert("emit_metrics", true);
    r.insert("emit_log", true);
}

static void tuneFormatForAggressive(uft_disk_format_id_t fmt, QJsonObject &f)
{
    // Formats that are timing-sensitive benefit from slightly higher tolerances.
    (void)fmt;
    if (f.contains("mfm_sync_tolerance")) {
        f.insert("mfm_sync_tolerance", 1.4);
    }
    if (f.contains("gcr_tolerance")) {
        f.insert("gcr_tolerance", 1.4);
    }
    if (f.contains("apple2_phase_lock")) {
        f.insert("apple2_phase_lock", true);
    }
    // Let RPM/data-rate stay "auto" by default; users can pin them if needed.
}

static void tuneOutputForAggressive(uft_output_format_t of, QJsonObject &o)
{
    // Make sure we keep the most diagnostics on by default.
    if (o.contains("write_map")) o.insert("write_map", true);
    if (o.contains("write_profile")) o.insert("write_profile", true);

    // Format-specific extras.
    if (of == UFT_OUTPUT_AMIGA_ADF) {
        if (o.contains("validate_bootblock")) o.insert("validate_bootblock", true);
        if (o.contains("virus_scan")) o.insert("virus_scan", true);
    }
}

QList<UftPreset> UftPresets::forFormat(uft_disk_format_id_t fmt)
{
    QList<UftPreset> out;

    const uft_format_spec_t *spec = uft_format_find_by_id(fmt);

    // Custom / no-op. (Still provides defaults to keep GUI state consistent.)
    {
        UftPreset p;
        p.id = "custom";
        p.name = "Custom";
        p.description = "Keep current options (no preset changes)";
        p.recoveryJson = defaultsForRecovery();
        p.formatJson = defaultsForFormat(fmt);
        p.outputJsonByFmt = outputDefaultsForSpec(spec);
        out.append(p);
    }

    // Fast
    {
        UftPreset p;
        p.id = "fast";
        p.name = "Fast";
        p.description = "Quick pass – minimal recovery (good media / verification run)";

        QJsonObject r = parseObj(defaultsForRecovery());
        applyCommonFast(r);
        p.recoveryJson = toCompact(r);

        p.formatJson = defaultsForFormat(fmt);
        p.outputJsonByFmt = outputDefaultsForSpec(spec);
        out.append(p);
    }

    // Balanced
    {
        UftPreset p;
        p.id = "balanced";
        p.name = "Balanced";
        p.description = "Default recovery – sensible for most disks";

        QJsonObject r = parseObj(defaultsForRecovery());
        applyCommonBalanced(r);
        p.recoveryJson = toCompact(r);

        p.formatJson = defaultsForFormat(fmt);
        p.outputJsonByFmt = outputDefaultsForSpec(spec);
        out.append(p);
    }

    // Aggressive
    {
        UftPreset p;
        p.id = "aggressive";
        p.name = "Aggressive";
        p.description = "Many passes + looser timing – best chance on weak media";

        QJsonObject r = parseObj(defaultsForRecovery());
        applyCommonAggressive(r);
        p.recoveryJson = toCompact(r);

        QJsonObject f = parseObj(defaultsForFormat(fmt));
        tuneFormatForAggressive(fmt, f);
        p.formatJson = toCompact(f);

        p.outputJsonByFmt = outputDefaultsForSpec(spec);
        for (auto it = p.outputJsonByFmt.begin(); it != p.outputJsonByFmt.end(); ++it) {
            QJsonObject o = parseObj(it.value());
            tuneOutputForAggressive(it.key(), o);
            it.value() = toCompact(o);
        }

        out.append(p);
    }

    return out;
}

const UftPreset *UftPresets::findById(const QList<UftPreset> &list, const QString &id)
{
    for (const auto &p : list) {
        if (p.id == id) return &p;
    }
    return nullptr;
}
