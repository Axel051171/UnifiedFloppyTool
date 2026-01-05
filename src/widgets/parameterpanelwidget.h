/**
 * @file parameterpanelwidget.h
 * @brief Three-Tier Parameter Panel (Profile/Advanced/Expert)
 * @version 4.0.0
 * 
 * FEATURES:
 * - Profile-based quick selection (Anfänger)
 * - Advanced parameters with context sensitivity (Fortgeschritten)
 * - Expert overrides with warnings (Experten)
 * - Automatic validation and dependencies
 * - JSON/YAML export/import
 * - CLI command generation
 */

#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QStackedWidget>
#include <QScrollArea>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QMessageBox>
#include <QJsonObject>
#include <QJsonDocument>
#include <map>
#include <vector>
#include <functional>

/**
 * @enum ParameterLevel
 * @brief Parameter visibility level
 */
enum class ParameterLevel
{
    PROFILE,    // Always visible - preset selection
    ADVANCED,   // Visible when "Show Advanced" is checked
    EXPERT      // Visible only in Expert Mode with confirmation
};

/**
 * @enum ParameterRisk
 * @brief Risk level for parameter changes
 */
enum class ParameterRisk
{
    NONE,       // No risk
    LOW,        // Minor impact
    MEDIUM,     // May affect results
    HIGH,       // Can cause data loss or corruption
    CRITICAL    // Requires explicit confirmation
};

/**
 * @enum ProfileType
 * @brief Built-in profile types
 */
enum class ProfileType
{
    FAST_READ,
    SAFE_READ,
    RECOVERY,
    FORENSIC,
    WRITE_STANDARD,
    WRITE_PROTECTED,
    CUSTOM
};

/**
 * @struct ParameterDef
 * @brief Parameter definition
 */
struct ParameterDef
{
    QString key;
    QString label;
    QString tooltip;
    QString group;
    ParameterLevel level;
    ParameterRisk risk;
    QVariant defaultValue;
    QVariant minValue;
    QVariant maxValue;
    QStringList enumValues;
    QString dependsOn;
    QString dependsCondition;
    std::function<bool(const QVariant&)> validator;
};

/**
 * @struct ProfileDef
 * @brief Profile definition with all parameters
 */
struct ProfileDef
{
    ProfileType type;
    QString name;
    QString description;
    QString icon;
    QMap<QString, QVariant> parameters;
};

/**
 * @class ParameterPanelWidget
 * @brief Three-tier parameter configuration panel
 */
class ParameterPanelWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ParameterPanelWidget(QWidget *parent = nullptr);
    virtual ~ParameterPanelWidget();
    
    // Profile management
    void setProfile(ProfileType profile);
    ProfileType currentProfile() const { return m_currentProfile; }
    void addCustomProfile(const QString& name, const QMap<QString, QVariant>& params);
    
    // Parameter access
    QVariant getParameter(const QString& key) const;
    void setParameter(const QString& key, const QVariant& value);
    QMap<QString, QVariant> getAllParameters() const;
    void setAllParameters(const QMap<QString, QVariant>& params);
    
    // View mode
    void setShowAdvanced(bool show);
    bool showAdvanced() const { return m_showAdvanced; }
    void setExpertMode(bool enabled);
    bool expertMode() const { return m_expertMode; }
    
    // Validation
    bool validate() const;
    QStringList getValidationErrors() const;
    
    // Export/Import
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);
    QString toYaml() const;
    bool fromYaml(const QString& yaml);
    QString toCLI() const;
    
    // Track-level overrides
    void setTrackOverride(int track, int head, const QString& key, const QVariant& value);
    void clearTrackOverrides(int track, int head);
    void clearAllTrackOverrides();
    QMap<QString, QVariant> getTrackOverrides(int track, int head) const;

signals:
    void profileChanged(ProfileType profile);
    void parameterChanged(const QString& key, const QVariant& value);
    void advancedModeChanged(bool enabled);
    void expertModeChanged(bool enabled);
    void validationFailed(const QStringList& errors);
    void exportRequested();
    void importRequested();

private slots:
    void onProfileSelected(int index);
    void onAdvancedToggled(bool checked);
    void onExpertToggled(bool checked);
    void onParameterChanged();
    void onExportClicked();
    void onImportClicked();
    void onResetClicked();

private:
    void setupUI();
    void setupProfiles();
    void setupParameters();
    void createParameterWidget(const ParameterDef& def, QLayout* layout);
    void updateParameterVisibility();
    void updateDependencies();
    void applyProfile(ProfileType profile);
    bool confirmExpertMode();
    bool confirmRiskyChange(const ParameterDef& def, const QVariant& newValue);
    void showValidationWarning(const QString& key, const QString& message);
    QString riskToStyle(ParameterRisk risk) const;
    QString riskToIcon(ParameterRisk risk) const;
    
    // Data
    ProfileType m_currentProfile;
    bool m_showAdvanced;
    bool m_expertMode;
    std::vector<ProfileDef> m_profiles;
    std::vector<ParameterDef> m_parameterDefs;
    QMap<QString, QVariant> m_parameters;
    QMap<QString, QMap<QString, QVariant>> m_trackOverrides; // "track_head" -> params
    
    // UI Components
    QVBoxLayout* m_mainLayout;
    
    // Profile section
    QGroupBox* m_profileGroup;
    QComboBox* m_profileCombo;
    QLabel* m_profileDescription;
    
    // View controls
    QCheckBox* m_advancedCheck;
    QCheckBox* m_expertCheck;
    
    // Parameter sections
    QScrollArea* m_scrollArea;
    QWidget* m_scrollContent;
    QVBoxLayout* m_scrollLayout;
    
    QGroupBox* m_readGroup;
    QGroupBox* m_recoveryGroup;
    QGroupBox* m_outputGroup;
    QGroupBox* m_expertGroup;
    
    // Parameter widgets (key -> widget)
    QMap<QString, QWidget*> m_paramWidgets;
    QMap<QString, QLabel*> m_paramLabels;
    
    // Expert section
    QPlainTextEdit* m_rawParamsEdit;
    QPushButton* m_validateButton;
    QLabel* m_validationStatus;
    
    // Actions
    QPushButton* m_exportButton;
    QPushButton* m_importButton;
    QPushButton* m_resetButton;
};
