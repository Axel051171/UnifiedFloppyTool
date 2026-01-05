/**
 * @file uft_session_manager.cpp
 * @brief Session Manager Implementation (P2-GUI-010)
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#include "uft_session_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMessageBox>
#include <QTimer>
#include <QStandardPaths>
#include <QApplication>

/*===========================================================================
 * JSON Serialization
 *===========================================================================*/

QJsonObject UftRecentFile::toJson() const
{
    QJsonObject obj;
    obj["path"] = path;
    obj["format"] = format;
    obj["lastOpened"] = lastOpened.toString(Qt::ISODate);
    obj["size"] = size;
    obj["pinned"] = pinned;
    return obj;
}

UftRecentFile UftRecentFile::fromJson(const QJsonObject &obj)
{
    UftRecentFile rf;
    rf.path = obj["path"].toString();
    rf.format = obj["format"].toString();
    rf.lastOpened = QDateTime::fromString(obj["lastOpened"].toString(), Qt::ISODate);
    rf.size = obj["size"].toVariant().toLongLong();
    rf.pinned = obj["pinned"].toBool();
    return rf;
}

QJsonObject UftSessionState::toJson() const
{
    QJsonObject obj;
    obj["windowGeometry"] = QString::fromLatin1(windowGeometry.toBase64());
    obj["windowState"] = QString::fromLatin1(windowState.toBase64());
    obj["splitterState"] = QString::fromLatin1(splitterState.toBase64());
    obj["currentFile"] = currentFile;
    obj["currentFormat"] = currentFormat;
    obj["currentTrack"] = currentTrack;
    obj["currentSector"] = currentSector;
    obj["activeTab"] = activeTab;
    obj["zoomLevel"] = zoomLevel;
    obj["showHexView"] = showHexView;
    obj["showFluxView"] = showFluxView;
    obj["formatOptions"] = formatOptions;
    obj["hardwareOptions"] = hardwareOptions;
    return obj;
}

UftSessionState UftSessionState::fromJson(const QJsonObject &obj)
{
    UftSessionState s;
    s.windowGeometry = QByteArray::fromBase64(obj["windowGeometry"].toString().toLatin1());
    s.windowState = QByteArray::fromBase64(obj["windowState"].toString().toLatin1());
    s.splitterState = QByteArray::fromBase64(obj["splitterState"].toString().toLatin1());
    s.currentFile = obj["currentFile"].toString();
    s.currentFormat = obj["currentFormat"].toString();
    s.currentTrack = obj["currentTrack"].toInt();
    s.currentSector = obj["currentSector"].toInt();
    s.activeTab = obj["activeTab"].toInt();
    s.zoomLevel = obj["zoomLevel"].toDouble(1.0);
    s.showHexView = obj["showHexView"].toBool();
    s.showFluxView = obj["showFluxView"].toBool();
    s.formatOptions = obj["formatOptions"].toObject();
    s.hardwareOptions = obj["hardwareOptions"].toObject();
    return s;
}

QJsonObject UftProject::toJson() const
{
    QJsonObject obj;
    obj["name"] = name;
    obj["path"] = path;
    obj["description"] = description;
    obj["created"] = created.toString(Qt::ISODate);
    obj["modified"] = modified.toString(Qt::ISODate);
    
    QJsonArray srcArr;
    for (const auto &f : sourceFiles) srcArr.append(f);
    obj["sourceFiles"] = srcArr;
    
    QJsonArray outArr;
    for (const auto &f : outputFiles) outArr.append(f);
    obj["outputFiles"] = outArr;
    
    obj["lastState"] = lastState.toJson();
    obj["metadata"] = metadata;
    return obj;
}

UftProject UftProject::fromJson(const QJsonObject &obj)
{
    UftProject p;
    p.name = obj["name"].toString();
    p.path = obj["path"].toString();
    p.description = obj["description"].toString();
    p.created = QDateTime::fromString(obj["created"].toString(), Qt::ISODate);
    p.modified = QDateTime::fromString(obj["modified"].toString(), Qt::ISODate);
    
    for (const auto &v : obj["sourceFiles"].toArray()) {
        p.sourceFiles.append(v.toString());
    }
    for (const auto &v : obj["outputFiles"].toArray()) {
        p.outputFiles.append(v.toString());
    }
    
    p.lastState = UftSessionState::fromJson(obj["lastState"].toObject());
    p.metadata = obj["metadata"].toObject();
    return p;
}

