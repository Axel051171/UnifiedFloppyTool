/**
 * @file sessionmanager.cpp
 * @brief Session Management - Full Implementation
 * @version 4.0.0
 */

#include "sessionmanager.h"
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QUuid>
#include <QJsonArray>
#include <QCryptographicHash>

// ============================================================================
// SessionManager Implementation
// ============================================================================

SessionManager::SessionManager(QObject *parent)
    : QObject(parent)
    , m_active(false)
    , m_maxHistory(20)
    , m_autoSaveEnabled(true)
    , m_autoSaveInterval(60)
{
    // Set session directory
    m_sessionDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                   + "/sessions";
    QDir().mkpath(m_sessionDir);
    
    loadHistory();
}

SessionManager::~SessionManager()
{
    if (m_active) {
        endSession(SessionState::CANCELLED);
    }
    saveHistory();
}

// ============================================================================
// Session Lifecycle
// ============================================================================

QString SessionManager::newSession(const QString& operation)
{
    if (m_active) {
        endSession(SessionState::CANCELLED);
    }
    
    m_currentSession = SessionData();
    m_currentSession.id = generateSessionId();
    m_currentSession.version = "4.0.0";
    m_currentSession.startTime = QDateTime::currentDateTime();
    m_currentSession.operation = operation;
    m_currentSession.state = SessionState::IDLE;
    m_currentSession.totalTracks = 0;
    m_currentSession.goodTracks = 0;
    m_currentSession.warningTracks = 0;
    m_currentSession.errorTracks = 0;
    m_currentSession.recoveredTracks = 0;
    m_currentSession.avgConfidence = 0.0;
    
    m_active = true;
    
    emit sessionStarted(m_currentSession.id);
    emit stateChanged(SessionState::IDLE);
    
    return m_currentSession.id;
}

void SessionManager::endSession(SessionState finalState)
{
    if (!m_active) return;
    
    m_currentSession.endTime = QDateTime::currentDateTime();
    m_currentSession.state = finalState;
    
    updateStatistics();
    
    // Auto-save completed sessions
    if (finalState == SessionState::COMPLETE || 
        finalState == SessionState::ERROR) {
        autoSave();
    }
    
    m_active = false;
    
    emit sessionEnded(m_currentSession.id, finalState);
}

void SessionManager::cancelSession()
{
    endSession(SessionState::CANCELLED);
}

// ============================================================================
// State Management
// ============================================================================

void SessionManager::setState(SessionState state)
{
    if (m_currentSession.state != state) {
        m_currentSession.state = state;
        emit stateChanged(state);
    }
}

// ============================================================================
// Hardware
// ============================================================================

void SessionManager::setHardware(const HardwareSnapshot& hw)
{
    m_currentSession.hardware = hw;
    m_currentSession.hardware.timestamp = QDateTime::currentDateTime();
}

// ============================================================================
// Configuration
// ============================================================================

void SessionManager::setProfile(const QString& profile)
{
    m_currentSession.profile = profile;
}

void SessionManager::setFormat(const QString& format)
{
    m_currentSession.format = format;
}

void SessionManager::setParameters(const QJsonObject& params)
{
    m_currentSession.parameters = params;
}

void SessionManager::setTrackOverrides(const QJsonObject& overrides)
{
    m_currentSession.trackOverrides = overrides;
}

// ============================================================================
// Results
// ============================================================================

void SessionManager::addTrackResult(const TrackResult& result)
{
    m_currentSession.tracks.push_back(result);
    updateStatistics();
    
    emit trackCompleted(result.cylinder, result.head, result);
    
    // Calculate progress
    if (m_currentSession.totalTracks > 0) {
        int percent = (m_currentSession.tracks.size() * 100) / m_currentSession.totalTracks;
        emit progressChanged(percent);
    }
}

void SessionManager::updateTrackResult(int cylinder, int head, const TrackResult& result)
{
    for (auto& track : m_currentSession.tracks) {
        if (track.cylinder == cylinder && track.head == head) {
            track = result;
            updateStatistics();
            emit trackCompleted(cylinder, head, result);
            return;
        }
    }
    
    // Not found, add new
    addTrackResult(result);
}

void SessionManager::setOutput(const QString& file, const QString& hash, qint64 size)
{
    m_currentSession.outputFile = file;
    m_currentSession.outputHash = hash;
    m_currentSession.outputSize = size;
}

