/**
 * @file sessionmanager.cpp
 * @brief Session Management - Full Implementation
 * @version 4.0.0
 */

#include "sessionmanager.h"
#include <QFile>
#include <QDir>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QCryptographicHash>

// ============================================================================
// Constructor / Destructor
// ============================================================================

SessionManager::SessionManager(QObject *parent)
    : QObject(parent)
    , m_autoSaveEnabled(true)
    , m_autoSaveTimer(nullptr)
{
    // Setup paths
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataPath);
    m_historyPath = dataPath + "/session_history.json";
    m_autoSavePath = dataPath + "/autosave.json";
    
    // Load history
    loadHistory();
    
    // Setup auto-save timer
    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setInterval(AUTO_SAVE_INTERVAL_MS);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &SessionManager::onAutoSaveTimer);
}

SessionManager::~SessionManager()
{
    if (m_currentSession) {
        // Auto-save on exit
        if (m_autoSaveEnabled) {
            saveSession(m_autoSavePath);
        }
    }
    saveHistory();
}

// ============================================================================
// Session Lifecycle
// ============================================================================

Session* SessionManager::newSession(OperationType type)
{
    // Close existing session
    closeSession();
    
    // Create new session
    m_currentSession = std::make_unique<Session>();
    m_currentSession->id = QUuid::createUuid();
    m_currentSession->created = QDateTime::currentDateTime();
    m_currentSession->modified = m_currentSession->created;
    m_currentSession->state = SessionState::IDLE;
    m_currentSession->operation = type;
    m_currentSession->uftVersion = "4.0.0";
    
    // Generate default name
    QString typeStr;
    switch (type) {
        case OperationType::READ:    typeStr = "Read"; break;
        case OperationType::WRITE:   typeStr = "Write"; break;
        case OperationType::COPY:    typeStr = "Copy"; break;
        case OperationType::ANALYZE: typeStr = "Analyze"; break;
        case OperationType::VERIFY:  typeStr = "Verify"; break;
        case OperationType::RECOVER: typeStr = "Recover"; break;
    }
    m_currentSession->name = QString("%1_%2")
        .arg(typeStr)
        .arg(m_currentSession->created.toString("yyyyMMdd_HHmmss"));
    
    // Start auto-save timer
    if (m_autoSaveEnabled) {
        m_autoSaveTimer->start();
    }
    
    emit sessionCreated(m_currentSession->id);
    return m_currentSession.get();
}

void SessionManager::closeSession()
{
    if (m_currentSession) {
        m_autoSaveTimer->stop();
        m_currentSession.reset();
        emit sessionClosed();
    }
}

// ============================================================================
// Session State
// ============================================================================

void SessionManager::setState(SessionState state)
{
    if (m_currentSession) {
        m_currentSession->state = state;
        updateModified();
        emit stateChanged(state);
    }
}

SessionState SessionManager::state() const
{
    return m_currentSession ? m_currentSession->state : SessionState::IDLE;
}

// ============================================================================
// Configuration
// ============================================================================

void SessionManager::setHardware(const HardwareInfo& info)
{
    if (m_currentSession) {
        m_currentSession->hardware = info;
        updateModified();
    }
}

void SessionManager::setProfile(const QString& profile)
{
    if (m_currentSession) {
        m_currentSession->profile = profile;
        updateModified();
    }
}

void SessionManager::setFormat(const QString& format)
{
    if (m_currentSession) {
        m_currentSession->format = format;
        updateModified();
    }
}

void SessionManager::setParameters(const QJsonObject& params)
{
    if (m_currentSession) {
        m_currentSession->parameters = params;
        updateModified();
    }
}

void SessionManager::setTrackOverride(int track, int head, const QJsonObject& params)
{
    if (m_currentSession) {
        QString key = QString("%1_%2").arg(track).arg(head);
        m_currentSession->trackOverrides[key] = params;
        updateModified();
    }
}

// ============================================================================
// Results
// ============================================================================

void SessionManager::setResults(const SessionResults& results)
{
    if (m_currentSession) {
        m_currentSession->results = results;
        updateModified();
    }
}

void SessionManager::addTrackResult(const TrackResult& result)
{
    if (m_currentSession) {
        m_currentSession->results.trackResults.push_back(result);
        updateModified();
        
        emit progressUpdated(result.cylinder, result.head,
            (m_currentSession->results.trackResults.size() * 100) /
            std::max(1, m_currentSession->results.tracksTotal));
    }
}

void SessionManager::updateTrackResult(int track, int head, const TrackResult& result)
{
    if (m_currentSession) {
        for (auto& tr : m_currentSession->results.trackResults) {
            if (tr.cylinder == track && tr.head == head) {
                tr = result;
                updateModified();
                return;
            }
        }
        // Not found, add new
        addTrackResult(result);
    }
}