/*===========================================================================
 * UftSessionManager
 *===========================================================================*/

UftSessionManager* UftSessionManager::s_instance = nullptr;

UftSessionManager* UftSessionManager::instance()
{
    if (!s_instance) {
        s_instance = new UftSessionManager(qApp);
    }
    return s_instance;
}

UftSessionManager::UftSessionManager(QObject *parent)
    : QObject(parent)
    , m_maxRecentFiles(20)
    , m_hasProject(false)
    , m_autoSaveEnabled(false)
    , m_autoSaveInterval(60000)
    , m_autoSaveTimer(nullptr)
{
    loadSettings();
    
    m_autoSaveTimer = new QTimer(this);
    connect(m_autoSaveTimer, &QTimer::timeout, [this]() {
        if (m_hasProject) {
            saveProject();
        }
    });
}

UftSessionManager::~UftSessionManager()
{
    saveSettings();
}

void UftSessionManager::loadSettings()
{
    QSettings settings;
    
    settings.beginGroup("Session");
    m_maxRecentFiles = settings.value("maxRecentFiles", 20).toInt();
    
    /* Load recent files */
    int count = settings.beginReadArray("recentFiles");
    for (int i = 0; i < count; i++) {
        settings.setArrayIndex(i);
        UftRecentFile rf;
        rf.path = settings.value("path").toString();
        rf.format = settings.value("format").toString();
        rf.lastOpened = settings.value("lastOpened").toDateTime();
        rf.size = settings.value("size").toLongLong();
        rf.pinned = settings.value("pinned").toBool();
        
        if (QFileInfo::exists(rf.path)) {
            m_recentFiles.append(rf);
        }
    }
    settings.endArray();
    
    /* Load recent projects */
    count = settings.beginReadArray("recentProjects");
    for (int i = 0; i < count; i++) {
        settings.setArrayIndex(i);
        QString path = settings.value("path").toString();
        if (QFileInfo::exists(path)) {
            m_recentProjects.append(path);
        }
    }
    settings.endArray();
    
    settings.endGroup();
}

void UftSessionManager::saveSettings()
{
    QSettings settings;
    
    settings.beginGroup("Session");
    settings.setValue("maxRecentFiles", m_maxRecentFiles);
    
    /* Save recent files */
    settings.beginWriteArray("recentFiles");
    for (int i = 0; i < m_recentFiles.size(); i++) {
        settings.setArrayIndex(i);
        const auto &rf = m_recentFiles[i];
        settings.setValue("path", rf.path);
        settings.setValue("format", rf.format);
        settings.setValue("lastOpened", rf.lastOpened);
        settings.setValue("size", rf.size);
        settings.setValue("pinned", rf.pinned);
    }
    settings.endArray();
    
    /* Save recent projects */
    settings.beginWriteArray("recentProjects");
    for (int i = 0; i < m_recentProjects.size(); i++) {
        settings.setArrayIndex(i);
        settings.setValue("path", m_recentProjects[i]);
    }
    settings.endArray();
    
    settings.endGroup();
}

void UftSessionManager::addRecentFile(const QString &path, const QString &format)
{
    /* Remove if already exists */
    removeRecentFile(path);
    
    /* Add to front */
    UftRecentFile rf;
    rf.path = path;
    rf.format = format.isEmpty() ? QFileInfo(path).suffix().toUpper() : format;
    rf.lastOpened = QDateTime::currentDateTime();
    rf.size = QFileInfo(path).size();
    rf.pinned = false;
    
    m_recentFiles.prepend(rf);
    
    /* Trim to max (keep pinned) */
    while (m_recentFiles.size() > m_maxRecentFiles) {
        bool removed = false;
        for (int i = m_recentFiles.size() - 1; i >= 0; i--) {
            if (!m_recentFiles[i].pinned) {
                m_recentFiles.removeAt(i);
                removed = true;
                break;
            }
        }
        if (!removed) break;
    }
    
    saveSettings();
    emit recentFilesChanged();
}

