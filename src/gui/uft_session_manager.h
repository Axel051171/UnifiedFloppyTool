/**
 * @file uft_session_manager.h
 * @brief Session Manager (P2-GUI-010)
 * 
 * Manages work sessions, recent files, and workspace state.
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#ifndef UFT_SESSION_MANAGER_H
#define UFT_SESSION_MANAGER_H

#include <QObject>
#include <QWidget>
#include <QDialog>
#include <QListWidget>
#include <QTreeWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QGroupBox>
#include <QSettings>
#include <QDateTime>
#include <QJsonObject>
#include <QStringList>

/*===========================================================================
 * Session Data Structures
 *===========================================================================*/

/**
 * @brief Recent file entry
 */
struct UftRecentFile {
    QString path;
    QString format;
    QDateTime lastOpened;
    qint64 size;
    bool pinned;
    
    QJsonObject toJson() const;
    static UftRecentFile fromJson(const QJsonObject &obj);
};

/**
 * @brief Session state
 */
struct UftSessionState {
    /* Window state */
    QByteArray windowGeometry;
    QByteArray windowState;
    QByteArray splitterState;
    
    /* Current work */
    QString currentFile;
    QString currentFormat;
    int currentTrack;
    int currentSector;
    
    /* View state */
    int activeTab;
    double zoomLevel;
    bool showHexView;
    bool showFluxView;
    
    /* Options */
    QJsonObject formatOptions;
    QJsonObject hardwareOptions;
    
    QJsonObject toJson() const;
    static UftSessionState fromJson(const QJsonObject &obj);
};

/**
 * @brief Project file
 */
struct UftProject {
    QString name;
    QString path;
    QString description;
    QDateTime created;
    QDateTime modified;
    
    /* Source files */
    QStringList sourceFiles;
    QStringList outputFiles;
    
    /* State */
    UftSessionState lastState;
    QJsonObject metadata;
    
    QJsonObject toJson() const;
    static UftProject fromJson(const QJsonObject &obj);
};

/*===========================================================================
 * Session Manager Core
 *===========================================================================*/

/**
 * @brief Session Manager
 */
class UftSessionManager : public QObject
{
    Q_OBJECT
    
public:
    static UftSessionManager* instance();
    
    /* Recent files */
    void addRecentFile(const QString &path, const QString &format = QString());
    void removeRecentFile(const QString &path);
    void clearRecentFiles();
    void pinRecentFile(const QString &path, bool pin);
    QList<UftRecentFile> recentFiles() const;
    int maxRecentFiles() const;
    void setMaxRecentFiles(int max);
    
    /* Session state */
    void saveSessionState(const UftSessionState &state);
    UftSessionState loadSessionState() const;
    void clearSessionState();
    
    /* Projects */
    bool createProject(const QString &path, const QString &name);
    bool openProject(const QString &path);
    bool saveProject();
    bool saveProjectAs(const QString &path);
    bool closeProject();
    bool hasProject() const;
    UftProject currentProject() const;
    
    /* Auto-save */
    void enableAutoSave(bool enable, int intervalMs = 60000);
    bool isAutoSaveEnabled() const;
    
    /* File history */
    QStringList recentProjects() const;
    void addRecentProject(const QString &path);

signals:
    void recentFilesChanged();
    void sessionStateChanged();
    void projectOpened(const QString &path);
    void projectClosed();
    void projectSaved();

private:
    explicit UftSessionManager(QObject *parent = nullptr);
    ~UftSessionManager();
    
    void loadSettings();
    void saveSettings();
    
    static UftSessionManager *s_instance;
    
    QList<UftRecentFile> m_recentFiles;
    QStringList m_recentProjects;
    int m_maxRecentFiles;
    
    UftSessionState m_currentState;
    UftProject m_currentProject;
    bool m_hasProject;
    
    bool m_autoSaveEnabled;
    int m_autoSaveInterval;
    QTimer *m_autoSaveTimer;
};

/*===========================================================================
 * Session Dialog
 *===========================================================================*/

/**
 * @brief Session dialog (start screen / file browser)
 */
class UftSessionDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit UftSessionDialog(QWidget *parent = nullptr);
    
    enum Action {
        ActionNone,
        ActionNewProject,
        ActionOpenProject,
        ActionOpenFile,
        ActionOpenRecent
    };
    
    Action selectedAction() const { return m_action; }
    QString selectedPath() const { return m_selectedPath; }

public slots:
    void refresh();

private slots:
    void onNewProject();
    void onOpenProject();
    void onOpenFile();
    void onRecentSelected(QListWidgetItem *item);
    void onRecentDoubleClicked(QListWidgetItem *item);
    void onProjectSelected(QListWidgetItem *item);
    void onRemoveRecent();
    void onClearRecent();
    void onPinRecent();

private:
    void setupUi();
    void populateRecent();
    void populateProjects();
    void updateButtons();
    
    /* Left panel */
    QListWidget *m_actionList;
    
    /* Recent files */
    QGroupBox *m_recentGroup;
    QListWidget *m_recentList;
    QPushButton *m_removeRecentBtn;
    QPushButton *m_clearRecentBtn;
    QPushButton *m_pinRecentBtn;
    
    /* Recent projects */
    QGroupBox *m_projectsGroup;
    QListWidget *m_projectsList;
    
    /* Info panel */
    QGroupBox *m_infoGroup;
    QLabel *m_infoLabel;
    
    /* Buttons */
    QPushButton *m_openButton;
    QPushButton *m_cancelButton;
    
    Action m_action;
    QString m_selectedPath;
};

/*===========================================================================
 * Project Dialog
 *===========================================================================*/

/**
 * @brief New project dialog
 */
class UftNewProjectDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit UftNewProjectDialog(QWidget *parent = nullptr);
    
    QString projectName() const;
    QString projectPath() const;
    QString projectDescription() const;

private slots:
    void browseLocation();
    void updatePath();
    void validate();

private:
    void setupUi();
    
    QLineEdit *m_nameEdit;
    QLineEdit *m_locationEdit;
    QLineEdit *m_pathPreview;
    QTextEdit *m_descEdit;
    QPushButton *m_browseBtn;
    QPushButton *m_createBtn;
    QPushButton *m_cancelBtn;
};

/*===========================================================================
 * Workspace State Widget
 *===========================================================================*/

/**
 * @brief Workspace state panel (shows current session info)
 */
class UftWorkspacePanel : public QWidget
{
    Q_OBJECT
    
public:
    explicit UftWorkspacePanel(QWidget *parent = nullptr);
    
    void updateFromSession();

signals:
    void fileRequested(const QString &path);
    void projectRequested(const QString &path);

private slots:
    void onRecentFilesChanged();

private:
    void setupUi();
    
    /* Current file */
    QGroupBox *m_currentGroup;
    QLabel *m_currentFile;
    QLabel *m_currentFormat;
    QLabel *m_currentSize;
    
    /* Project */
    QGroupBox *m_projectGroup;
    QLabel *m_projectName;
    QLabel *m_projectPath;
    QPushButton *m_saveProjectBtn;
    
    /* Quick access */
    QGroupBox *m_quickGroup;
    QListWidget *m_recentList;
};

#endif /* UFT_SESSION_MANAGER_H */
