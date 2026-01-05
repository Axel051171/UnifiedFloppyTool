/**
 * @file thememanager.h
 * @brief Theme Manager für Dark/Light Mode Switching
 * @version 1.0.0
 * 
 * Features:
 * - Runtime Theme Switching ohne Restart
 * - System Theme Detection (Auto Mode)
 * - Persistente Theme-Einstellungen
 * - Smooth Transition Support
 */

#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QString>
#include <QSettings>
#include <QApplication>

/**
 * @brief Theme Enumeration
 */
enum class Theme {
    Auto = 0,   ///< Folgt System-Einstellung
    Light = 1,  ///< Heller Modus
    Dark = 2    ///< Dunkler Modus
};

/**
 * @brief Singleton Theme Manager
 * 
 * Verwaltet Application-weites Theme Switching.
 * 
 * Usage:
 * @code
 * // Theme setzen
 * ThemeManager::instance().setTheme(Theme::Dark);
 * 
 * // Aktuelles Theme abfragen
 * Theme current = ThemeManager::instance().currentTheme();
 * 
 * // Auf Änderungen reagieren
 * connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
 *         this, &MyWidget::onThemeChanged);
 * @endcode
 */
class ThemeManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Singleton Instance
     */
    static ThemeManager& instance();

    /**
     * @brief Aktuelles Theme (resolved - nie Auto)
     */
    Theme currentTheme() const { return m_resolvedTheme; }

    /**
     * @brief Konfiguriertes Theme (kann Auto sein)
     */
    Theme configuredTheme() const { return m_configuredTheme; }

    /**
     * @brief Theme Name für UI
     */
    QString themeName() const;

    /**
     * @brief Prüft ob aktuell Dark Mode aktiv
     */
    bool isDarkMode() const { return m_resolvedTheme == Theme::Dark; }

    /**
     * @brief Prüft ob System Dark Mode verwendet
     */
    static bool isSystemDarkMode();

public slots:
    /**
     * @brief Setzt neues Theme
     * @param theme Gewünschtes Theme (Auto/Light/Dark)
     */
    void setTheme(Theme theme);

    /**
     * @brief Wechselt zwischen Light und Dark
     */
    void toggleTheme();

    /**
     * @brief Lädt Theme aus Settings
     */
    void loadFromSettings();

    /**
     * @brief Speichert Theme in Settings
     */
    void saveToSettings();

signals:
    /**
     * @brief Emittiert bei Theme-Änderung
     * @param newTheme Das neue aktive Theme
     */
    void themeChanged(Theme newTheme);

    /**
     * @brief Emittiert vor Theme-Änderung (für Animationen)
     */
    void themeAboutToChange();

private:
    explicit ThemeManager(QObject* parent = nullptr);
    ~ThemeManager() override = default;

    // Singleton - nicht kopierbar
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;

    void applyTheme();
    void resolveTheme();
    QString loadStyleSheet(const QString& name);

    Theme m_configuredTheme = Theme::Auto;
    Theme m_resolvedTheme = Theme::Dark;

    // Stylesheet Cache
    QString m_darkStyleSheet;
    QString m_lightStyleSheet;
};

#endif // THEMEMANAGER_H