void UftSessionManager::removeRecentFile(const QString &path)
{
    for (int i = 0; i < m_recentFiles.size(); i++) {
        if (m_recentFiles[i].path == path) {
            m_recentFiles.removeAt(i);
            saveSettings();
            emit recentFilesChanged();
            return;
        }
    }
}

void UftSessionManager::clearRecentFiles()
{
    /* Keep pinned */
    QList<UftRecentFile> pinned;
    for (const auto &rf : m_recentFiles) {
        if (rf.pinned) pinned.append(rf);
    }
    m_recentFiles = pinned;
    saveSettings();
    emit recentFilesChanged();
}

void UftSessionManager::pinRecentFile(const QString &path, bool pin)
{
    for (auto &rf : m_recentFiles) {
        if (rf.path == path) {
            rf.pinned = pin;
            saveSettings();
            emit recentFilesChanged();
            return;
        }
    }
}

QList<UftRecentFile> UftSessionManager::recentFiles() const
{
    return m_recentFiles;
}

int UftSessionManager::maxRecentFiles() const
{
    return m_maxRecentFiles;
}

void UftSessionManager::setMaxRecentFiles(int max)
{
    m_maxRecentFiles = max;
    saveSettings();
}

void UftSessionManager::saveSessionState(const UftSessionState &state)
{
    m_currentState = state;
    
    QSettings settings;
    settings.beginGroup("SessionState");
    
    QJsonDocument doc(state.toJson());
    settings.setValue("state", doc.toJson(QJsonDocument::Compact));
    
    settings.endGroup();
    
    emit sessionStateChanged();
}

UftSessionState UftSessionManager::loadSessionState() const
{
    QSettings settings;
    settings.beginGroup("SessionState");
    
    QByteArray data = settings.value("state").toByteArray();
    settings.endGroup();
    
    if (data.isEmpty()) return UftSessionState();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    return UftSessionState::fromJson(doc.object());
}

void UftSessionManager::clearSessionState()
{
    m_currentState = UftSessionState();
    
    QSettings settings;
    settings.beginGroup("SessionState");
    settings.remove("state");
    settings.endGroup();
    
    emit sessionStateChanged();
}

bool UftSessionManager::createProject(const QString &path, const QString &name)
{
    if (m_hasProject) {
        closeProject();
    }
    
    m_currentProject = UftProject();
    m_currentProject.name = name;
    m_currentProject.path = path;
    m_currentProject.created = QDateTime::currentDateTime();
    m_currentProject.modified = QDateTime::currentDateTime();
    
    m_hasProject = true;
    
    if (!saveProject()) {
        m_hasProject = false;
        return false;
    }
    
    addRecentProject(path);
    emit projectOpened(path);
    return true;
}

bool UftSessionManager::openProject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull()) {
        return false;
    }
    
    if (m_hasProject) {
        closeProject();
    }
    
    m_currentProject = UftProject::fromJson(doc.object());
    m_currentProject.path = path;
    m_hasProject = true;
    
    addRecentProject(path);
    emit projectOpened(path);
    return true;
}

