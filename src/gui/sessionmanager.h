/**
 * @file sessionmanager.h
 * @brief Session Management for Reproducible Operations
 * @version 4.1.0
 * 
 * FEATURES:
 * - Complete session snapshots (hardware + params + results)
 * - JSON export/import for CLI equivalence
 * - Session history tracking
 * - Auto-save and recovery
 * - Session comparison
 */

#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <vector>
#include <memory>

/**
 * @enum SessionState
 * @brief Current state of a session
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
 * @enum OperationType
 * @brief Type of operation performed
 */
enum class OperationType
{
    READ,
    WRITE,
    COPY,
    ANALYZE,
    VERIFY,
    RECOVER
};

/**
 * @struct HardwareInfo
 * @brief Hardware configuration snapshot
 */
struct HardwareInfo
{
    QString controller;     // "greaseweazle", "kryoflux", etc.
    QString firmware;       // Firmware version
    QString port;           // "/dev/ttyACM0", "COM3", etc.
    QString drive;          // "A", "B", "0", "1"
    QString driveType;      // "DD", "HD", "ED"
    int heads;
    int cylinders;
    double rpm;
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
    QString protection;
    QStringList errors;
};

/**
 * @struct SessionResults
 * @brief Complete results of an operation
 */
struct SessionResults
{
    int tracksTotal;
    int tracksGood;
    int tracksWarning;
    int tracksError;
    int tracksProtected;
    double averageConfidence;
    QString outputFile;
    QString outputHash;     // SHA-256
    qint64 outputSize;
    QDateTime startTime;
    QDateTime endTime;
    qint64 durationMs;
    std::vector<TrackResult> trackResults;
};

/**
 * @struct Session
 * @brief Complete session data
 */
struct Session
{
    // Identity
    QUuid id;
    QString name;
    QDateTime created;
    QDateTime modified;
    
    // State
    SessionState state;
    OperationType operation;
    
    // Configuration
    HardwareInfo hardware;
    QString profile;
    QString format;
    QJsonObject parameters;
    QJsonObject trackOverrides;
    
    // Results
    SessionResults results;
    
    // Metadata
    QString notes;
    QStringList tags;
    QString uftVersion;
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
    Session* newSession(OperationType type = OperationType::READ);
    Session* currentSession() const { return m_currentSession.get(); }
    void closeSession();
    
    // Session state
    void setState(SessionState state);
    SessionState state() const;
    
    // Configuration
    void setHardware(const HardwareInfo& info);
    void setProfile(const QString& profile);
    void setFormat(const QString& format);
    void setParameters(const QJsonObject& params);
    void setTrackOverride(int track, int head, const QJsonObject& params);
    
    // Results
    void setResults(const SessionResults& results);
    void addTrackResult(const TrackResult& result);
    void updateTrackResult(int track, int head, const TrackResult& result);
    
    // Metadata
    void setSessionName(const QString& name);
    void setNotes(const QString& notes);
    void addTag(const QString& tag);
    void removeTag(const QString& tag);
    
    // Persistence
    bool saveSession(const QString& path);
    bool loadSession(const QString& path);
    bool exportToJson(const QString& path) const;
    bool importFromJson(const QString& path);
    
    // Auto-save
    void setAutoSaveEnabled(bool enabled);
    bool autoSaveEnabled() const { return m_autoSaveEnabled; }
    void setAutoSavePath(const QString& path);
    QString autoSavePath() const { return m_autoSavePath; }
    
    // History
    QStringList recentSessions() const;
    bool loadRecentSession(int index);
    void clearHistory();
    
    // CLI generation
    QString generateCLI() const;
    
    // Comparison
    static QJsonObject compareSessions(const Session& a, const Session& b);

signals:
    void sessionCreated(const QUuid& id);
    void sessionLoaded(const QUuid& id);
    void sessionSaved(const QString& path);
    void sessionClosed();
    void stateChanged(SessionState state);
    void progressUpdated(int track, int head, int percent);
    void autoSaved(const QString& path);

private slots:
    void onAutoSaveTimer();

private:
    void updateModified();
    QJsonObject sessionToJson(const Session& session) const;
    Session jsonToSession(const QJsonObject& json) const;
    void addToHistory(const QString& path);
    void loadHistory();
    void saveHistory();
    
    std::unique_ptr<Session> m_currentSession;
    bool m_autoSaveEnabled;
    QString m_autoSavePath;
    QStringList m_recentSessions;
    QString m_historyPath;
    
    class QTimer* m_autoSaveTimer;
    static const int AUTO_SAVE_INTERVAL_MS = 30000;  // 30 seconds
    static const int MAX_RECENT_SESSIONS = 20;
};