// ============================================================================
// Metadata
// ============================================================================

void SessionManager::setSessionName(const QString& name)
{
    if (m_currentSession) {
        m_currentSession->name = name;
        updateModified();
    }
}

void SessionManager::setNotes(const QString& notes)
{
    if (m_currentSession) {
        m_currentSession->notes = notes;
        updateModified();
    }
}

void SessionManager::addTag(const QString& tag)
{
    if (m_currentSession && !m_currentSession->tags.contains(tag)) {
        m_currentSession->tags.append(tag);
        updateModified();
    }
}

void SessionManager::removeTag(const QString& tag)
{
    if (m_currentSession) {
        m_currentSession->tags.removeAll(tag);
        updateModified();
    }
}

// ============================================================================
// Persistence
// ============================================================================

bool SessionManager::saveSession(const QString& path)
{
    if (!m_currentSession) return false;
    
    QJsonObject json = sessionToJson(*m_currentSession);
    QJsonDocument doc(json);
    
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    addToHistory(path);
    emit sessionSaved(path);
    return true;
}

bool SessionManager::loadSession(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        return false;
    }
    
    closeSession();
    m_currentSession = std::make_unique<Session>(jsonToSession(doc.object()));
    
    addToHistory(path);
    emit sessionLoaded(m_currentSession->id);
    
    if (m_autoSaveEnabled) {
        m_autoSaveTimer->start();
    }
    
    return true;
}

bool SessionManager::exportToJson(const QString& path) const
{
    return const_cast<SessionManager*>(this)->saveSession(path);
}

bool SessionManager::importFromJson(const QString& path)
{
    return loadSession(path);
}

// ============================================================================
// Auto-Save
// ============================================================================

void SessionManager::setAutoSaveEnabled(bool enabled)
{
    m_autoSaveEnabled = enabled;
    if (enabled && m_currentSession) {
        m_autoSaveTimer->start();
    } else {
        m_autoSaveTimer->stop();
    }
}

void SessionManager::setAutoSavePath(const QString& path)
{
    m_autoSavePath = path;
}

void SessionManager::onAutoSaveTimer()
{
    if (m_currentSession && m_autoSaveEnabled) {
        saveSession(m_autoSavePath);
        emit autoSaved(m_autoSavePath);
    }
}

// ============================================================================
// History
// ============================================================================

QStringList SessionManager::recentSessions() const
{
    return m_recentSessions;
}

bool SessionManager::loadRecentSession(int index)
{
    if (index >= 0 && index < m_recentSessions.size()) {
        return loadSession(m_recentSessions[index]);
    }
    return false;
}

void SessionManager::clearHistory()
{
    m_recentSessions.clear();
    saveHistory();
}

void SessionManager::addToHistory(const QString& path)
{
    m_recentSessions.removeAll(path);
    m_recentSessions.prepend(path);
    
    while (m_recentSessions.size() > MAX_RECENT_SESSIONS) {
        m_recentSessions.removeLast();
    }
    
    saveHistory();
}

void SessionManager::loadHistory()
{
    QFile file(m_historyPath);
    if (!file.open(QIODevice::ReadOnly)) return;
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (doc.isArray()) {
        m_recentSessions.clear();
        for (const auto& val : doc.array()) {
            m_recentSessions.append(val.toString());
        }
    }
}

void SessionManager::saveHistory()
{
    QJsonArray arr;
    for (const auto& path : m_recentSessions) {
        arr.append(path);
    }
    
    QFile file(m_historyPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(arr).toJson());
        file.close();
    }
}

// ============================================================================
// CLI Generation
// ============================================================================

