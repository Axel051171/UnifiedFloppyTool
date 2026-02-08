/**
 * @file sessionmanager.h
 * @brief Session Management for UFT GUI
 * @version 4.0.0
 * 
 * FEATURES:
 * - Complete session state capture
 * - JSON/YAML serialization
 * - Session history and recovery
 * - Hardware state snapshot
 * - Parameter + disk state
 * - Reproducibility guarantee
 */

#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDateTime>
#include <QDir>
#include <QString>
#include <QMap>
#include <vector>

/**
 * @enum SessionState
 * @brief Current session state
 */
enum class SessionState
{
    IDLE,
    SCANNING,
    READING,
    RECOVERING,
    WRITING,
    VERIFYING,
    COMPLETE,
    ERROR,
    CANCELLED
};

/**
 * @struct HardwareSnapshot
 * @brief Hardware configuration at session time
 */
struct HardwareSnapshot
{
    QString controller;     // "greaseweazle", "kryoflux", etc.
    QString firmware;       // Firmware version
    QString port;           // USB/Serial port
    QString drive;          // Drive letter/name
    QString driveType;      // "DD", "HD", "ED"
    int cylinders;
    int heads;
    double rpm;
    QDateTime timestamp;
};

/**
 * @struct TrackResult
 * @brief Result for a single track
 */
struct TrackResult
{
    int cylinder;
    int head;
    QString status;         // "good", "warning", "error", "protected"
    int goodSectors;
    int totalSectors;
    int confidence;
    int retries;
    QString protection;     // Protection type if detected
    QStringList errors;
};

/**
 * @struct SessionData
 * @brief Complete session data
 */
struct SessionData
{
    // Metadata
    QString id;             // Unique session ID
    QString version;        // UFT version
    QDateTime startTime;
    QDateTime endTime;
    QString operation;      // "read", "write", "copy"
    SessionState state;
    
    // Hardware
    HardwareSnapshot hardware;
    
    // Configuration
    QString profile;
    QString format;
    QJsonObject parameters;
    QJsonObject trackOverrides;
    
    // Results
    std::vector<TrackResult> tracks;
    int totalTracks;
    int goodTracks;
    int warningTracks;
    int errorTracks;
    int recoveredTracks;
    double avgConfidence;
    
    // Output
    QString outputFile;
    QString outputHash;     // SHA256
    qint64 outputSize;
    
    // Notes
    QString userNotes;
    QStringList warnings;
    QStringList errors;
};

/**
 * @class SessionManager
 * @brief Manages session lifecycle and persistence
 */
class SessionManager : public QObject
{
    Q_OBJECT

public:
    explicit SessionManager(QObject *parent = nullptr);
    virtual ~SessionManager();
    
    // Session lifecycle
    QString newSession(const QString& operation = "read");
    void endSession(SessionState finalState = SessionState::COMPLETE);
    void cancelSession();
    bool isActive() const { return m_active; }
    QString currentSessionId() const { return m_currentSession.id; }
    
    // State management
    void setState(SessionState state);
    SessionState state() const { return m_currentSession.state; }
    
    // Hardware
    void setHardware(const HardwareSnapshot& hw);
    HardwareSnapshot hardware() const { return m_currentSession.hardware; }
    
    // Configuration
    void setProfile(const QString& profile);
    void setFormat(const QString& format);
    void setParameters(const QJsonObject& params);
    void setTrackOverrides(const QJsonObject& overrides);
    
    // Results
    void addTrackResult(const TrackResult& result);
    void updateTrackResult(int cylinder, int head, const TrackResult& result);
    void setOutput(const QString& file, const QString& hash, qint64 size);
    
    // Notes
    void addNote(const QString& note);
    void addWarning(const QString& warning);
    void addError(const QString& error);
    
    // Serialization
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);
    QString toYaml() const;
    bool fromYaml(const QString& yaml);
    QString toCLI() const;
    
    // File operations
    bool save(const QString& path = QString());
    bool load(const QString& path);
    bool autoSave();
    QString defaultSessionPath() const;
    
    // History
    QStringList recentSessions() const;
    bool loadRecent(int index);
    void clearHistory();
    
    // Comparison
    static bool compare(const SessionData& a, const SessionData& b, QStringList& differences);
    
    // Current session data (read-only)
    const SessionData& data() const { return m_currentSession; }
    
signals:
    void sessionStarted(const QString& id);
    void sessionEnded(const QString& id, SessionState state);
    void stateChanged(SessionState state);
    void trackCompleted(int cylinder, int head, const TrackResult& result);
    void progressChanged(int percent);
    void autoSaved(const QString& path);
    void errorOccurred(const QString& error);

private:
    QString generateSessionId() const;
    void updateStatistics();
    QString stateToString(SessionState state) const;
    SessionState stringToState(const QString& str) const;
    void loadHistory();
    void saveHistory();
    
    SessionData m_currentSession;
    bool m_active;
    QString m_sessionDir;
    QStringList m_recentPaths;
    int m_maxHistory;
    bool m_autoSaveEnabled;
    int m_autoSaveInterval;  // seconds
};

/**
 * @class SessionValidator
 * @brief Validates session data for consistency
 */
class SessionValidator
{
public:
    static bool validate(const SessionData& session, QStringList& errors);
    static bool validateHardware(const HardwareSnapshot& hw, QStringList& errors);
    static bool validateParameters(const QJsonObject& params, QStringList& errors);
    static bool validateResults(const std::vector<TrackResult>& tracks, QStringList& errors);
};

/**
 * @class SessionReporter
 * @brief Generates reports from session data
 */
class SessionReporter
{
public:
    static QString generateTextReport(const SessionData& session);
    static QString generateHtmlReport(const SessionData& session);
    static QString generateMarkdownReport(const SessionData& session);
    static QJsonObject generateJsonReport(const SessionData& session);
};