bool UftSessionManager::saveProject()
{
    if (!m_hasProject || m_currentProject.path.isEmpty()) {
        return false;
    }
    
    m_currentProject.modified = QDateTime::currentDateTime();
    
    QFile file(m_currentProject.path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    QJsonDocument doc(m_currentProject.toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    
    emit projectSaved();
    return true;
}

bool UftSessionManager::saveProjectAs(const QString &path)
{
    m_currentProject.path = path;
    return saveProject();
}

bool UftSessionManager::closeProject()
{
    if (!m_hasProject) return true;
    
    m_currentProject = UftProject();
    m_hasProject = false;
    
    emit projectClosed();
    return true;
}

bool UftSessionManager::hasProject() const
{
    return m_hasProject;
}

UftProject UftSessionManager::currentProject() const
{
    return m_currentProject;
}

void UftSessionManager::enableAutoSave(bool enable, int intervalMs)
{
    m_autoSaveEnabled = enable;
    m_autoSaveInterval = intervalMs;
    
    if (enable) {
        m_autoSaveTimer->start(intervalMs);
    } else {
        m_autoSaveTimer->stop();
    }
}

bool UftSessionManager::isAutoSaveEnabled() const
{
    return m_autoSaveEnabled;
}

QStringList UftSessionManager::recentProjects() const
{
    return m_recentProjects;
}

void UftSessionManager::addRecentProject(const QString &path)
{
    m_recentProjects.removeAll(path);
    m_recentProjects.prepend(path);
    
    while (m_recentProjects.size() > 10) {
        m_recentProjects.removeLast();
    }
    
    saveSettings();
}

/*===========================================================================
 * UftSessionDialog
 *===========================================================================*/

UftSessionDialog::UftSessionDialog(QWidget *parent)
    : QDialog(parent)
    , m_action(ActionNone)
{
    setWindowTitle(tr("Welcome to UFT"));
    setMinimumSize(700, 450);
    setupUi();
    refresh();
}

void UftSessionDialog::setupUi()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    
    /* Left: Actions */
    QVBoxLayout *leftLayout = new QVBoxLayout();
    
    QLabel *logoLabel = new QLabel("<h1>UFT</h1><p>Unified Floppy Tool</p>");
    leftLayout->addWidget(logoLabel);
    
    m_actionList = new QListWidget();
    m_actionList->setMaximumWidth(180);
    m_actionList->addItem(new QListWidgetItem(
        QApplication::style()->standardIcon(QStyle::SP_FileIcon),
        tr("New Project")));
    m_actionList->addItem(new QListWidgetItem(
        QApplication::style()->standardIcon(QStyle::SP_DirOpenIcon),
        tr("Open Project")));
    m_actionList->addItem(new QListWidgetItem(
        QApplication::style()->standardIcon(QStyle::SP_FileDialogStart),
        tr("Open File")));
    leftLayout->addWidget(m_actionList);
    leftLayout->addStretch();
    
    mainLayout->addLayout(leftLayout);
    
    /* Center: Recent files */
    QVBoxLayout *centerLayout = new QVBoxLayout();
    
    m_recentGroup = new QGroupBox(tr("Recent Files"));
    QVBoxLayout *recentLayout = new QVBoxLayout(m_recentGroup);
    m_recentList = new QListWidget();
    recentLayout->addWidget(m_recentList);
    
    QHBoxLayout *recentBtnLayout = new QHBoxLayout();
    m_removeRecentBtn = new QPushButton(tr("Remove"));
    m_pinRecentBtn = new QPushButton(tr("Pin"));
    m_clearRecentBtn = new QPushButton(tr("Clear All"));
    recentBtnLayout->addWidget(m_removeRecentBtn);
    recentBtnLayout->addWidget(m_pinRecentBtn);
    recentBtnLayout->addStretch();
    recentBtnLayout->addWidget(m_clearRecentBtn);
    recentLayout->addLayout(recentBtnLayout);
    
    centerLayout->addWidget(m_recentGroup);
    
    m_projectsGroup = new QGroupBox(tr("Recent Projects"));
    QVBoxLayout *projectsLayout = new QVBoxLayout(m_projectsGroup);
    m_projectsList = new QListWidget();
    m_projectsList->setMaximumHeight(100);
    projectsLayout->addWidget(m_projectsList);
    centerLayout->addWidget(m_projectsGroup);
    
    mainLayout->addLayout(centerLayout);
    
    /* Right: Info */
    QVBoxLayout *rightLayout = new QVBoxLayout();
    
    m_infoGroup = new QGroupBox(tr("Information"));
    QVBoxLayout *infoLayout = new QVBoxLayout(m_infoGroup);
    m_infoLabel = new QLabel(tr("Select a file to see details"));
    m_infoLabel->setWordWrap(true);
    m_infoLabel->setMinimumWidth(200);
    infoLayout->addWidget(m_infoLabel);
    rightLayout->addWidget(m_infoGroup);
    rightLayout->addStretch();
    
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_openButton = new QPushButton(tr("Open"));
    m_openButton->setDefault(true);
    m_cancelButton = new QPushButton(tr("Cancel"));
    btnLayout->addStretch();
    btnLayout->addWidget(m_openButton);
    btnLayout->addWidget(m_cancelButton);
    rightLayout->addLayout(btnLayout);
    
    mainLayout->addLayout(rightLayout);
    
    /* Connections */
    connect(m_actionList, &QListWidget::currentRowChanged, [this](int row) {
        switch (row) {
            case 0: m_action = ActionNewProject; break;
            case 1: m_action = ActionOpenProject; break;
            case 2: m_action = ActionOpenFile; break;
        }
        updateButtons();
    });
    
    connect(m_recentList, &QListWidget::itemClicked, this, &UftSessionDialog::onRecentSelected);
    connect(m_recentList, &QListWidget::itemDoubleClicked, this, &UftSessionDialog::onRecentDoubleClicked);
    connect(m_projectsList, &QListWidget::itemClicked, this, &UftSessionDialog::onProjectSelected);
    
    connect(m_removeRecentBtn, &QPushButton::clicked, this, &UftSessionDialog::onRemoveRecent);
    connect(m_clearRecentBtn, &QPushButton::clicked, this, &UftSessionDialog::onClearRecent);
    connect(m_pinRecentBtn, &QPushButton::clicked, this, &UftSessionDialog::onPinRecent);
    
    connect(m_openButton, &QPushButton::clicked, [this]() {
        if (m_action == ActionNewProject) onNewProject();
        else if (m_action == ActionOpenProject) onOpenProject();
        else if (m_action == ActionOpenFile) onOpenFile();
        else if (m_action == ActionOpenRecent) accept();
    });
    
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

void UftSessionDialog::refresh()
{
    populateRecent();
    populateProjects();
}

void UftSessionDialog::populateRecent()
{
    m_recentList->clear();
    
    auto files = UftSessionManager::instance()->recentFiles();
    for (const auto &rf : files) {
        QFileInfo fi(rf.path);
        QString text = QString("%1 [%2]").arg(fi.fileName()).arg(rf.format);
        if (rf.pinned) text = "📌 " + text;
        
        QListWidgetItem *item = new QListWidgetItem(text);
        item->setData(Qt::UserRole, rf.path);
        item->setToolTip(rf.path);
        
        if (!fi.exists()) {
            item->setForeground(Qt::gray);
        }
        
        m_recentList->addItem(item);
    }
}

void UftSessionDialog::populateProjects()
{
    m_projectsList->clear();
    
    auto projects = UftSessionManager::instance()->recentProjects();
    for (const auto &path : projects) {
        QFileInfo fi(path);
        QListWidgetItem *item = new QListWidgetItem(fi.baseName());
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
        m_projectsList->addItem(item);
    }
}

void UftSessionDialog::updateButtons()
{
    m_openButton->setEnabled(m_action != ActionNone);
}

void UftSessionDialog::onNewProject()
{
    UftNewProjectDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        m_selectedPath = dlg.projectPath();
        m_action = ActionNewProject;
        accept();
    }
}

void UftSessionDialog::onOpenProject()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Open Project"),
                                                 QString(), tr("UFT Projects (*.uftproj)"));
    if (!path.isEmpty()) {
        m_selectedPath = path;
        m_action = ActionOpenProject;
        accept();
    }
}