QString SessionManager::generateCLI() const
{
    if (!m_currentSession) return QString();
    
    QStringList args;
    args << "uft";
    
    // Operation
    switch (m_currentSession->operation) {
        case OperationType::READ:    args << "read"; break;
        case OperationType::WRITE:   args << "write"; break;
        case OperationType::COPY:    args << "copy"; break;
        case OperationType::ANALYZE: args << "analyze"; break;
        case OperationType::VERIFY:  args << "verify"; break;
        case OperationType::RECOVER: args << "recover"; break;
    }
    
    // Profile
    if (!m_currentSession->profile.isEmpty()) {
        args << "--profile" << m_currentSession->profile;
    }
    
    // Format
    if (!m_currentSession->format.isEmpty()) {
        args << "--format" << m_currentSession->format;
    }
    
    // Hardware
    if (!m_currentSession->hardware.controller.isEmpty()) {
        args << "--controller" << m_currentSession->hardware.controller;
    }
    if (!m_currentSession->hardware.port.isEmpty()) {
        args << "--port" << m_currentSession->hardware.port;
    }
    if (!m_currentSession->hardware.drive.isEmpty()) {
        args << "--drive" << m_currentSession->hardware.drive;
    }
    
    // Parameters
    for (auto it = m_currentSession->parameters.begin();
         it != m_currentSession->parameters.end(); ++it) {
        QString key = it.key();
        QJsonValue val = it.value();
        
        if (val.isBool()) {
            if (val.toBool()) {
                args << QString("--%1").arg(key);
            }
        } else if (val.isDouble()) {
            args << QString("--%1").arg(key) << QString::number(val.toDouble());
        } else if (val.isString()) {
            args << QString("--%1").arg(key) << val.toString();
        }
    }
    
    // Output
    if (!m_currentSession->results.outputFile.isEmpty()) {
        args << "--output" << m_currentSession->results.outputFile;
    }
    
    return args.join(" ");
}

// ============================================================================
// Session Comparison
// ============================================================================

QJsonObject SessionManager::compareSessions(const Session& a, const Session& b)
{
    QJsonObject diff;
    
    // Compare profiles
    if (a.profile != b.profile) {
        QJsonObject profileDiff;
        profileDiff["a"] = a.profile;
        profileDiff["b"] = b.profile;
        diff["profile"] = profileDiff;
    }
    
    // Compare formats
    if (a.format != b.format) {
        QJsonObject formatDiff;
        formatDiff["a"] = a.format;
        formatDiff["b"] = b.format;
        diff["format"] = formatDiff;
    }
    
    // Compare parameters
    QJsonObject paramDiff;
    QSet<QString> allKeys;
    for (auto it = a.parameters.begin(); it != a.parameters.end(); ++it) {
        allKeys.insert(it.key());
    }
    for (auto it = b.parameters.begin(); it != b.parameters.end(); ++it) {
        allKeys.insert(it.key());
    }
    
    for (const QString& key : allKeys) {
        QJsonValue valA = a.parameters.value(key);
        QJsonValue valB = b.parameters.value(key);
        
        if (valA != valB) {
            QJsonObject keyDiff;
            keyDiff["a"] = valA;
            keyDiff["b"] = valB;
            paramDiff[key] = keyDiff;
        }
    }
    
    if (!paramDiff.isEmpty()) {
        diff["parameters"] = paramDiff;
    }
    
    // Compare results
    QJsonObject resultDiff;
    if (a.results.tracksGood != b.results.tracksGood) {
        QJsonObject gdiff;
        gdiff["a"] = a.results.tracksGood;
        gdiff["b"] = b.results.tracksGood;
        resultDiff["tracksGood"] = gdiff;
    }
    if (a.results.averageConfidence != b.results.averageConfidence) {
        QJsonObject cdiff;
        cdiff["a"] = a.results.averageConfidence;
        cdiff["b"] = b.results.averageConfidence;
        resultDiff["averageConfidence"] = cdiff;
    }
    
    if (!resultDiff.isEmpty()) {
        diff["results"] = resultDiff;
    }
    
    return diff;
}

// ============================================================================
// Private Helpers
// ============================================================================

void SessionManager::updateModified()
{
    if (m_currentSession) {
        m_currentSession->modified = QDateTime::currentDateTime();
    }
}

