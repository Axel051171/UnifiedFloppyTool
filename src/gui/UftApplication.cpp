/**
 * @file UftApplication.cpp
 * @brief Application Singleton Implementation
 */

#include "UftApplication.h"
#include "UftMainController.h"
#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QDebug>

UftApplication* UftApplication::s_instance = nullptr;

UftApplication* UftApplication::instance()
{
    if (!s_instance) {
        s_instance = new UftApplication(qApp);
    }
    return s_instance;
}

UftApplication::UftApplication(QObject *parent)
    : QObject(parent)
{
    initSettings();
    initController();
    loadSettings();
}

UftApplication::~UftApplication()
{
    saveSettings();
    delete m_controller;
    delete m_settings;
    s_instance = nullptr;
}

void UftApplication::initController()
{
    m_controller = new UftMainController(this);
}

void UftApplication::initSettings()
{
    m_settings = new QSettings(
        QSettings::IniFormat,
        QSettings::UserScope,
        "UFT", "UnifiedFloppyTool");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Settings
 * ═══════════════════════════════════════════════════════════════════════════════ */

void UftApplication::saveSetting(const QString &key, const QVariant &value)
{
    m_settings->setValue(key, value);
}

QVariant UftApplication::loadSetting(const QString &key, const QVariant &defaultValue)
{
    return m_settings->value(key, defaultValue);
}

void UftApplication::saveSettings()
{
    m_settings->setValue("theme/darkMode", m_darkMode);
    m_settings->setValue("recentFiles", m_recentFiles);
    m_settings->sync();
}

void UftApplication::loadSettings()
{
    m_darkMode = m_settings->value("theme/darkMode", false).toBool();
    m_recentFiles = m_settings->value("recentFiles").toStringList();
    
    /* Remove non-existent files from recent list */
    m_recentFiles.erase(
        std::remove_if(m_recentFiles.begin(), m_recentFiles.end(),
            [](const QString &path) { return !QFile::exists(path); }),
        m_recentFiles.end());
    
    applyTheme();
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Recent Files
 * ═══════════════════════════════════════════════════════════════════════════════ */

void UftApplication::addRecentFile(const QString &path)
{
    /* Remove if already in list */
    m_recentFiles.removeAll(path);
    
    /* Add to front */
    m_recentFiles.prepend(path);
    
    /* Limit to 10 entries */
    while (m_recentFiles.size() > 10) {
        m_recentFiles.removeLast();
    }
    
    emit recentFilesChanged();
}

void UftApplication::clearRecentFiles()
{
    m_recentFiles.clear();
    emit recentFilesChanged();
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Theme
 * ═══════════════════════════════════════════════════════════════════════════════ */

void UftApplication::setDarkMode(bool dark)
{
    if (m_darkMode != dark) {
        m_darkMode = dark;
        applyTheme();
        emit themeChanged();
    }
}

void UftApplication::applyTheme()
{
    if (qApp) {
        if (m_darkMode) {
            qApp->setStyleSheet(UftTheme::darkStyleSheet());
        } else {
            qApp->setStyleSheet(UftTheme::lightStyleSheet());
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Logging
 * ═══════════════════════════════════════════════════════════════════════════════ */

QString UftApplication::logFilePath() const
{
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(logDir);
    return QDir(logDir).filePath("uft.log");
}

void UftApplication::log(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString logLine = QString("[%1] %2").arg(timestamp, message);
    
    QFile file(logFilePath());
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << logLine << "\n";
    }
    
    emit logMessage(logLine);
    qDebug() << logLine;
}

void UftApplication::logError(const QString &error)
{
    log(QString("[ERROR] %1").arg(error));
}