// ============================================================================
// Notes
// ============================================================================

void SessionManager::addNote(const QString& note)
{
    if (!m_currentSession.userNotes.isEmpty()) {
        m_currentSession.userNotes += "\n";
    }
    m_currentSession.userNotes += note;
}

void SessionManager::addWarning(const QString& warning)
{
    m_currentSession.warnings.append(warning);
}

void SessionManager::addError(const QString& error)
{
    m_currentSession.errors.append(error);
    emit errorOccurred(error);
}

// ============================================================================
// Serialization - JSON
// ============================================================================

QJsonObject SessionManager::toJson() const
{
    QJsonObject obj;
    
    // Metadata
    obj["id"] = m_currentSession.id;
    obj["version"] = m_currentSession.version;
    obj["start_time"] = m_currentSession.startTime.toString(Qt::ISODate);
    obj["end_time"] = m_currentSession.endTime.toString(Qt::ISODate);
    obj["operation"] = m_currentSession.operation;
    obj["state"] = stateToString(m_currentSession.state);
    
    // Hardware
    QJsonObject hw;
    hw["controller"] = m_currentSession.hardware.controller;
    hw["firmware"] = m_currentSession.hardware.firmware;
    hw["port"] = m_currentSession.hardware.port;
    hw["drive"] = m_currentSession.hardware.drive;
    hw["drive_type"] = m_currentSession.hardware.driveType;
    hw["cylinders"] = m_currentSession.hardware.cylinders;
    hw["heads"] = m_currentSession.hardware.heads;
    hw["rpm"] = m_currentSession.hardware.rpm;
    obj["hardware"] = hw;
    
    // Configuration
    obj["profile"] = m_currentSession.profile;
    obj["format"] = m_currentSession.format;
    obj["parameters"] = m_currentSession.parameters;
    if (!m_currentSession.trackOverrides.isEmpty()) {
        obj["track_overrides"] = m_currentSession.trackOverrides;
    }
    
    // Results
    QJsonArray tracks;
    for (const auto& t : m_currentSession.tracks) {
        QJsonObject track;
        track["cylinder"] = t.cylinder;
        track["head"] = t.head;
        track["status"] = t.status;
        track["good_sectors"] = t.goodSectors;
        track["total_sectors"] = t.totalSectors;
        track["confidence"] = t.confidence;
        track["retries"] = t.retries;
        if (!t.protection.isEmpty()) {
            track["protection"] = t.protection;
        }
        if (!t.errors.isEmpty()) {
            QJsonArray errs;
            for (const auto& e : t.errors) {
                errs.append(e);
            }
            track["errors"] = errs;
        }
        tracks.append(track);
    }
    obj["tracks"] = tracks;
    
    // Statistics
    QJsonObject stats;
    stats["total_tracks"] = m_currentSession.totalTracks;
    stats["good_tracks"] = m_currentSession.goodTracks;
    stats["warning_tracks"] = m_currentSession.warningTracks;
    stats["error_tracks"] = m_currentSession.errorTracks;
    stats["recovered_tracks"] = m_currentSession.recoveredTracks;
    stats["avg_confidence"] = m_currentSession.avgConfidence;
    obj["statistics"] = stats;
    
    // Output
    if (!m_currentSession.outputFile.isEmpty()) {
        QJsonObject output;
        output["file"] = m_currentSession.outputFile;
        output["hash"] = m_currentSession.outputHash;
        output["size"] = m_currentSession.outputSize;
        obj["output"] = output;
    }
    
    // Notes
    if (!m_currentSession.userNotes.isEmpty()) {
        obj["notes"] = m_currentSession.userNotes;
    }
    if (!m_currentSession.warnings.isEmpty()) {
        QJsonArray warns;
        for (const auto& w : m_currentSession.warnings) {
            warns.append(w);
        }
        obj["warnings"] = warns;
    }
    if (!m_currentSession.errors.isEmpty()) {
        QJsonArray errs;
        for (const auto& e : m_currentSession.errors) {
            errs.append(e);
        }
        obj["errors"] = errs;
    }
    
    return obj;
}