QJsonObject SessionManager::sessionToJson(const Session& session) const
{
    QJsonObject json;
    
    // Identity
    json["id"] = session.id.toString();
    json["name"] = session.name;
    json["created"] = session.created.toString(Qt::ISODate);
    json["modified"] = session.modified.toString(Qt::ISODate);
    json["uftVersion"] = session.uftVersion;
    
    // State
    json["state"] = static_cast<int>(session.state);
    json["operation"] = static_cast<int>(session.operation);
    
    // Hardware
    QJsonObject hw;
    hw["controller"] = session.hardware.controller;
    hw["firmware"] = session.hardware.firmware;
    hw["port"] = session.hardware.port;
    hw["drive"] = session.hardware.drive;
    hw["driveType"] = session.hardware.driveType;
    hw["heads"] = session.hardware.heads;
    hw["cylinders"] = session.hardware.cylinders;
    hw["rpm"] = session.hardware.rpm;
    json["hardware"] = hw;
    
    // Configuration
    json["profile"] = session.profile;
    json["format"] = session.format;
    json["parameters"] = session.parameters;
    json["trackOverrides"] = session.trackOverrides;
    
    // Results
    QJsonObject res;
    res["tracksTotal"] = session.results.tracksTotal;
    res["tracksGood"] = session.results.tracksGood;
    res["tracksWarning"] = session.results.tracksWarning;
    res["tracksError"] = session.results.tracksError;
    res["tracksProtected"] = session.results.tracksProtected;
    res["averageConfidence"] = session.results.averageConfidence;
    res["outputFile"] = session.results.outputFile;
    res["outputHash"] = session.results.outputHash;
    res["outputSize"] = session.results.outputSize;
    res["startTime"] = session.results.startTime.toString(Qt::ISODate);
    res["endTime"] = session.results.endTime.toString(Qt::ISODate);
    res["durationMs"] = session.results.durationMs;
    
    QJsonArray trackArr;
    for (const auto& tr : session.results.trackResults) {
        QJsonObject tro;
        tro["cylinder"] = tr.cylinder;
        tro["head"] = tr.head;
        tro["status"] = tr.status;
        tro["goodSectors"] = tr.goodSectors;
        tro["totalSectors"] = tr.totalSectors;
        tro["confidence"] = tr.confidence;
        tro["retries"] = tr.retries;
        tro["protection"] = tr.protection;
        QJsonArray errArr;
        for (const auto& err : tr.errors) errArr.append(err);
        tro["errors"] = errArr;
        trackArr.append(tro);
    }
    res["trackResults"] = trackArr;
    json["results"] = res;
    
    // Metadata
    json["notes"] = session.notes;
    QJsonArray tagArr;
    for (const auto& tag : session.tags) tagArr.append(tag);
    json["tags"] = tagArr;
    
    return json;
}

Session SessionManager::jsonToSession(const QJsonObject& json) const
{
    Session session;
    
    // Identity
    session.id = QUuid::fromString(json["id"].toString());
    session.name = json["name"].toString();
    session.created = QDateTime::fromString(json["created"].toString(), Qt::ISODate);
    session.modified = QDateTime::fromString(json["modified"].toString(), Qt::ISODate);
    session.uftVersion = json["uftVersion"].toString();
    
    // State
    session.state = static_cast<SessionState>(json["state"].toInt());
    session.operation = static_cast<OperationType>(json["operation"].toInt());
    
    // Hardware
    QJsonObject hw = json["hardware"].toObject();
    session.hardware.controller = hw["controller"].toString();
    session.hardware.firmware = hw["firmware"].toString();
    session.hardware.port = hw["port"].toString();
    session.hardware.drive = hw["drive"].toString();
    session.hardware.driveType = hw["driveType"].toString();
    session.hardware.heads = hw["heads"].toInt();
    session.hardware.cylinders = hw["cylinders"].toInt();
    session.hardware.rpm = hw["rpm"].toDouble();
    
    // Configuration
    session.profile = json["profile"].toString();
    session.format = json["format"].toString();
    session.parameters = json["parameters"].toObject();
    session.trackOverrides = json["trackOverrides"].toObject();
    
    // Results
    QJsonObject res = json["results"].toObject();
    session.results.tracksTotal = res["tracksTotal"].toInt();
    session.results.tracksGood = res["tracksGood"].toInt();
    session.results.tracksWarning = res["tracksWarning"].toInt();
    session.results.tracksError = res["tracksError"].toInt();
    session.results.tracksProtected = res["tracksProtected"].toInt();
    session.results.averageConfidence = res["averageConfidence"].toDouble();
    session.results.outputFile = res["outputFile"].toString();
    session.results.outputHash = res["outputHash"].toString();
    session.results.outputSize = res["outputSize"].toVariant().toLongLong();
    session.results.startTime = QDateTime::fromString(res["startTime"].toString(), Qt::ISODate);
    session.results.endTime = QDateTime::fromString(res["endTime"].toString(), Qt::ISODate);
    session.results.durationMs = res["durationMs"].toVariant().toLongLong();
    
    for (const auto& val : res["trackResults"].toArray()) {
        QJsonObject tro = val.toObject();
        TrackResult tr;
        tr.cylinder = tro["cylinder"].toInt();
        tr.head = tro["head"].toInt();
        tr.status = tro["status"].toString();
        tr.goodSectors = tro["goodSectors"].toInt();
        tr.totalSectors = tro["totalSectors"].toInt();
        tr.confidence = tro["confidence"].toInt();
        tr.retries = tro["retries"].toInt();
        tr.protection = tro["protection"].toString();
        for (const auto& err : tro["errors"].toArray()) {
            tr.errors.append(err.toString());
        }
        session.results.trackResults.push_back(tr);
    }
    
    // Metadata
    session.notes = json["notes"].toString();
    for (const auto& val : json["tags"].toArray()) {
        session.tags.append(val.toString());
    }
    
    return session;
}
