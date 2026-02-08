/**
 * @file UftParameterModel.cpp
 * @brief Qt Parameter Model Implementation
 * 
 * P1-004: GUI Parameter Bidirektional
 */

#include "UftParameterModel.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QDebug>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constructor / Destructor
 * ═══════════════════════════════════════════════════════════════════════════════ */

UftParameterModel::UftParameterModel(QObject *parent)
    : QObject(parent)
{
    loadDefaults();
}

UftParameterModel::~UftParameterModel()
{
    /* Backend cleanup if needed */
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Setters with Change Tracking
 * ═══════════════════════════════════════════════════════════════════════════════ */

void UftParameterModel::setInputPath(const QString &path)
{
    if (m_inputPath != path) {
        recordChange("inputPath", m_inputPath, path);
        m_inputPath = path;
        emit inputPathChanged(path);
        markModified();
    }
}

void UftParameterModel::setOutputPath(const QString &path)
{
    if (m_outputPath != path) {
        recordChange("outputPath", m_outputPath, path);
        m_outputPath = path;
        emit outputPathChanged(path);
        markModified();
    }
}

void UftParameterModel::setVerbose(bool v)
{
    if (m_verbose != v) {
        recordChange("verbose", m_verbose, v);
        m_verbose = v;
        emit verboseChanged(v);
        markModified();
    }
}

void UftParameterModel::setQuiet(bool q)
{
    if (m_quiet != q) {
        recordChange("quiet", m_quiet, q);
        m_quiet = q;
        emit quietChanged(q);
        markModified();
    }
}

void UftParameterModel::setFormat(const QString &fmt)
{
    if (m_format != fmt) {
        recordChange("format", m_format, fmt);
        m_format = fmt;
        emit formatChanged(fmt);
        markModified();
    }
}

void UftParameterModel::setCylinders(int c)
{
    c = qBound(1, c, 200);
    if (m_cylinders != c) {
        recordChange("cylinders", m_cylinders, c);
        m_cylinders = c;
        emit cylindersChanged(c);
        markModified();
    }
}

void UftParameterModel::setHeads(int h)
{
    h = qBound(1, h, 2);
    if (m_heads != h) {
        recordChange("heads", m_heads, h);
        m_heads = h;
        emit headsChanged(h);
        markModified();
    }
}

void UftParameterModel::setSectors(int s)
{
    s = qBound(1, s, 64);
    if (m_sectors != s) {
        recordChange("sectors", m_sectors, s);
        m_sectors = s;
        emit sectorsChanged(s);
        markModified();
    }
}

void UftParameterModel::setEncoding(const QString &enc)
{
    if (m_encoding != enc) {
        recordChange("encoding", m_encoding, enc);
        m_encoding = enc;
        emit encodingChanged(enc);
        markModified();
    }
}

void UftParameterModel::setHardware(const QString &hw)
{
    if (m_hardware != hw) {
        recordChange("hardware", m_hardware, hw);
        m_hardware = hw;
        emit hardwareChanged(hw);
        markModified();
    }
}

void UftParameterModel::setDevicePath(const QString &path)
{
    if (m_devicePath != path) {
        recordChange("devicePath", m_devicePath, path);
        m_devicePath = path;
        emit devicePathChanged(path);
        markModified();
    }
}

void UftParameterModel::setDriveNumber(int d)
{
    d = qBound(0, d, 3);
    if (m_driveNumber != d) {
        recordChange("driveNumber", m_driveNumber, d);
        m_driveNumber = d;
        emit driveNumberChanged(d);
        markModified();
    }
}

void UftParameterModel::setRetries(int r)
{
    r = qBound(0, r, 100);
    if (m_retries != r) {
        recordChange("retries", m_retries, r);
        m_retries = r;
        emit retriesChanged(r);
        markModified();
    }
}

void UftParameterModel::setRevolutions(int r)
{
    r = qBound(1, r, 20);
    if (m_revolutions != r) {
        recordChange("revolutions", m_revolutions, r);
        m_revolutions = r;
        emit revolutionsChanged(r);
        markModified();
    }
}

void UftParameterModel::setWeakBits(bool w)
{
    if (m_weakBits != w) {
        recordChange("weakBits", m_weakBits, w);
        m_weakBits = w;
        emit weakBitsChanged(w);
        markModified();
    }
}

void UftParameterModel::setPllPhaseGain(double g)
{
    g = qBound(0.01, g, 1.0);
    if (!qFuzzyCompare(m_pllPhaseGain, g)) {
        recordChange("pllPhaseGain", m_pllPhaseGain, g);
        m_pllPhaseGain = g;
        emit pllPhaseGainChanged(g);
        markModified();
    }
}

void UftParameterModel::setPllFreqGain(double g)
{
    g = qBound(0.001, g, 0.5);
    if (!qFuzzyCompare(m_pllFreqGain, g)) {
        recordChange("pllFreqGain", m_pllFreqGain, g);
        m_pllFreqGain = g;
        emit pllFreqGainChanged(g);
        markModified();
    }
}

void UftParameterModel::setPllWindowTolerance(double t)
{
    t = qBound(0.1, t, 0.5);
    if (!qFuzzyCompare(m_pllWindowTolerance, t)) {
        recordChange("pllWindowTolerance", m_pllWindowTolerance, t);
        m_pllWindowTolerance = t;
        emit pllWindowToleranceChanged(t);
        markModified();
    }
}

void UftParameterModel::setPllPreset(const QString &preset)
{
    if (m_pllPreset != preset) {
        recordChange("pllPreset", m_pllPreset, preset);
        m_pllPreset = preset;
        emit pllPresetChanged(preset);
        markModified();
        
        /* Apply preset values */
        if (preset == "Amiga DD") {
            m_pllPhaseGain = 0.10; m_pllFreqGain = 0.05;
        } else if (preset == "Amiga HD") {
            m_pllPhaseGain = 0.08; m_pllFreqGain = 0.04;
        } else if (preset == "C64 GCR") {
            m_pllPhaseGain = 0.12; m_pllFreqGain = 0.06;
        } else if (preset == "Apple II") {
            m_pllPhaseGain = 0.15; m_pllFreqGain = 0.08;
        } else if (preset == "IBM PC DD") {
            m_pllPhaseGain = 0.10; m_pllFreqGain = 0.05;
        } else if (preset == "IBM PC HD") {
            m_pllPhaseGain = 0.08; m_pllFreqGain = 0.04;
        }
        emit pllPhaseGainChanged(m_pllPhaseGain);
        emit pllFreqGainChanged(m_pllFreqGain);
    }
}

void UftParameterModel::setVerifyAfterWrite(bool v)
{
    if (m_verifyAfterWrite != v) {
        recordChange("verifyAfterWrite", m_verifyAfterWrite, v);
        m_verifyAfterWrite = v;
        emit verifyAfterWriteChanged(v);
        markModified();
    }
}

void UftParameterModel::setWriteRetries(int r)
{
    r = qBound(0, r, 10);
    if (m_writeRetries != r) {
        recordChange("writeRetries", m_writeRetries, r);
        m_writeRetries = r;
        emit writeRetriesChanged(r);
        markModified();
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Validation
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool UftParameterModel::isValid() const
{
    /* Basic validation */
    if (m_inputPath.isEmpty()) return false;
    if (m_cylinders < 1 || m_cylinders > 200) return false;
    if (m_heads < 1 || m_heads > 2) return false;
    if (m_sectors < 1 || m_sectors > 64) return false;
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Actions
 * ═══════════════════════════════════════════════════════════════════════════════ */

void UftParameterModel::reset()
{
    loadDefaults();
    m_history.clear();
    m_historyIndex = -1;
    m_modified = false;
    emit modifiedChanged(false);
}

void UftParameterModel::loadDefaults()
{
    m_inputPath.clear();
    m_outputPath.clear();
    m_verbose = false;
    m_quiet = false;
    m_format = "auto";
    m_cylinders = 80;
    m_heads = 2;
    m_sectors = 18;
    m_encoding = "auto";
    m_hardware = "auto";
    m_devicePath.clear();
    m_driveNumber = 0;
    m_retries = 3;
    m_revolutions = 3;
    m_weakBits = true;
    m_pllPhaseGain = 0.10;
    m_pllFreqGain = 0.05;
    m_pllWindowTolerance = 0.40;
    m_pllPreset = "Amiga DD";
    m_verifyAfterWrite = true;
    m_writeRetries = 3;
}

bool UftParameterModel::loadFromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        emit errorOccurred(QString("Cannot open file: %1").arg(path));
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull()) {
        emit errorOccurred("Invalid JSON format");
        return false;
    }
    
    QJsonObject obj = doc.object();
    
    /* Load all parameters */
    if (obj.contains("inputPath")) setInputPath(obj["inputPath"].toString());
    if (obj.contains("outputPath")) setOutputPath(obj["outputPath"].toString());
    if (obj.contains("verbose")) setVerbose(obj["verbose"].toBool());
    if (obj.contains("quiet")) setQuiet(obj["quiet"].toBool());
    if (obj.contains("format")) setFormat(obj["format"].toString());
    if (obj.contains("cylinders")) setCylinders(obj["cylinders"].toInt());
    if (obj.contains("heads")) setHeads(obj["heads"].toInt());
    if (obj.contains("sectors")) setSectors(obj["sectors"].toInt());
    if (obj.contains("encoding")) setEncoding(obj["encoding"].toString());
    if (obj.contains("hardware")) setHardware(obj["hardware"].toString());
    if (obj.contains("devicePath")) setDevicePath(obj["devicePath"].toString());
    if (obj.contains("driveNumber")) setDriveNumber(obj["driveNumber"].toInt());
    if (obj.contains("retries")) setRetries(obj["retries"].toInt());
    if (obj.contains("revolutions")) setRevolutions(obj["revolutions"].toInt());
    if (obj.contains("weakBits")) setWeakBits(obj["weakBits"].toBool());
    if (obj.contains("pllPhaseGain")) setPllPhaseGain(obj["pllPhaseGain"].toDouble());
    if (obj.contains("pllFreqGain")) setPllFreqGain(obj["pllFreqGain"].toDouble());
    if (obj.contains("pllWindowTolerance")) setPllWindowTolerance(obj["pllWindowTolerance"].toDouble());
    if (obj.contains("pllPreset")) setPllPreset(obj["pllPreset"].toString());
    if (obj.contains("verifyAfterWrite")) setVerifyAfterWrite(obj["verifyAfterWrite"].toBool());
    if (obj.contains("writeRetries")) setWriteRetries(obj["writeRetries"].toInt());
    
    m_modified = false;
    emit modifiedChanged(false);
    return true;
}

bool UftParameterModel::saveToFile(const QString &path)
{
    QJsonObject obj;
    
    obj["inputPath"] = m_inputPath;
    obj["outputPath"] = m_outputPath;
    obj["verbose"] = m_verbose;
    obj["quiet"] = m_quiet;
    obj["format"] = m_format;
    obj["cylinders"] = m_cylinders;
    obj["heads"] = m_heads;
    obj["sectors"] = m_sectors;
    obj["encoding"] = m_encoding;
    obj["hardware"] = m_hardware;
    obj["devicePath"] = m_devicePath;
    obj["driveNumber"] = m_driveNumber;
    obj["retries"] = m_retries;
    obj["revolutions"] = m_revolutions;
    obj["weakBits"] = m_weakBits;
    obj["pllPhaseGain"] = m_pllPhaseGain;
    obj["pllFreqGain"] = m_pllFreqGain;
    obj["pllWindowTolerance"] = m_pllWindowTolerance;
    obj["pllPreset"] = m_pllPreset;
    obj["verifyAfterWrite"] = m_verifyAfterWrite;
    obj["writeRetries"] = m_writeRetries;
    
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        emit errorOccurred(QString("Cannot write file: %1").arg(path));
        return false;
    }
    
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    m_modified = false;
    emit modifiedChanged(false);
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Backend Sync
 * ═══════════════════════════════════════════════════════════════════════════════ */

void UftParameterModel::syncToBackend()
{
    /* Sync Qt model values to C backend struct */
    /* Note: Full implementation requires uft_param_bridge.h linking */
    
#ifdef UFT_HAS_PARAM_BRIDGE
    if (!m_backendParams) return;
    
    /* Format Parameters */
    uft_params_set_int(m_backendParams, "cylinders", m_cylinders);
    uft_params_set_int(m_backendParams, "heads", m_heads);
    uft_params_set_int(m_backendParams, "sectors", m_sectors);
    uft_params_set_string(m_backendParams, "format", m_format.toUtf8().constData());
    uft_params_set_string(m_backendParams, "encoding", m_encoding.toUtf8().constData());
    
    /* Hardware Parameters */
    uft_params_set_string(m_backendParams, "hardware", m_hardware.toUtf8().constData());
    uft_params_set_string(m_backendParams, "devicePath", m_devicePath.toUtf8().constData());
    uft_params_set_int(m_backendParams, "driveNumber", m_driveNumber);
    
    /* Recovery Parameters */
    uft_params_set_int(m_backendParams, "retries", m_retries);
    uft_params_set_int(m_backendParams, "revolutions", m_revolutions);
    uft_params_set_bool(m_backendParams, "weakBits", m_weakBits);
    
    /* PLL Parameters */
    uft_params_set_float(m_backendParams, "pllPhaseGain", (float)m_pllPhaseGain);
    uft_params_set_float(m_backendParams, "pllFreqGain", (float)m_pllFreqGain);
    uft_params_set_float(m_backendParams, "pllWindowTolerance", (float)m_pllWindowTolerance);
    
    /* Write Parameters */
    uft_params_set_bool(m_backendParams, "verifyAfterWrite", m_verifyAfterWrite);
    uft_params_set_int(m_backendParams, "writeRetries", m_writeRetries);
#endif
    
    qDebug() << "Synced to backend: cylinders=" << m_cylinders 
             << "heads=" << m_heads << "format=" << m_format;
    
    emit backendSynced();
}

void UftParameterModel::syncFromBackend()
{
    /* Sync C backend struct values to Qt model */

#ifdef UFT_HAS_PARAM_BRIDGE
    if (!m_backendParams) return;
    
    m_cylinders = uft_params_get_int(m_backendParams, "cylinders");
    m_heads = uft_params_get_int(m_backendParams, "heads");
    m_sectors = uft_params_get_int(m_backendParams, "sectors");
    m_retries = uft_params_get_int(m_backendParams, "retries");
    m_revolutions = uft_params_get_int(m_backendParams, "revolutions");
    m_weakBits = uft_params_get_bool(m_backendParams, "weakBits");
    m_driveNumber = uft_params_get_int(m_backendParams, "driveNumber");
    m_verifyAfterWrite = uft_params_get_bool(m_backendParams, "verifyAfterWrite");
    m_writeRetries = uft_params_get_int(m_backendParams, "writeRetries");
    
    const char *fmt = uft_params_get_string(m_backendParams, "format");
    if (fmt) m_format = QString::fromUtf8(fmt);
    
    const char *enc = uft_params_get_string(m_backendParams, "encoding");
    if (enc) m_encoding = QString::fromUtf8(enc);
    
    /* Emit signals */
    emit cylindersChanged(m_cylinders);
    emit headsChanged(m_heads);
    emit sectorsChanged(m_sectors);
    emit formatChanged(m_format);
    emit encodingChanged(m_encoding);
    emit retriesChanged(m_retries);
    emit revolutionsChanged(m_revolutions);
    emit weakBitsChanged(m_weakBits);
#endif
    
    qDebug() << "Synced from backend";
    emit backendSynced();
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Undo/Redo
 * ═══════════════════════════════════════════════════════════════════════════════ */

void UftParameterModel::recordChange(const QString &name, const QVariant &oldVal, const QVariant &newVal)
{
    /* Remove any redo history */
    while (m_history.size() > m_historyIndex + 1) {
        m_history.removeLast();
    }
    
    UftParamChange change;
    change.name = name;
    change.oldValue = oldVal;
    change.newValue = newVal;
    change.timestamp = QDateTime::currentMSecsSinceEpoch();
    
    m_history.append(change);
    m_historyIndex = m_history.size() - 1;
    
    /* Limit history size */
    while (m_history.size() > 100) {
        m_history.removeFirst();
        m_historyIndex--;
    }
    
    emit parameterChanged(name, oldVal, newVal);
}

void UftParameterModel::undo()
{
    if (!canUndo()) return;
    
    const UftParamChange &change = m_history[m_historyIndex];
    setValue(change.name, change.oldValue);
    m_historyIndex--;
}

void UftParameterModel::redo()
{
    if (!canRedo()) return;
    
    m_historyIndex++;
    const UftParamChange &change = m_history[m_historyIndex];
    setValue(change.name, change.newValue);
}

bool UftParameterModel::canUndo() const
{
    return m_historyIndex >= 0;
}

bool UftParameterModel::canRedo() const
{
    return m_historyIndex < m_history.size() - 1;
}

void UftParameterModel::markModified()
{
    if (!m_modified) {
        m_modified = true;
        emit modifiedChanged(true);
    }
    emit validChanged(isValid());
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Generic Access
 * ═══════════════════════════════════════════════════════════════════════════════ */

QVariant UftParameterModel::getValue(const QString &name) const
{
    if (name == "inputPath") return m_inputPath;
    if (name == "outputPath") return m_outputPath;
    if (name == "verbose") return m_verbose;
    if (name == "quiet") return m_quiet;
    if (name == "format") return m_format;
    if (name == "cylinders") return m_cylinders;
    if (name == "heads") return m_heads;
    if (name == "sectors") return m_sectors;
    if (name == "encoding") return m_encoding;
    if (name == "hardware") return m_hardware;
    if (name == "devicePath") return m_devicePath;
    if (name == "driveNumber") return m_driveNumber;
    if (name == "retries") return m_retries;
    if (name == "revolutions") return m_revolutions;
    if (name == "weakBits") return m_weakBits;
    if (name == "pllPhaseGain") return m_pllPhaseGain;
    if (name == "pllFreqGain") return m_pllFreqGain;
    if (name == "pllWindowTolerance") return m_pllWindowTolerance;
    if (name == "pllPreset") return m_pllPreset;
    if (name == "verifyAfterWrite") return m_verifyAfterWrite;
    if (name == "writeRetries") return m_writeRetries;
    return QVariant();
}

bool UftParameterModel::setValue(const QString &name, const QVariant &value)
{
    if (name == "inputPath") { setInputPath(value.toString()); return true; }
    if (name == "outputPath") { setOutputPath(value.toString()); return true; }
    if (name == "verbose") { setVerbose(value.toBool()); return true; }
    if (name == "quiet") { setQuiet(value.toBool()); return true; }
    if (name == "format") { setFormat(value.toString()); return true; }
    if (name == "cylinders") { setCylinders(value.toInt()); return true; }
    if (name == "heads") { setHeads(value.toInt()); return true; }
    if (name == "sectors") { setSectors(value.toInt()); return true; }
    if (name == "encoding") { setEncoding(value.toString()); return true; }
    if (name == "hardware") { setHardware(value.toString()); return true; }
    if (name == "devicePath") { setDevicePath(value.toString()); return true; }
    if (name == "driveNumber") { setDriveNumber(value.toInt()); return true; }
    if (name == "retries") { setRetries(value.toInt()); return true; }
    if (name == "revolutions") { setRevolutions(value.toInt()); return true; }
    if (name == "weakBits") { setWeakBits(value.toBool()); return true; }
    if (name == "pllPhaseGain") { setPllPhaseGain(value.toDouble()); return true; }
    if (name == "pllFreqGain") { setPllFreqGain(value.toDouble()); return true; }
    if (name == "pllWindowTolerance") { setPllWindowTolerance(value.toDouble()); return true; }
    if (name == "pllPreset") { setPllPreset(value.toString()); return true; }
    if (name == "verifyAfterWrite") { setVerifyAfterWrite(value.toBool()); return true; }
    if (name == "writeRetries") { setWriteRetries(value.toInt()); return true; }
    return false;
}

QStringList UftParameterModel::parameterNames() const
{
    return QStringList{
        "inputPath", "outputPath", "verbose", "quiet",
        "format", "cylinders", "heads", "sectors", "encoding",
        "hardware", "devicePath", "driveNumber",
        "retries", "revolutions", "weakBits",
        "pllPhaseGain", "pllFreqGain", "pllWindowTolerance", "pllPreset",
        "verifyAfterWrite", "writeRetries"
    };
}