void UftSessionDialog::onOpenFile()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Open Disk Image"));
    if (!path.isEmpty()) {
        m_selectedPath = path;
        m_action = ActionOpenFile;
        accept();
    }
}

void UftSessionDialog::onRecentSelected(QListWidgetItem *item)
{
    m_selectedPath = item->data(Qt::UserRole).toString();
    m_action = ActionOpenRecent;
    
    QFileInfo fi(m_selectedPath);
    QString info = QString("<b>%1</b><br><br>"
                           "Path: %2<br>"
                           "Size: %3<br>"
                           "Modified: %4")
        .arg(fi.fileName())
        .arg(fi.path())
        .arg(fi.size())
        .arg(fi.lastModified().toString());
    m_infoLabel->setText(info);
    
    updateButtons();
}

void UftSessionDialog::onRecentDoubleClicked(QListWidgetItem *item)
{
    m_selectedPath = item->data(Qt::UserRole).toString();
    m_action = ActionOpenRecent;
    accept();
}

void UftSessionDialog::onProjectSelected(QListWidgetItem *item)
{
    m_selectedPath = item->data(Qt::UserRole).toString();
    m_action = ActionOpenProject;
    updateButtons();
}

void UftSessionDialog::onRemoveRecent()
{
    if (m_recentList->currentItem()) {
        QString path = m_recentList->currentItem()->data(Qt::UserRole).toString();
        UftSessionManager::instance()->removeRecentFile(path);
        refresh();
    }
}