bool SessionManager::fromJson(const QJsonObject& json)
{
    m_currentSession = SessionData();
    
    // Metadata
    m_currentSession.id = json["id"].toString();
    m_currentSession.version = json["version"].toString();
    m_currentSession.startTime = QDateTime::fromString(json["start_time"].toString(), Qt::ISODate);
    m_currentSession.endTime = QDateTime::fromString(json["end_time"].toString(), Qt::ISODate);
    m_currentSession.operation = json["operation"].toString();
    m_currentSession.state = stringToState(json["state"].toString());
    
    // Hardware
    if (json.contains("hardware")) {
        QJsonObject hw = json["hardware"].toObject();
        m_currentSession.hardware.controller = hw["controller"].toString();
        m_currentSession.hardware.firmware = hw["firmware"].toString();
        m_currentSession.hardware.port = hw["port"].toString();
        m_currentSession.hardware.drive = hw["drive"].toString();
        m_currentSession.hardware.driveType = hw["drive_type"].toString();
        m_currentSession.hardware.cylinders = hw["cylinders"].toInt();
        m_currentSession.hardware.heads = hw["heads"].toInt();
        m_currentSession.hardware.rpm = hw["rpm"].toDouble();
    }
    
    // Configuration
    m_currentSession.profile = json["profile"].toString();
    m_currentSession.format = json["format"].toString();
    m_currentSession.parameters = json["parameters"].toObject();
    if (json.contains("track_overrides")) {
        m_currentSession.trackOverrides = json["track_overrides"].toObject();
    }
    
    // Results
    if (json.contains("tracks")) {
        QJsonArray tracks = json["tracks"].toArray();
        for (const auto& t : tracks) {
            QJsonObject track = t.toObject();
            TrackResult result;
            result.cylinder = track["cylinder"].toInt();
            result.head = track["head"].toInt();
            result.status = track["status"].toString();
            result.goodSectors = track["good_sectors"].toInt();
            result.totalSectors = track["total_sectors"].toInt();
            result.confidence = track["confidence"].toInt();
            result.retries = track["retries"].toInt();
            result.protection = track["protection"].toString();
            if (track.contains("errors")) {
                QJsonArray errs = track["errors"].toArray();
                for (const auto& e : errs) {
                    result.errors.append(e.toString());
                }
            }
            m_currentSession.tracks.push_back(result);
        }
    }
    
    // Statistics
    if (json.contains("statistics")) {
        QJsonObject stats = json["statistics"].toObject();
        m_currentSession.totalTracks = stats["total_tracks"].toInt();
        m_currentSession.goodTracks = stats["good_tracks"].toInt();
        m_currentSession.warningTracks = stats["warning_tracks"].toInt();
        m_currentSession.errorTracks = stats["error_tracks"].toInt();
        m_currentSession.recoveredTracks = stats["recovered_tracks"].toInt();
        m_currentSession.avgConfidence = stats["avg_confidence"].toDouble();
    }
    
    // Output
    if (json.contains("output")) {
        QJsonObject output = json["output"].toObject();
        m_currentSession.outputFile = output["file"].toString();
        m_currentSession.outputHash = output["hash"].toString();
        m_currentSession.outputSize = output["size"].toVariant().toLongLong();
    }
    
    // Notes
    m_currentSession.userNotes = json["notes"].toString();
    if (json.contains("warnings")) {
        QJsonArray warns = json["warnings"].toArray();
        for (const auto& w : warns) {
            m_currentSession.warnings.append(w.toString());
        }
    }
    if (json.contains("errors")) {
        QJsonArray errs = json["errors"].toArray();
        for (const auto& e : errs) {
            m_currentSession.errors.append(e.toString());
        }
    }
    
    m_active = (m_currentSession.state == SessionState::IDLE ||
                m_currentSession.state == SessionState::READING ||
                m_currentSession.state == SessionState::RECOVERING);
    
    return true;
}

// ============================================================================
// Serialization - YAML
// ============================================================================

