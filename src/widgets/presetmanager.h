/**
 * @file presetmanager.h
 * @brief Profile/Preset Management System
 * @version 3.0.0
 * 
 * 
 * FEATURES:
 * - Built-in presets (PC, Amiga, C64, Atari, Apple)
 * - User custom presets
 * - Import/Export (.uft-preset files)
 * - Auto-save/load
 * - Preset validation
 */

#pragma once

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QGroupBox>
#include <QLabel>
#include <QString>
#include <QSettings>
#include <QJsonObject>
#include <vector>

/**
 * @struct FormatPreset
 * @brief Complete disk format configuration
 */
struct FormatPreset
{
    // Metadata
    QString id;
    QString name;
    QString description;
    QString category;
    bool isBuiltIn;
    
    // Geometry
    int tracks;
    int sides;
    int sectorsPerTrack;
    int sectorSize;
    
    // Encoding
    enum class Encoding {
        MFM,
        FM,
        GCR,
        AUTO
    } encoding;
    
    int bitrate;
    int rpm;
    
    // Advanced
    int interleave;
    int gap3Length;
    int firstSectorID;
    bool autoDetectVariants;
    
    // Helper methods
    qint64 calculateCapacity() const;
    QJsonObject toJson() const;
    static FormatPreset fromJson(const QJsonObject& json);
    bool validate(QString* errorMsg = nullptr) const;
};

/**
 * @class PresetManager
 * @brief Manage format presets (built-in + user)
 */
class PresetManager : public QObject
{
    Q_OBJECT

public:
    static PresetManager& instance();
    
    const std::vector<FormatPreset>& getPresets() const { return m_presets; }
    const FormatPreset* getPreset(const QString& id) const;
    
    bool addPreset(const FormatPreset& preset);
    bool updatePreset(const QString& id, const FormatPreset& preset);
    bool deletePreset(const QString& id);
    
    bool importPreset(const QString& filepath);
    bool exportPreset(const QString& id, const QString& filepath);
    
    void loadPresets();
    void savePresets();

signals:
    void presetsChanged();
    void presetAdded(const QString& id);
    void presetDeleted(const QString& id);

private:
    PresetManager();
    ~PresetManager() = default;
    
    void initializeBuiltInPresets();
    
    std::vector<FormatPreset> m_presets;
};

/**
 * @class PresetManagerDialog
 * @brief GUI for preset management
 */
class PresetManagerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PresetManagerDialog(QWidget *parent = nullptr);
    
    const FormatPreset* getSelectedPreset() const;
    void selectPreset(const QString& id);

private slots:
    void onPresetSelected(QListWidgetItem* item);
    void onNewClicked();
    void onCopyClicked();
    void onDeleteClicked();
    void onImportClicked();
    void onExportClicked();
    void onSaveClicked();
    void onPresetChanged();

private:
    void createUI();
    void populatePresetList();
    void loadPresetDetails(const FormatPreset* preset);
    FormatPreset getCurrentPresetFromUI() const;
    void updateCommandPreview();
    
    // UI Components
    QListWidget*    m_presetList;
    QPushButton*    m_newButton;
    QPushButton*    m_copyButton;
    QPushButton*    m_deleteButton;
    QPushButton*    m_importButton;
    QPushButton*    m_exportButton;
    
    QLineEdit*      m_nameEdit;
    QTextEdit*      m_descriptionEdit;
    QComboBox*      m_categoryCombo;
    
    QSpinBox*       m_tracksSpinBox;
    QSpinBox*       m_sidesSpinBox;
    QSpinBox*       m_sectorsPerTrackSpinBox;
    QComboBox*      m_sectorSizeCombo;
    QLabel*         m_capacityLabel;
    
    QComboBox*      m_encodingCombo;
    QSpinBox*       m_bitrateSpinBox;
    QComboBox*      m_rpmCombo;
    
    QSpinBox*       m_interleaveSpinBox;
    QComboBox*      m_gap3Combo;
    QSpinBox*       m_firstSectorSpinBox;
    QCheckBox*      m_autoDetectCheck;
    
    QPushButton*    m_saveButton;
    QPushButton*    m_closeButton;
    
    QString         m_currentPresetId;
    bool            m_modified;
};
