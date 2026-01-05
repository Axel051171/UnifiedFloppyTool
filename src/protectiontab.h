/**
 * @file protectiontab.h
 * @brief Protection Tab Widget - GUI↔Backend Integration
 * 
 * P1-GUI-001: Complete backend integration for protection detection settings
 * 
 * @author UFT Team
 * @date 2026-01-05
 */

#ifndef PROTECTIONTAB_H
#define PROTECTIONTAB_H

#include <QWidget>
#include <QSettings>

// Forward declare C structs
extern "C" {
    #include "uft/uft_protection.h"
}

namespace Ui { class TabProtection; }

/**
 * @brief Protection settings tab with full backend integration
 * 
 * Maps 40+ UI widgets to uft_prot_config_t parameters.
 * Supports profile save/load and real-time validation.
 */
class ProtectionTab : public QWidget {
    Q_OBJECT

public:
    explicit ProtectionTab(QWidget *parent = nullptr);
    ~ProtectionTab();

    // ========================================================================
    // Backend Integration API
    // ========================================================================
    
    /**
     * @brief Get current protection analysis configuration
     * @param config Output configuration structure
     */
    void getConfig(uft_prot_config_t* config) const;
    
    /**
     * @brief Set protection analysis configuration (updates UI)
     * @param config Configuration to apply
     */
    void setConfig(const uft_prot_config_t* config);
    
    /**
     * @brief Get analysis flags from UI checkboxes
     * @return Combined UFT_PROT_ANAL_* flags
     */
    uint32_t getAnalysisFlags() const;
    
    /**
     * @brief Get platform hint from combo/checkboxes
     * @return Selected platform or UFT_PLATFORM_UNKNOWN for auto
     */
    uft_platform_t getPlatformHint() const;
    
    /**
     * @brief Get confidence threshold
     * @return Threshold 0-100 (default 70)
     */
    uint8_t getConfidenceThreshold() const;
    
    /**
     * @brief Check if auto-detection is enabled
     */
    bool isAutoDetectEnabled() const;
    
    /**
     * @brief Check if protection preservation is requested
     */
    bool isPreserveEnabled() const;
    
    /**
     * @brief Check if detailed reporting is enabled
     */
    bool isReportEnabled() const;

signals:
    /**
     * @brief Emitted when any protection setting changes
     */
    void configChanged();
    
    /**
     * @brief Emitted when profile selection changes
     * @param profileName Name of selected profile
     */
    void profileChanged(const QString& profileName);
    
    /**
     * @brief Emitted when platform selection changes
     * @param platform New platform hint
     */
    void platformChanged(uft_platform_t platform);

public slots:
    /**
     * @brief Load settings from QSettings
     */
    void loadSettings();
    
    /**
     * @brief Save current settings to QSettings
     */
    void saveSettings();
    
    /**
     * @brief Reset to default values
     */
    void resetDefaults();
    
    /**
     * @brief Apply a named preset profile
     * @param profileName Profile name from combo
     */
    void applyProfile(const QString& profileName);

private slots:
    // Profile management
    void onProfileChanged(int index);
    void onSaveProfile();
    void onLoadProfile();
    void onDeleteProfile();
    
    // Detection settings
    void onAutoDetectToggled(bool checked);
    void onPreserveToggled(bool checked);
    
    // Platform-specific tabs
    void onC64EnableToggled(bool checked);
    void onC64ExpertToggled(bool checked);
    void onDDEnableToggled(bool checked);
    void onDDExpertToggled(bool checked);
    void onXCopyEnableToggled(bool checked);
    
    // Generic flag changes
    void onAnyCheckboxChanged();
    void onSliderChanged(int value);
    void onSpinboxChanged(int value);

private:
    Ui::TabProtection *ui;
    
    // ========================================================================
    // Internal Helpers
    // ========================================================================
    
    void setupConnections();
    void setupDefaults();
    void updateDependentWidgets();
    void syncC64Widgets(bool enabled);
    void syncDDWidgets(bool enabled);
    void syncXCopyWidgets(bool enabled);
    
    // Profile helpers
    QString currentProfileName() const;
    void populateProfileCombo();
    QStringList savedProfiles() const;
    
    // Mapping helpers
    uint32_t mapC64Flags() const;
    uint32_t mapAmigaFlags() const;
    uint32_t mapAtariFlags() const;
    uint32_t mapGenericFlags() const;
    
    // Settings key
    static constexpr const char* SETTINGS_GROUP = "ProtectionTab";
};

#endif // PROTECTIONTAB_H