QString SessionManager::toYaml() const
{
    QString yaml;
    QTextStream ts(&yaml);
    
    ts << "# UFT Session: " << m_currentSession.id << "\n";
    ts << "# Generated: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n\n";
    
    ts << "session:\n";
    ts << "  id: " << m_currentSession.id << "\n";
    ts << "  version: " << m_currentSession.version << "\n";
    ts << "  operation: " << m_currentSession.operation << "\n";
    ts << "  state: " << stateToString(m_currentSession.state) << "\n";
    ts << "  start_time: " << m_currentSession.startTime.toString(Qt::ISODate) << "\n";
    ts << "  end_time: " << m_currentSession.endTime.toString(Qt::ISODate) << "\n\n";
    
    ts << "hardware:\n";
    ts << "  controller: " << m_currentSession.hardware.controller << "\n";
    ts << "  firmware: " << m_currentSession.hardware.firmware << "\n";
    ts << "  port: " << m_currentSession.hardware.port << "\n";
    ts << "  drive_type: " << m_currentSession.hardware.driveType << "\n";
    ts << "  geometry: " << m_currentSession.hardware.cylinders << "/" 
       << m_currentSession.hardware.heads << "\n";
    ts << "  rpm: " << m_currentSession.hardware.rpm << "\n\n";
    
    ts << "configuration:\n";
    ts << "  profile: " << m_currentSession.profile << "\n";
    ts << "  format: " << m_currentSession.format << "\n\n";
    
    ts << "statistics:\n";
    ts << "  total_tracks: " << m_currentSession.totalTracks << "\n";
    ts << "  good_tracks: " << m_currentSession.goodTracks << "\n";
    ts << "  warning_tracks: " << m_currentSession.warningTracks << "\n";
    ts << "  error_tracks: " << m_currentSession.errorTracks << "\n";
    ts << "  recovered_tracks: " << m_currentSession.recoveredTracks << "\n";
    ts << "  avg_confidence: " << QString::number(m_currentSession.avgConfidence, 'f', 1) << "%\n\n";
    
    if (!m_currentSession.outputFile.isEmpty()) {
        ts << "output:\n";
        ts << "  file: " << m_currentSession.outputFile << "\n";
        ts << "  hash: " << m_currentSession.outputHash << "\n";
        ts << "  size: " << m_currentSession.outputSize << "\n";
    }
    
    return yaml;
}

bool SessionManager::fromYaml(const QString& yaml)
{
    // Simple YAML parser - line by line
    // For full YAML support, would use a proper library
    Q_UNUSED(yaml);
    return false; // Not implemented - use JSON for full fidelity
}

// ============================================================================
// CLI Generation
// ============================================================================

QString SessionManager::toCLI() const
{
    QStringList args;
    args << "uft";
    args << m_currentSession.operation;
    
    // Profile
    if (!m_currentSession.profile.isEmpty()) {
        args << "--profile" << m_currentSession.profile;
    }
    
    // Format
    if (!m_currentSession.format.isEmpty()) {
        args << "--format" << m_currentSession.format;
    }
    
    // Hardware
    if (!m_currentSession.hardware.controller.isEmpty()) {
        args << "--controller" << m_currentSession.hardware.controller;
    }
    if (!m_currentSession.hardware.port.isEmpty()) {
        args << "--port" << m_currentSession.hardware.port;
    }
    
    // Parameters from JSON
    for (auto it = m_currentSession.parameters.begin(); 
         it != m_currentSession.parameters.end(); ++it) {
        QString key = it.key().replace('_', '-');
        QJsonValue val = it.value();
        
        if (val.isBool()) {
            if (val.toBool()) {
                args << QString("--%1").arg(key);
            }
        } else {
            args << QString("--%1").arg(key) << val.toVariant().toString();
        }
    }
    
    // Output
    if (!m_currentSession.outputFile.isEmpty()) {
        args << "--output" << m_currentSession.outputFile;
    }
    
    return args.join(" ");
}

// ============================================================================
// File Operations
// ============================================================================

bool SessionManager::save(const QString& path)
{
    QString filePath = path.isEmpty() ? defaultSessionPath() : path;
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        addError(QString("Failed to save session: %1").arg(file.errorString()));
        return false;
    }
    
    QJsonDocument doc(toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    // Add to history
    if (!m_recentPaths.contains(filePath)) {
        m_recentPaths.prepend(filePath);
        while (m_recentPaths.size() > m_maxHistory) {
            m_recentPaths.removeLast();
        }
        saveHistory();
    }
    
    return true;
}

bool SessionManager::load(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorOccurred(QString("Failed to load session: %1").arg(file.errorString()));
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (doc.isNull()) {
        emit errorOccurred("Invalid session file format");
        return false;
    }
    
    return fromJson(doc.object());
}

bool SessionManager::autoSave()
{
    if (!m_autoSaveEnabled) return false;
    
    QString path = defaultSessionPath();
    bool result = save(path);
    
    if (result) {
        emit autoSaved(path);
    }
    
    return result;
}