void UftSessionDialog::onClearRecent()
{
    if (QMessageBox::question(this, tr("Clear Recent"),
                               tr("Clear all non-pinned recent files?")) == QMessageBox::Yes) {
        UftSessionManager::instance()->clearRecentFiles();
        refresh();
    }
}

void UftSessionDialog::onPinRecent()
{
    if (m_recentList->currentItem()) {
        QString path = m_recentList->currentItem()->data(Qt::UserRole).toString();
        auto files = UftSessionManager::instance()->recentFiles();
        for (const auto &rf : files) {
            if (rf.path == path) {
                UftSessionManager::instance()->pinRecentFile(path, !rf.pinned);
                refresh();
                break;
            }
        }
    }
}

/*===========================================================================
 * UftNewProjectDialog
 *===========================================================================*/

UftNewProjectDialog::UftNewProjectDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("New Project"));
    setMinimumWidth(400);
    setupUi();
}

void UftNewProjectDialog::setupUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    QFormLayout *form = new QFormLayout();
    
    m_nameEdit = new QLineEdit();
    m_nameEdit->setPlaceholderText(tr("My Disk Project"));
    form->addRow(tr("Name:"), m_nameEdit);
    
    QHBoxLayout *locLayout = new QHBoxLayout();
    m_locationEdit = new QLineEdit();
    m_locationEdit->setText(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    m_browseBtn = new QPushButton(tr("..."));
    m_browseBtn->setMaximumWidth(30);
    locLayout->addWidget(m_locationEdit);
    locLayout->addWidget(m_browseBtn);
    form->addRow(tr("Location:"), locLayout);
    
    m_pathPreview = new QLineEdit();
    m_pathPreview->setReadOnly(true);
    m_pathPreview->setStyleSheet("background: palette(window);");
    form->addRow(tr("Path:"), m_pathPreview);
    
    m_descEdit = new QTextEdit();
    m_descEdit->setMaximumHeight(80);
    m_descEdit->setPlaceholderText(tr("Optional description..."));
    form->addRow(tr("Description:"), m_descEdit);
    
    layout->addLayout(form);
    
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_createBtn = new QPushButton(tr("Create"));
    m_createBtn->setDefault(true);
    m_cancelBtn = new QPushButton(tr("Cancel"));
    btnLayout->addStretch();
    btnLayout->addWidget(m_createBtn);
    btnLayout->addWidget(m_cancelBtn);
    layout->addLayout(btnLayout);
    
    connect(m_nameEdit, &QLineEdit::textChanged, this, &UftNewProjectDialog::updatePath);
    connect(m_locationEdit, &QLineEdit::textChanged, this, &UftNewProjectDialog::updatePath);
    connect(m_browseBtn, &QPushButton::clicked, this, &UftNewProjectDialog::browseLocation);
    connect(m_createBtn, &QPushButton::clicked, this, &UftNewProjectDialog::validate);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    
    updatePath();
}

void UftNewProjectDialog::browseLocation()
{
    QString path = QFileDialog::getExistingDirectory(this, tr("Select Location"),
                                                      m_locationEdit->text());
    if (!path.isEmpty()) {
        m_locationEdit->setText(path);
    }
}

void UftNewProjectDialog::updatePath()
{
    QString name = m_nameEdit->text().isEmpty() ? "untitled" : m_nameEdit->text();
    name = name.simplified().replace(' ', '_');
    QString path = m_locationEdit->text() + "/" + name + "/" + name + ".uftproj";
    m_pathPreview->setText(path);
}

