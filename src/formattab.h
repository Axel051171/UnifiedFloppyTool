/**
 * @file formattab.h
 * @brief Format/Settings Tab Widget - GUI↔Backend Integration
 * 
 * P1-GUI-001: Extended Protection settings integration
 * 
 * @author UFT Team
 * @date 2026-01-05
 */

#ifndef FORMATTAB_H
#define FORMATTAB_H

#include <QWidget>
#include <QSettings>

// Forward declare C structs
extern "C" {
    #include "uft/uft_protection.h"
}

namespace Ui { class TabFormat; }

/**
 * @brief Format/Settings tab with Protection detection integration
 */
class FormatTab : public QWidget {
    Q_OBJECT

public:
    explicit FormatTab(QWidget *parent = nullptr);
    ~FormatTab();

    // ========================================================================
    // Protection Settings API
    // ========================================================================
    
    /**
     * @brief Get protection analysis flags from UI
     * @return Combined UFT_PROT_ANAL_* flags
     */
    uint32_t getProtectionFlags() const;
    
    /**
     * @brief Get platform hint from combo
     * @return Selected platform or UFT_PLATFORM_UNKNOWN for auto
     */
    uft_platform_t getPlatformHint() const;
    
    /**
     * @brief Check if protection should be preserved
     */
    bool isPreserveProtection() const;
    
    /**
     * @brief Fill protection config from UI settings
     * @param config Output configuration
     */
    void getProtectionConfig(uft_prot_config_t* config) const;

signals:
    void protectionSettingsChanged();
    void formatSettingsChanged();

public slots:
    void loadSettings();
    void saveSettings();

private slots:
    void onDetectAllToggled(bool checked);
    void onPlatformChanged(int index);
    void onProtectionCheckChanged();

private:
    Ui::TabFormat *ui;
    void setupConnections();
    void syncProtectionWidgets(bool detectAll);
    
    static constexpr const char* SETTINGS_GROUP = "FormatTab";
};

#endif // FORMATTAB_H
