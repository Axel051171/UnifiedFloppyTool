/**
 * @file paramschema.cpp
 * @brief Implementation of schema-based JSON sanitizer.
 */

#include "paramschema.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

static QJsonValue defaultForDef(const uft_param_def_t &d)
{
    const QString v = d.default_value ? QString::fromUtf8(d.default_value) : QString();

    switch (d.type) {
    case UFT_PARAM_BOOL:
        return QJsonValue(v == "true" || v == "1");
    case UFT_PARAM_INT: {
        bool ok = false;
        const int iv = v.toInt(&ok);
        return ok ? QJsonValue(iv) : QJsonValue(v);
    }
    case UFT_PARAM_FLOAT: {
        bool ok = false;
        const double dv = v.toDouble(&ok);
        return ok ? QJsonValue(dv) : QJsonValue(v);
    }
    case UFT_PARAM_ENUM:
    case UFT_PARAM_STRING:
    default:
        return QJsonValue(v);
    }
}

static bool enumContains(const uft_param_def_t &d, const QString &s)
{
    if (d.type != UFT_PARAM_ENUM || !d.enum_values || d.enum_count == 0) return true;
    for (size_t i = 0; i < d.enum_count; ++i) {
        if (!d.enum_values[i]) continue;
        if (s == QString::fromUtf8(d.enum_values[i])) return true;
    }
    return false;
}

static QJsonValue coerceAndClamp(const uft_param_def_t &d, const QJsonValue &in, QStringList *warnings)
{
    // Constraints are optional; ignore if min>max.
    const bool hasRange = (d.min_value <= d.max_value);

    switch (d.type) {
    case UFT_PARAM_BOOL:
        if (in.isBool()) return in;
        if (in.isDouble()) return QJsonValue(in.toInt() != 0);
        if (in.isString()) {
            const QString s = in.toString().toLower();
            return QJsonValue(s == "true" || s == "1" || s == "yes" || s == "on");
        }
        if (warnings) warnings->push_back(QString("%1: invalid bool, using default").arg(QString::fromUtf8(d.key)));
        return defaultForDef(d);

    case UFT_PARAM_INT: {
        int v = 0;
        bool ok = false;
        if (in.isDouble()) { v = (int)in.toInteger(); ok = true; }
        else if (in.isString()) { v = in.toString().toInt(&ok); }
        else if (in.isBool()) { v = in.toBool() ? 1 : 0; ok = true; }

        if (!ok) {
            if (warnings) warnings->push_back(QString("%1: invalid int, using default").arg(QString::fromUtf8(d.key)));
            return defaultForDef(d);
        }
        if (hasRange) {
            const int minV = (int)d.min_value;
            const int maxV = (int)d.max_value;
            const int clamped = (v < minV) ? minV : (v > maxV ? maxV : v);
            if (clamped != v && warnings) warnings->push_back(QString("%1: clamped to %2").arg(QString::fromUtf8(d.key)).arg(clamped));
            v = clamped;
        }
        return QJsonValue(v);
    }

    case UFT_PARAM_FLOAT: {
        double v = 0.0;
        bool ok = false;
        if (in.isDouble()) { v = in.toDouble(); ok = true; }
        else if (in.isString()) { v = in.toString().toDouble(&ok); }
        else if (in.isBool()) { v = in.toBool() ? 1.0 : 0.0; ok = true; }

        if (!ok) {
            if (warnings) warnings->push_back(QString("%1: invalid float, using default").arg(QString::fromUtf8(d.key)));
            return defaultForDef(d);
        }
        if (hasRange) {
            const double clamped = (v < d.min_value) ? d.min_value : (v > d.max_value ? d.max_value : v);
            if (clamped != v && warnings) warnings->push_back(QString("%1: clamped to %2").arg(QString::fromUtf8(d.key)).arg(clamped));
            v = clamped;
        }
        return QJsonValue(v);
    }

    case UFT_PARAM_ENUM: {
        QString s;
        if (in.isString()) s = in.toString();
        else if (in.isDouble()) s = QString::number(in.toInt());
        else if (in.isBool()) s = in.toBool() ? "true" : "false";
        else s = QString();

        if (!enumContains(d, s)) {
            if (warnings) warnings->push_back(QString("%1: invalid enum '%2', using default").arg(QString::fromUtf8(d.key)).arg(s));
            return defaultForDef(d);
        }
        return QJsonValue(s);
    }

    case UFT_PARAM_STRING:
    default:
        if (in.isString()) return in;
        if (in.isDouble()) return QJsonValue(QString::number(in.toDouble()));
        if (in.isBool()) return QJsonValue(in.toBool() ? "true" : "false");
        return defaultForDef(d);
    }
}

QString ParamSchema::defaults(const uft_param_def_t *defs, size_t count)
{
    QJsonObject obj;
    for (size_t i = 0; i < count; ++i) {
        const uft_param_def_t &d = defs[i];
        if (!d.key) continue;
        obj.insert(QString::fromUtf8(d.key), defaultForDef(d));
    }
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

QString ParamSchema::sanitize(const uft_param_def_t *defs,
                              size_t count,
                              const QString &jsonInput,
                              bool *ok,
                              QStringList *warnings)
{
    if (ok) *ok = false;

    // Parse: must be a JSON object.
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonInput.toUtf8(), &err);
    QJsonObject inObj;

    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        inObj = doc.object();
        if (ok) *ok = true;
    } else {
        if (warnings) warnings->push_back(QString("Invalid JSON (%1), using defaults").arg(err.errorString()));
        // Return defaults for broken JSON.
        return defaults(defs, count);
    }

    // Build normalized object using schema order; unknown keys are dropped.
    QJsonObject out;

    for (size_t i = 0; i < count; ++i) {
        const uft_param_def_t &d = defs[i];
        if (!d.key) continue;

        const QString key = QString::fromUtf8(d.key);
        const QJsonValue v = inObj.contains(key) ? inObj.value(key) : defaultForDef(d);
        out.insert(key, coerceAndClamp(d, v, warnings));
    }

    // Detect unknown keys (helpful for GUI).
    for (auto it = inObj.begin(); it != inObj.end(); ++it) {
        bool known = false;
        for (size_t i = 0; i < count; ++i) {
            if (defs[i].key && it.key() == QString::fromUtf8(defs[i].key)) {
                known = true;
                break;
            }
        }
        if (!known && warnings) warnings->push_back(QString("Unknown key removed: %1").arg(it.key()));
    }

    return QString::fromUtf8(QJsonDocument(out).toJson(QJsonDocument::Compact));
}