void UftNewProjectDialog::validate()
{
    if (m_nameEdit->text().isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Please enter a project name."));
        return;
    }
    
    QString dir = QFileInfo(projectPath()).path();
    if (QDir().exists(dir)) {
        if (QMessageBox::question(this, tr("Directory Exists"),
                                   tr("The directory already exists. Continue?")) != QMessageBox::Yes) {
            return;
        }
    }
    
    accept();
}

QString UftNewProjectDialog::projectName() const
{
    return m_nameEdit->text();
}

QString UftNewProjectDialog::projectPath() const
{
    return m_pathPreview->text();
}

QString UftNewProjectDialog::projectDescription() const
{
    return m_descEdit->toPlainText();
}

/*===========================================================================
 * UftWorkspacePanel
 *===========================================================================*/

UftWorkspacePanel::UftWorkspacePanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    
    connect(UftSessionManager::instance(), &UftSessionManager::recentFilesChanged,
            this, &UftWorkspacePanel::onRecentFilesChanged);
    
    updateFromSession();
}

void UftWorkspacePanel::setupUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    
    /* Current file */
    m_currentGroup = new QGroupBox(tr("Current File"));
    QFormLayout *currentLayout = new QFormLayout(m_currentGroup);
    m_currentFile = new QLabel("-");
    m_currentFormat = new QLabel("-");
    m_currentSize = new QLabel("-");
    currentLayout->addRow(tr("File:"), m_currentFile);
    currentLayout->addRow(tr("Format:"), m_currentFormat);
    currentLayout->addRow(tr("Size:"), m_currentSize);
    layout->addWidget(m_currentGroup);
    
    /* Project */
    m_projectGroup = new QGroupBox(tr("Project"));
    QFormLayout *projectLayout = new QFormLayout(m_projectGroup);
    m_projectName = new QLabel("-");
    m_projectPath = new QLabel("-");
    m_saveProjectBtn = new QPushButton(tr("Save"));
    m_saveProjectBtn->setEnabled(false);
    projectLayout->addRow(tr("Name:"), m_projectName);
    projectLayout->addRow(tr("Path:"), m_projectPath);
    projectLayout->addRow("", m_saveProjectBtn);
    layout->addWidget(m_projectGroup);
    
    /* Quick access */
    m_quickGroup = new QGroupBox(tr("Quick Access"));
    QVBoxLayout *quickLayout = new QVBoxLayout(m_quickGroup);
    m_recentList = new QListWidget();
    m_recentList->setMaximumHeight(150);
    quickLayout->addWidget(m_recentList);
    layout->addWidget(m_quickGroup);
    
    layout->addStretch();
    
    connect(m_recentList, &QListWidget::itemDoubleClicked, [this](QListWidgetItem *item) {
        emit fileRequested(item->data(Qt::UserRole).toString());
    });
    
    connect(m_saveProjectBtn, &QPushButton::clicked, []() {
        UftSessionManager::instance()->saveProject();
    });
}

void UftWorkspacePanel::updateFromSession()
{
    auto session = UftSessionManager::instance();
    
    /* Update project info */
    if (session->hasProject()) {
        auto proj = session->currentProject();
        m_projectName->setText(proj.name);
        m_projectPath->setText(QFileInfo(proj.path).path());
        m_saveProjectBtn->setEnabled(true);
    } else {
        m_projectName->setText("-");
        m_projectPath->setText("-");
        m_saveProjectBtn->setEnabled(false);
    }
    
    onRecentFilesChanged();
}

void UftWorkspacePanel::onRecentFilesChanged()
{
    m_recentList->clear();
    
    auto files = UftSessionManager::instance()->recentFiles();
    int count = 0;
    for (const auto &rf : files) {
        if (count >= 8) break;
        
        QFileInfo fi(rf.path);
        QListWidgetItem *item = new QListWidgetItem(fi.fileName());
        item->setData(Qt::UserRole, rf.path);
        item->setToolTip(rf.path);
        m_recentList->addItem(item);
        count++;
    }
}