QString SessionManager::defaultSessionPath() const
{
    QString filename = QString("session_%1.json")
                       .arg(m_currentSession.startTime.toString("yyyy-MM-dd_HH-mm-ss"));
    return m_sessionDir + "/" + filename;
}

// ============================================================================
// History
// ============================================================================

QStringList SessionManager::recentSessions() const
{
    return m_recentPaths;
}

bool SessionManager::loadRecent(int index)
{
    if (index < 0 || index >= m_recentPaths.size()) {
        return false;
    }
    return load(m_recentPaths[index]);
}

void SessionManager::clearHistory()
{
    m_recentPaths.clear();
    saveHistory();
}

void SessionManager::loadHistory()
{
    QString historyPath = m_sessionDir + "/history.txt";
    QFile file(historyPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (!line.isEmpty() && QFile::exists(line)) {
                m_recentPaths.append(line);
            }
        }
    }
}

void SessionManager::saveHistory()
{
    QString historyPath = m_sessionDir + "/history.txt";
    QFile file(historyPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        for (const auto& path : m_recentPaths) {
            out << path << "\n";
        }
    }
}

// ============================================================================
// Comparison
// ============================================================================

bool SessionManager::compare(const SessionData& a, const SessionData& b, 
                             QStringList& differences)
{
    differences.clear();
    
    if (a.profile != b.profile) {
        differences << QString("Profile: %1 vs %2").arg(a.profile).arg(b.profile);
    }
    if (a.format != b.format) {
        differences << QString("Format: %1 vs %2").arg(a.format).arg(b.format);
    }
    if (a.hardware.controller != b.hardware.controller) {
        differences << QString("Controller: %1 vs %2")
                       .arg(a.hardware.controller).arg(b.hardware.controller);
    }
    if (a.totalTracks != b.totalTracks) {
        differences << QString("Total tracks: %1 vs %2")
                       .arg(a.totalTracks).arg(b.totalTracks);
    }
    if (a.outputHash != b.outputHash) {
        differences << "Output hash differs (data is different)";
    }
    
    return differences.isEmpty();
}

// ============================================================================
// Helper Functions
// ============================================================================

QString SessionManager::generateSessionId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).left(8).toUpper();
}

void SessionManager::updateStatistics()
{
    m_currentSession.totalTracks = m_currentSession.tracks.size();
    m_currentSession.goodTracks = 0;
    m_currentSession.warningTracks = 0;
    m_currentSession.errorTracks = 0;
    
    double totalConfidence = 0;
    
    for (const auto& track : m_currentSession.tracks) {
        if (track.status == "good") {
            m_currentSession.goodTracks++;
        } else if (track.status == "warning") {
            m_currentSession.warningTracks++;
        } else if (track.status == "error") {
            m_currentSession.errorTracks++;
        }
        totalConfidence += track.confidence;
    }
    
    if (m_currentSession.totalTracks > 0) {
        m_currentSession.avgConfidence = totalConfidence / m_currentSession.totalTracks;
    }
}

QString SessionManager::stateToString(SessionState state) const
{
    switch (state) {
        case SessionState::IDLE:      return "idle";
        case SessionState::SCANNING:  return "scanning";
        case SessionState::READING:   return "reading";
        case SessionState::RECOVERING:return "recovering";
        case SessionState::WRITING:   return "writing";
        case SessionState::VERIFYING: return "verifying";
        case SessionState::COMPLETE:  return "complete";
        case SessionState::ERROR:     return "error";
        case SessionState::CANCELLED: return "cancelled";
        default:                      return "unknown";
    }
}

SessionState SessionManager::stringToState(const QString& str) const
{
    if (str == "idle") return SessionState::IDLE;
    if (str == "scanning") return SessionState::SCANNING;
    if (str == "reading") return SessionState::READING;
    if (str == "recovering") return SessionState::RECOVERING;
    if (str == "writing") return SessionState::WRITING;
    if (str == "verifying") return SessionState::VERIFYING;
    if (str == "complete") return SessionState::COMPLETE;
    if (str == "error") return SessionState::ERROR;
    if (str == "cancelled") return SessionState::CANCELLED;
    return SessionState::IDLE;
}

// ============================================================================
// SessionValidator Implementation
// ============================================================================

