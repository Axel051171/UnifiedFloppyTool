/**
 * @file thememanager.cpp
 * @brief Theme Manager Implementation
 */

#include "thememanager.h"
#include <QFile>
#include <QStyleHints>
#include <QGuiApplication>
#include <QPalette>
#include <QDebug>

#ifdef Q_OS_WIN
#include <QSettings>
#endif

ThemeManager& ThemeManager::instance()
{
    static ThemeManager instance;
    return instance;
}

ThemeManager::ThemeManager(QObject* parent)
    : QObject(parent)
{
    // Cache Stylesheets beim Start
    m_darkStyleSheet = loadStyleSheet("darkmode");
    m_lightStyleSheet = loadStyleSheet("lightmode");

    // System Theme Changes überwachen (Qt 6.5+)
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, [this](Qt::ColorScheme scheme) {
                Q_UNUSED(scheme)
                if (m_configuredTheme == Theme::Auto) {
                    resolveTheme();
                    applyTheme();
                }
            });
#endif

    // Initial Load
    loadFromSettings();
}

QString ThemeManager::themeName() const
{
    switch (m_resolvedTheme) {
        case Theme::Light: return tr("Light");
        case Theme::Dark: return tr("Dark");
        default: return tr("Auto");
    }
}

bool ThemeManager::isSystemDarkMode()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // Qt 6.5+ hat native Unterstützung
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    // Fallback für ältere Qt Versionen

#ifdef Q_OS_WIN
    // Windows: Registry abfragen
    QSettings settings(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        QSettings::NativeFormat
    );
    return settings.value("AppsUseLightTheme", 1).toInt() == 0;
#endif

#ifdef Q_OS_MACOS
    // macOS: Palette-basierte Erkennung
    const QPalette palette = QGuiApplication::palette();
    const QColor windowColor = palette.color(QPalette::Window);
    return windowColor.lightness() < 128;
#endif

#ifdef Q_OS_LINUX
    // Linux: Palette oder Environment Variable
    const char* desktop = qgetenv("XDG_CURRENT_DESKTOP").constData();
    const char* gtkTheme = qgetenv("GTK_THEME").constData();
    
    // Heuristik: "dark" im Theme-Namen
    QString theme = QString::fromUtf8(gtkTheme).toLower();
    if (theme.contains("dark")) {
        return true;
    }
    
    // Fallback: Palette prüfen
    const QPalette palette = QGuiApplication::palette();
    return palette.color(QPalette::Window).lightness() < 128;
#endif

    // Default: Light Mode
    return false;
#endif
}

void ThemeManager::setTheme(Theme theme)
{
    if (m_configuredTheme == theme) {
        return;
    }

    emit themeAboutToChange();

    m_configuredTheme = theme;
    resolveTheme();
    applyTheme();
    saveToSettings();

    emit themeChanged(m_resolvedTheme);
}

void ThemeManager::toggleTheme()
{
    if (m_resolvedTheme == Theme::Dark) {
        setTheme(Theme::Light);
    } else {
        setTheme(Theme::Dark);
    }
}

void ThemeManager::loadFromSettings()
{
    QSettings settings("UFT", "UnifiedFloppyTool");
    int themeValue = settings.value("appearance/theme", static_cast<int>(Theme::Auto)).toInt();
    
    m_configuredTheme = static_cast<Theme>(qBound(0, themeValue, 2));
    resolveTheme();
    applyTheme();
}

void ThemeManager::saveToSettings()
{
    QSettings settings("UFT", "UnifiedFloppyTool");
    settings.setValue("appearance/theme", static_cast<int>(m_configuredTheme));
}

void ThemeManager::resolveTheme()
{
    if (m_configuredTheme == Theme::Auto) {
        m_resolvedTheme = isSystemDarkMode() ? Theme::Dark : Theme::Light;
    } else {
        m_resolvedTheme = m_configuredTheme;
    }
}

void ThemeManager::applyTheme()
{
    QString styleSheet;
    
    if (m_resolvedTheme == Theme::Dark) {
        styleSheet = m_darkStyleSheet;
    } else {
        styleSheet = m_lightStyleSheet;
    }

    if (styleSheet.isEmpty()) {
        qWarning() << "ThemeManager: StyleSheet is empty!";
        return;
    }

    qApp->setStyleSheet(styleSheet);
    
    qDebug() << "ThemeManager: Applied" << (m_resolvedTheme == Theme::Dark ? "Dark" : "Light") << "theme";
}

QString ThemeManager::loadStyleSheet(const QString& name)
{
    QString path = QString(":/styles/%1.qss").arg(name);
    QFile file(path);
    
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        qWarning() << "ThemeManager: Failed to load" << path;
        return QString();
    }
    
    QString content = QString::fromUtf8(file.readAll());
    file.close();
    
    return content;
}