bool SessionValidator::validate(const SessionData& session, QStringList& errors)
{
    errors.clear();
    
    if (session.id.isEmpty()) {
        errors << "Session ID is empty";
    }
    
    if (!validateHardware(session.hardware, errors)) {
        // Errors already added
    }
    
    if (!validateParameters(session.parameters, errors)) {
        // Errors already added
    }
    
    if (!validateResults(session.tracks, errors)) {
        // Errors already added
    }
    
    return errors.isEmpty();
}

bool SessionValidator::validateHardware(const HardwareSnapshot& hw, QStringList& errors)
{
    if (hw.controller.isEmpty()) {
        errors << "Hardware controller not specified";
        return false;
    }
    if (hw.cylinders <= 0 || hw.cylinders > 255) {
        errors << QString("Invalid cylinder count: %1").arg(hw.cylinders);
        return false;
    }
    if (hw.heads <= 0 || hw.heads > 2) {
        errors << QString("Invalid head count: %1").arg(hw.heads);
        return false;
    }
    return true;
}

bool SessionValidator::validateParameters(const QJsonObject& params, QStringList& errors)
{
    // Check for required parameters
    if (params.contains("retries")) {
        int retries = params["retries"].toInt();
        if (retries < 0 || retries > 100) {
            errors << QString("Retries out of range: %1").arg(retries);
            return false;
        }
    }
    return true;
}

bool SessionValidator::validateResults(const std::vector<TrackResult>& tracks, QStringList& errors)
{
    for (const auto& track : tracks) {
        if (track.cylinder < 0 || track.cylinder > 255) {
            errors << QString("Invalid cylinder: %1").arg(track.cylinder);
            return false;
        }
        if (track.head < 0 || track.head > 1) {
            errors << QString("Invalid head: %1").arg(track.head);
            return false;
        }
    }
    return true;
}

// ============================================================================
// SessionReporter Implementation
// ============================================================================

QString SessionReporter::generateTextReport(const SessionData& session)
{
    QString report;
    QTextStream ts(&report);
    
    ts << "═══════════════════════════════════════════════════════════════\n";
    ts << "                    UFT SESSION REPORT\n";
    ts << "═══════════════════════════════════════════════════════════════\n\n";
    
    ts << "Session ID:    " << session.id << "\n";
    ts << "Operation:     " << session.operation << "\n";
    ts << "Start Time:    " << session.startTime.toString() << "\n";
    ts << "End Time:      " << session.endTime.toString() << "\n";
    ts << "Duration:      " << session.startTime.secsTo(session.endTime) << " seconds\n\n";
    
    ts << "───────────────────────────────────────────────────────────────\n";
    ts << "HARDWARE\n";
    ts << "───────────────────────────────────────────────────────────────\n";
    ts << "Controller:    " << session.hardware.controller << "\n";
    ts << "Firmware:      " << session.hardware.firmware << "\n";
    ts << "Port:          " << session.hardware.port << "\n";
    ts << "Drive Type:    " << session.hardware.driveType << "\n";
    ts << "Geometry:      " << session.hardware.cylinders << " cylinders, "
       << session.hardware.heads << " heads\n";
    ts << "RPM:           " << QString::number(session.hardware.rpm, 'f', 1) << "\n\n";
    
    ts << "───────────────────────────────────────────────────────────────\n";
    ts << "CONFIGURATION\n";
    ts << "───────────────────────────────────────────────────────────────\n";
    ts << "Profile:       " << session.profile << "\n";
    ts << "Format:        " << session.format << "\n\n";
    
    ts << "───────────────────────────────────────────────────────────────\n";
    ts << "RESULTS\n";
    ts << "───────────────────────────────────────────────────────────────\n";
    ts << "Total Tracks:  " << session.totalTracks << "\n";
    ts << "Good:          " << session.goodTracks << " ("
       << QString::number(session.totalTracks > 0 ? 
          session.goodTracks * 100.0 / session.totalTracks : 0, 'f', 1) << "%)\n";
    ts << "Warning:       " << session.warningTracks << "\n";
    ts << "Error:         " << session.errorTracks << "\n";
    ts << "Recovered:     " << session.recoveredTracks << "\n";
    ts << "Avg Confidence:" << QString::number(session.avgConfidence, 'f', 1) << "%\n\n";
    
    if (!session.outputFile.isEmpty()) {
        ts << "───────────────────────────────────────────────────────────────\n";
        ts << "OUTPUT\n";
        ts << "───────────────────────────────────────────────────────────────\n";
        ts << "File:          " << session.outputFile << "\n";
        ts << "Size:          " << session.outputSize << " bytes\n";
        ts << "SHA256:        " << session.outputHash << "\n\n";
    }
    
    ts << "═══════════════════════════════════════════════════════════════\n";
    
    return report;
}

QString SessionReporter::generateHtmlReport(const SessionData& session)
{
    QString html;
    QTextStream ts(&html);
    
    ts << "<!DOCTYPE html>\n<html><head>\n";
    ts << "<title>UFT Session Report - " << session.id << "</title>\n";
    ts << "<style>\n";
    ts << "body { font-family: Arial, sans-serif; margin: 20px; }\n";
    ts << "h1 { color: #333; }\n";
    ts << "h2 { color: #666; border-bottom: 1px solid #ccc; }\n";
    ts << "table { border-collapse: collapse; width: 100%; }\n";
    ts << "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\n";
    ts << "th { background-color: #f2f2f2; }\n";
    ts << ".good { color: green; }\n";
    ts << ".warning { color: orange; }\n";
    ts << ".error { color: red; }\n";
    ts << "</style></head><body>\n";
    
    ts << "<h1>UFT Session Report</h1>\n";
    ts << "<p><strong>Session ID:</strong> " << session.id << "</p>\n";
    ts << "<p><strong>Date:</strong> " << session.startTime.toString() << "</p>\n";
    
    ts << "<h2>Statistics</h2>\n";
    ts << "<table>\n";
    ts << "<tr><td>Total Tracks</td><td>" << session.totalTracks << "</td></tr>\n";
    ts << "<tr><td class='good'>Good</td><td>" << session.goodTracks << "</td></tr>\n";
    ts << "<tr><td class='warning'>Warning</td><td>" << session.warningTracks << "</td></tr>\n";
    ts << "<tr><td class='error'>Error</td><td>" << session.errorTracks << "</td></tr>\n";
    ts << "<tr><td>Average Confidence</td><td>" 
       << QString::number(session.avgConfidence, 'f', 1) << "%</td></tr>\n";
    ts << "</table>\n";
    
    ts << "</body></html>\n";
    
    return html;
}

QString SessionReporter::generateMarkdownReport(const SessionData& session)
{
    QString md;
    QTextStream ts(&md);
    
    ts << "# UFT Session Report\n\n";
    ts << "**Session ID:** " << session.id << "  \n";
    ts << "**Date:** " << session.startTime.toString() << "\n\n";
    
    ts << "## Statistics\n\n";
    ts << "| Metric | Value |\n";
    ts << "|--------|-------|\n";
    ts << "| Total Tracks | " << session.totalTracks << " |\n";
    ts << "| Good | " << session.goodTracks << " |\n";
    ts << "| Warning | " << session.warningTracks << " |\n";
    ts << "| Error | " << session.errorTracks << " |\n";
    ts << "| Avg Confidence | " << QString::number(session.avgConfidence, 'f', 1) << "% |\n\n";
    
    if (!session.outputFile.isEmpty()) {
        ts << "## Output\n\n";
        ts << "- **File:** `" << session.outputFile << "`\n";
        ts << "- **SHA256:** `" << session.outputHash << "`\n";
    }
    
    return md;
}

QJsonObject SessionReporter::generateJsonReport(const SessionData& session)
{
    QJsonObject report;
    
    report["session_id"] = session.id;
    report["timestamp"] = session.startTime.toString(Qt::ISODate);
    report["operation"] = session.operation;
    report["profile"] = session.profile;
    report["format"] = session.format;
    
    QJsonObject stats;
    stats["total_tracks"] = session.totalTracks;
    stats["good_tracks"] = session.goodTracks;
    stats["warning_tracks"] = session.warningTracks;
    stats["error_tracks"] = session.errorTracks;
    stats["avg_confidence"] = session.avgConfidence;
    report["statistics"] = stats;
    
    if (!session.outputFile.isEmpty()) {
        QJsonObject output;
        output["file"] = session.outputFile;
        output["hash"] = session.outputHash;
        output["size"] = session.outputSize;
        report["output"] = output;
    }
    
    return report;
}
