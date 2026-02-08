/**
 * @file parameterpanelwidget.cpp
 * @brief Three-Tier Parameter Panel - Full Implementation
 * @version 3.8.0
 */

#include "parameterpanelwidget.h"
#include <QFormLayout>
#include <QFileDialog>
#include <QTextStream>
#include <QTimer>
#include <QToolTip>

// ============================================================================
// Constructor / Destructor
// ============================================================================

ParameterPanelWidget::ParameterPanelWidget(QWidget *parent)
    : QWidget(parent)
    , m_currentProfile(ProfileType::SAFE_READ)
    , m_showAdvanced(false)
    , m_expertMode(false)
{
    setupProfiles();
    setupParameters();
    setupUI();
    applyProfile(ProfileType::SAFE_READ);
}

ParameterPanelWidget::~ParameterPanelWidget()
{
}

// ============================================================================
// Profile Setup
// ============================================================================

void ParameterPanelWidget::setupProfiles()
{
    // Fast Read
    {
        ProfileDef p;
        p.type = ProfileType::FAST_READ;
        p.name = "Fast Read";
        p.description = "Quick scan with minimal retries. Best for known-good disks.";
        p.icon = "⚡";
        p.parameters = {
            {"retries", 1},
            {"multi_pass", false},
            {"pass_count", 1},
            {"verify", false},
            {"min_confidence", 50},
            {"timeout_ms", 3000}
        };
        m_profiles.push_back(p);
    }
    
    // Safe Read
    {
        ProfileDef p;
        p.type = ProfileType::SAFE_READ;
        p.name = "Safe Read";
        p.description = "Standard archival with verification. Recommended for most disks.";
        p.icon = "🛡️";
        p.parameters = {
            {"retries", 3},
            {"multi_pass", false},
            {"pass_count", 1},
            {"verify", true},
            {"min_confidence", 75},
            {"timeout_ms", 5000}
        };
        m_profiles.push_back(p);
    }
    
    // Recovery
    {
        ProfileDef p;
        p.type = ProfileType::RECOVERY;
        p.name = "Recovery";
        p.description = "Multi-pass reading with voting for damaged disks.";
        p.icon = "🔧";
        p.parameters = {
            {"retries", 10},
            {"multi_pass", true},
            {"pass_count", 5},
            {"voting_method", "majority"},
            {"verify", true},
            {"min_confidence", 75},
            {"timeout_ms", 10000},
            {"adaptive_pll", true}
        };
        m_profiles.push_back(p);
    }
    
    // Forensic
    {
        ProfileDef p;
        p.type = ProfileType::FORENSIC;
        p.name = "Forensic";
        p.description = "Maximum preservation with full logging. For evidence collection.";
        p.icon = "🔬";
        p.parameters = {
            {"retries", 20},
            {"multi_pass", true},
            {"pass_count", 10},
            {"voting_method", "weighted"},
            {"verify", true},
            {"min_confidence", 90},
            {"timeout_ms", 30000},
            {"adaptive_pll", true},
            {"save_all_passes", true},
            {"generate_hash", true},
            {"log_level", "debug"}
        };
        m_profiles.push_back(p);
    }
    
    // Write Standard
    {
        ProfileDef p;
        p.type = ProfileType::WRITE_STANDARD;
        p.name = "Write Standard";
        p.description = "Standard write with mandatory verification.";
        p.icon = "📝";
        p.parameters = {
            {"verify_after_write", true},
            {"precomp", "auto"},
            {"write_splice", "auto"},
            {"erase_empty_tracks", false}
        };
        m_profiles.push_back(p);
    }
    
    // Write Protected
    {
        ProfileDef p;
        p.type = ProfileType::WRITE_PROTECTED;
        p.name = "Write Protected";
        p.description = "For copy-protected disk duplication.";
        p.icon = "🔒";
        p.parameters = {
            {"verify_after_write", true},
            {"precomp", "manual"},
            {"write_splice", "manual"},
            {"preserve_weak_bits", true},
            {"preserve_timing", true}
        };
        m_profiles.push_back(p);
    }
    
    // Custom
    {
        ProfileDef p;
        p.type = ProfileType::CUSTOM;
        p.name = "Custom";
        p.description = "User-defined settings.";
        p.icon = "⚙️";
        m_profiles.push_back(p);
    }
}

// ============================================================================
// Parameter Setup
// ============================================================================

void ParameterPanelWidget::setupParameters()
{
    // Read Settings (Advanced)
    m_parameterDefs.push_back({
        "retries", "Retries", "Number of retry attempts for failed sectors",
        "Read", ParameterLevel::ADVANCED, ParameterRisk::LOW,
        3, 1, 50, {}, "", "", nullptr
    });
    
    m_parameterDefs.push_back({
        "timeout_ms", "Timeout (ms)", "Maximum time per track",
        "Read", ParameterLevel::ADVANCED, ParameterRisk::LOW,
        5000, 1000, 60000, {}, "", "", nullptr
    });
    
    m_parameterDefs.push_back({
        "rpm_tolerance", "RPM Tolerance (%)", "Allowed RPM deviation",
        "Read", ParameterLevel::ADVANCED, ParameterRisk::MEDIUM,
        2.0, 0.1, 10.0, {}, "", "", nullptr
    });
    
    // Recovery Settings (Advanced)
    m_parameterDefs.push_back({
        "multi_pass", "Multi-Pass", "Enable multiple read passes",
        "Recovery", ParameterLevel::ADVANCED, ParameterRisk::NONE,
        false, QVariant(), QVariant(), {}, "", "", nullptr
    });
    
    m_parameterDefs.push_back({
        "pass_count", "Pass Count", "Number of read passes",
        "Recovery", ParameterLevel::ADVANCED, ParameterRisk::LOW,
        5, 2, 20, {}, "multi_pass", "true", nullptr
    });
    
    m_parameterDefs.push_back({
        "voting_method", "Voting Method", "Method to combine multiple passes",
        "Recovery", ParameterLevel::ADVANCED, ParameterRisk::MEDIUM,
        "majority", QVariant(), QVariant(),
        {"majority", "weighted", "best", "unanimous"},
        "multi_pass", "true", nullptr
    });
    
    m_parameterDefs.push_back({
        "min_confidence", "Min Confidence (%)", "Minimum confidence threshold",
        "Recovery", ParameterLevel::ADVANCED, ParameterRisk::MEDIUM,
        75, 0, 100, {}, "", "", nullptr
    });
    
    m_parameterDefs.push_back({
        "adaptive_pll", "Adaptive PLL", "Auto-adjust PLL for timing drift",
        "Recovery", ParameterLevel::ADVANCED, ParameterRisk::LOW,
        false, QVariant(), QVariant(), {}, "", "", nullptr
    });
    
    // Output Settings (Advanced)
    m_parameterDefs.push_back({
        "verify", "Verify", "Verify data after read",
        "Output", ParameterLevel::ADVANCED, ParameterRisk::NONE,
        true, QVariant(), QVariant(), {}, "", "", nullptr
    });
    
    m_parameterDefs.push_back({
        "generate_hash", "Generate Hash", "Calculate SHA256 hash",
        "Output", ParameterLevel::ADVANCED, ParameterRisk::NONE,
        true, QVariant(), QVariant(), {}, "", "", nullptr
    });
    
    m_parameterDefs.push_back({
        "save_all_passes", "Save All Passes", "Keep data from all passes",
        "Output", ParameterLevel::ADVANCED, ParameterRisk::LOW,
        false, QVariant(), QVariant(), {}, "multi_pass", "true", nullptr
    });
    
    // Expert Settings (require confirmation)
    m_parameterDefs.push_back({
        "ignore_crc", "Ignore CRC", "⚠️ Accept data with CRC errors",
        "Expert", ParameterLevel::EXPERT, ParameterRisk::HIGH,
        false, QVariant(), QVariant(), {}, "", "", nullptr
    });
    
    m_parameterDefs.push_back({
        "force_density", "Force Density", "⚠️ Override auto-detected density",
        "Expert", ParameterLevel::EXPERT, ParameterRisk::HIGH,
        "auto", QVariant(), QVariant(),
        {"auto", "SD", "DD", "HD", "ED"}, "", "", nullptr
    });
    
    m_parameterDefs.push_back({
        "skip_verification", "Skip Verification", "⚠️ Disable write verification",
        "Expert", ParameterLevel::EXPERT, ParameterRisk::CRITICAL,
        false, QVariant(), QVariant(), {}, "", "", nullptr
    });
    
    m_parameterDefs.push_back({
        "raw_pll_bandwidth", "PLL Bandwidth", "Manual PLL bandwidth (0.001-0.1)",
        "Expert", ParameterLevel::EXPERT, ParameterRisk::HIGH,
        0.02, 0.001, 0.1, {}, "", "", nullptr
    });
    
    m_parameterDefs.push_back({
        "sync_threshold", "Sync Threshold", "Custom sync detection threshold",
        "Expert", ParameterLevel::EXPERT, ParameterRisk::MEDIUM,
        0x4489, 0, 0xFFFF, {}, "", "", nullptr
    });
}

// ============================================================================
// UI Setup
// ============================================================================

void ParameterPanelWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(10);
    
    // === Profile Selection ===
    m_profileGroup = new QGroupBox("📋 Profile", this);
    QVBoxLayout* profileLayout = new QVBoxLayout(m_profileGroup);
    
    m_profileCombo = new QComboBox(this);
    for (const auto& p : m_profiles) {
        m_profileCombo->addItem(QString("%1 %2").arg(p.icon).arg(p.name));
    }
    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ParameterPanelWidget::onProfileSelected);
    
    m_profileDescription = new QLabel(this);
    m_profileDescription->setWordWrap(true);
    m_profileDescription->setStyleSheet("color: gray; font-style: italic;");
    
    profileLayout->addWidget(m_profileCombo);
    profileLayout->addWidget(m_profileDescription);
    
    m_mainLayout->addWidget(m_profileGroup);
    
    // === View Controls ===
    QHBoxLayout* viewLayout = new QHBoxLayout();
    
    m_advancedCheck = new QCheckBox("Show Advanced Options", this);
    connect(m_advancedCheck, &QCheckBox::toggled,
            this, &ParameterPanelWidget::onAdvancedToggled);
    
    m_expertCheck = new QCheckBox("Expert Mode", this);
    m_expertCheck->setStyleSheet("color: orange;");
    connect(m_expertCheck, &QCheckBox::toggled,
            this, &ParameterPanelWidget::onExpertToggled);
    
    viewLayout->addWidget(m_advancedCheck);
    viewLayout->addWidget(m_expertCheck);
    viewLayout->addStretch();
    
    m_mainLayout->addLayout(viewLayout);
    
    // === Scrollable Parameter Area ===
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    
    m_scrollContent = new QWidget();
    m_scrollLayout = new QVBoxLayout(m_scrollContent);
    m_scrollLayout->setSpacing(10);
    
    // Read Settings Group
    m_readGroup = new QGroupBox("📖 Read Settings", m_scrollContent);
    QFormLayout* readLayout = new QFormLayout(m_readGroup);
    for (const auto& def : m_parameterDefs) {
        if (def.group == "Read") {
            createParameterWidget(def, readLayout);
        }
    }
    m_scrollLayout->addWidget(m_readGroup);
    
    // Recovery Settings Group
    m_recoveryGroup = new QGroupBox("🔧 Recovery Settings", m_scrollContent);
    QFormLayout* recoveryLayout = new QFormLayout(m_recoveryGroup);
    for (const auto& def : m_parameterDefs) {
        if (def.group == "Recovery") {
            createParameterWidget(def, recoveryLayout);
        }
    }
    m_scrollLayout->addWidget(m_recoveryGroup);
    
    // Output Settings Group
    m_outputGroup = new QGroupBox("📤 Output Settings", m_scrollContent);
    QFormLayout* outputLayout = new QFormLayout(m_outputGroup);
    for (const auto& def : m_parameterDefs) {
        if (def.group == "Output") {
            createParameterWidget(def, outputLayout);
        }
    }
    m_scrollLayout->addWidget(m_outputGroup);
    
    // Expert Settings Group
    m_expertGroup = new QGroupBox("⚠️ Expert Overrides", m_scrollContent);
    m_expertGroup->setStyleSheet("QGroupBox { color: orange; }");
    QVBoxLayout* expertLayout = new QVBoxLayout(m_expertGroup);
    
    QLabel* expertWarning = new QLabel(
        "⚠️ These settings can cause data corruption or hardware issues. "
        "Only use if you know what you're doing.", this);
    expertWarning->setWordWrap(true);
    expertWarning->setStyleSheet("color: red; background-color: #FFF3CD; padding: 5px;");
    expertLayout->addWidget(expertWarning);
    
    QFormLayout* expertFormLayout = new QFormLayout();
    for (const auto& def : m_parameterDefs) {
        if (def.group == "Expert") {
            createParameterWidget(def, expertFormLayout);
        }
    }
    expertLayout->addLayout(expertFormLayout);
    
    // Raw JSON editor
    QLabel* rawLabel = new QLabel("Raw Parameters (JSON):", this);
    m_rawParamsEdit = new QPlainTextEdit(this);
    m_rawParamsEdit->setMaximumHeight(100);
    m_rawParamsEdit->setPlaceholderText("{ \"custom_key\": \"value\" }");
    
    m_validateButton = new QPushButton("✓ Validieren", this);
    connect(m_validateButton, &QPushButton::clicked, [this]() {
        QStringList errors = getValidationErrors();
        
        if (errors.isEmpty()) {
            m_validationStatus->setText("✓ Alle Parameter gültig");
            m_validationStatus->setStyleSheet("color: #4CAF50; font-weight: bold;");
            
            // Kurze Erfolgsmeldung
            QToolTip::showText(m_validateButton->mapToGlobal(QPoint(0, -30)),
                              "✓ Alle Eingaben sind gültig!", 
                              m_validateButton, QRect(), 2000);
        } else {
            m_validationStatus->setText(QString("✕ %1 Fehler gefunden").arg(errors.size()));
            m_validationStatus->setStyleSheet("color: #F44336; font-weight: bold;");
            
            // Detaillierte Fehlermeldung anzeigen
            QString errorHtml = QString(
                "<div style='max-width: 400px; padding: 10px;'>"
                "<h3 style='color: #F44336; margin: 0 0 10px 0;'>"
                "⚠ Validierungsfehler</h3>"
                "<p style='color: #666;'>%1 Problem(e) gefunden:</p>"
                "<div style='background: #FFF8E1; padding: 10px; "
                "border-left: 3px solid #FFC107; margin: 5px 0;'>"
                "%2"
                "</div>"
                "<p style='font-size: 11px; color: #999; margin-top: 10px;'>"
                "💡 Korrigieren Sie die markierten Felder</p>"
                "</div>")
                .arg(errors.size())
                .arg(errors.join("<br><br>").replace("\n", "<br>"));
            
            QToolTip::showText(m_validateButton->mapToGlobal(QPoint(0, 30)),
                              errorHtml, m_validateButton, QRect(), 10000);
            
            // Signal mit Fehlern senden
            emit validationFailed(errors);
        }
    });
    
    m_validationStatus = new QLabel("", this);
    
    QHBoxLayout* rawButtonLayout = new QHBoxLayout();
    rawButtonLayout->addWidget(m_validateButton);
    rawButtonLayout->addWidget(m_validationStatus);
    rawButtonLayout->addStretch();
    
    expertLayout->addWidget(rawLabel);
    expertLayout->addWidget(m_rawParamsEdit);
    expertLayout->addLayout(rawButtonLayout);
    
    m_scrollLayout->addWidget(m_expertGroup);
    m_scrollLayout->addStretch();
    
    m_scrollArea->setWidget(m_scrollContent);
    m_mainLayout->addWidget(m_scrollArea, 1);
    
    // === Action Buttons ===
    QHBoxLayout* actionLayout = new QHBoxLayout();
    
    m_exportButton = new QPushButton("📤 Export", this);
    connect(m_exportButton, &QPushButton::clicked,
            this, &ParameterPanelWidget::onExportClicked);
    
    m_importButton = new QPushButton("📥 Import", this);
    connect(m_importButton, &QPushButton::clicked,
            this, &ParameterPanelWidget::onImportClicked);
    
    m_resetButton = new QPushButton("🔄 Reset", this);
    connect(m_resetButton, &QPushButton::clicked,
            this, &ParameterPanelWidget::onResetClicked);
    
    actionLayout->addWidget(m_exportButton);
    actionLayout->addWidget(m_importButton);
    actionLayout->addStretch();
    actionLayout->addWidget(m_resetButton);
    
    m_mainLayout->addLayout(actionLayout);
    
    // Initial visibility
    updateParameterVisibility();
}

void ParameterPanelWidget::createParameterWidget(const ParameterDef& def, QLayout* layout)
{
    QFormLayout* formLayout = qobject_cast<QFormLayout*>(layout);
    if (!formLayout) return;
    
    QWidget* widget = nullptr;
    QString labelText = def.label;
    
    // Add risk indicator
    if (def.risk >= ParameterRisk::HIGH) {
        labelText = riskToIcon(def.risk) + " " + labelText;
    }
    
    QLabel* label = new QLabel(labelText, this);
    label->setToolTip(def.tooltip);
    m_paramLabels[def.key] = label;
    
    // Create appropriate widget type
    if (!def.enumValues.isEmpty()) {
        // Enum -> ComboBox
        QComboBox* combo = new QComboBox(this);
        combo->addItems(def.enumValues);
        combo->setCurrentText(def.defaultValue.toString());
        combo->setProperty("param_key", def.key);
        connect(combo, &QComboBox::currentTextChanged, this, &ParameterPanelWidget::onParameterChanged);
        widget = combo;
    }
    else if (def.defaultValue.typeId() == QMetaType::Bool) {
        // Bool -> CheckBox
        QCheckBox* check = new QCheckBox(this);
        check->setChecked(def.defaultValue.toBool());
        check->setProperty("param_key", def.key);
        connect(check, &QCheckBox::toggled, this, &ParameterPanelWidget::onParameterChanged);
        widget = check;
    }
    else if (def.defaultValue.typeId() == QMetaType::Int) {
        // Int -> SpinBox
        QSpinBox* spin = new QSpinBox(this);
        spin->setRange(def.minValue.toInt(), def.maxValue.toInt());
        spin->setValue(def.defaultValue.toInt());
        spin->setProperty("param_key", def.key);
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), 
                this, &ParameterPanelWidget::onParameterChanged);
        widget = spin;
    }
    else if (def.defaultValue.typeId() == QMetaType::Double) {
        // Double -> DoubleSpinBox
        QDoubleSpinBox* spin = new QDoubleSpinBox(this);
        spin->setRange(def.minValue.toDouble(), def.maxValue.toDouble());
        spin->setValue(def.defaultValue.toDouble());
        spin->setDecimals(3);
        spin->setProperty("param_key", def.key);
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &ParameterPanelWidget::onParameterChanged);
        widget = spin;
    }
    else {
        // Default -> LineEdit
        QLineEdit* edit = new QLineEdit(this);
        edit->setText(def.defaultValue.toString());
        edit->setProperty("param_key", def.key);
        connect(edit, &QLineEdit::textChanged, this, &ParameterPanelWidget::onParameterChanged);
        widget = edit;
    }
    
    if (widget) {
        widget->setToolTip(def.tooltip);
        widget->setStyleSheet(riskToStyle(def.risk));
        m_paramWidgets[def.key] = widget;
        m_parameters[def.key] = def.defaultValue;
        formLayout->addRow(label, widget);
    }
}

// ============================================================================
// Profile Management
// ============================================================================

void ParameterPanelWidget::setProfile(ProfileType profile)
{
    for (int i = 0; i < static_cast<int>(m_profiles.size()); ++i) {
        if (m_profiles[i].type == profile) {
            m_profileCombo->setCurrentIndex(i);
            break;
        }
    }
}

void ParameterPanelWidget::applyProfile(ProfileType profile)
{
    m_currentProfile = profile;
    
    for (const auto& p : m_profiles) {
        if (p.type == profile) {
            m_profileDescription->setText(p.description);
            
            // Apply all profile parameters
            for (auto it = p.parameters.begin(); it != p.parameters.end(); ++it) {
                setParameter(it.key(), it.value());
            }
            break;
        }
    }
    
    emit profileChanged(profile);
}

void ParameterPanelWidget::addCustomProfile(const QString& name, const QMap<QString, QVariant>& params)
{
    ProfileDef p;
    p.type = ProfileType::CUSTOM;
    p.name = name;
    p.description = "User-defined profile";
    p.icon = "👤";
    p.parameters = params;
    
    m_profiles.push_back(p);
    m_profileCombo->addItem(QString("%1 %2").arg(p.icon).arg(p.name));
}

// ============================================================================
// Parameter Access
// ============================================================================

QVariant ParameterPanelWidget::getParameter(const QString& key) const
{
    return m_parameters.value(key);
}

void ParameterPanelWidget::setParameter(const QString& key, const QVariant& value)
{
    m_parameters[key] = value;
    
    // Update widget
    QWidget* widget = m_paramWidgets.value(key);
    if (!widget) return;
    
    if (QComboBox* combo = qobject_cast<QComboBox*>(widget)) {
        combo->setCurrentText(value.toString());
    }
    else if (QCheckBox* check = qobject_cast<QCheckBox*>(widget)) {
        check->setChecked(value.toBool());
    }
    else if (QSpinBox* spin = qobject_cast<QSpinBox*>(widget)) {
        spin->setValue(value.toInt());
    }
    else if (QDoubleSpinBox* spin = qobject_cast<QDoubleSpinBox*>(widget)) {
        spin->setValue(value.toDouble());
    }
    else if (QLineEdit* edit = qobject_cast<QLineEdit*>(widget)) {
        edit->setText(value.toString());
    }
}

QMap<QString, QVariant> ParameterPanelWidget::getAllParameters() const
{
    return m_parameters;
}

void ParameterPanelWidget::setAllParameters(const QMap<QString, QVariant>& params)
{
    for (auto it = params.begin(); it != params.end(); ++it) {
        setParameter(it.key(), it.value());
    }
}

// ============================================================================
// View Mode
// ============================================================================

void ParameterPanelWidget::setShowAdvanced(bool show)
{
    m_advancedCheck->setChecked(show);
}

void ParameterPanelWidget::setExpertMode(bool enabled)
{
    m_expertCheck->setChecked(enabled);
}

void ParameterPanelWidget::updateParameterVisibility()
{
    // Read/Recovery/Output groups visible with advanced
    m_readGroup->setVisible(m_showAdvanced);
    m_recoveryGroup->setVisible(m_showAdvanced);
    m_outputGroup->setVisible(m_showAdvanced);
    
    // Expert group only in expert mode
    m_expertGroup->setVisible(m_expertMode);
    
    // Update individual parameter visibility based on dependencies
    updateDependencies();
}

void ParameterPanelWidget::updateDependencies()
{
    for (const auto& def : m_parameterDefs) {
        if (def.dependsOn.isEmpty()) continue;
        
        QWidget* widget = m_paramWidgets.value(def.key);
        QLabel* label = m_paramLabels.value(def.key);
        if (!widget || !label) continue;
        
        // Check dependency
        QVariant depValue = m_parameters.value(def.dependsOn);
        bool satisfied = false;
        
        if (def.dependsCondition == "true") {
            satisfied = depValue.toBool();
        } else if (def.dependsCondition == "false") {
            satisfied = !depValue.toBool();
        } else {
            satisfied = (depValue.toString() == def.dependsCondition);
        }
        
        widget->setEnabled(satisfied);
        label->setEnabled(satisfied);
    }
}

// ============================================================================
// Validation
// ============================================================================

bool ParameterPanelWidget::validate() const
{
    return getValidationErrors().isEmpty();
}

QStringList ParameterPanelWidget::getValidationErrors() const
{
    QStringList errors;
    
    for (const auto& def : m_parameterDefs) {
        QVariant value = m_parameters.value(def.key);
        
        // Range check für Integer
        if (!def.minValue.isNull() && !def.maxValue.isNull()) {
            if (value.typeId() == QMetaType::Int) {
                int v = value.toInt();
                int minV = def.minValue.toInt();
                int maxV = def.maxValue.toInt();
                
                if (v < minV) {
                    errors << QString("⚠ %1: Wert %2 ist zu klein\n"
                                     "   → Minimum: %3\n"
                                     "   → Erhöhen Sie den Wert um %4")
                              .arg(def.label).arg(v)
                              .arg(minV).arg(minV - v);
                }
                else if (v > maxV) {
                    errors << QString("⚠ %1: Wert %2 ist zu groß\n"
                                     "   → Maximum: %3\n"
                                     "   → Verringern Sie den Wert um %4")
                              .arg(def.label).arg(v)
                              .arg(maxV).arg(v - maxV);
                }
            }
            else if (value.typeId() == QMetaType::Double) {
                double v = value.toDouble();
                double minV = def.minValue.toDouble();
                double maxV = def.maxValue.toDouble();
                
                if (v < minV) {
                    errors << QString("⚠ %1: Wert %.2f ist zu klein\n"
                                     "   → Minimum: %.2f")
                              .arg(def.label).arg(v).arg(minV);
                }
                else if (v > maxV) {
                    errors << QString("⚠ %1: Wert %.2f ist zu groß\n"
                                     "   → Maximum: %.2f")
                              .arg(def.label).arg(v).arg(maxV);
                }
            }
        }
        
        // Custom validator mit verbesserter Meldung
        if (def.validator && !def.validator(value)) {
            QString hint;
            
            // Kontextbezogene Hinweise
            if (def.key.contains("track", Qt::CaseInsensitive)) {
                hint = "\n   💡 Tracks: 0-79 (Standard), 0-83 (erweitert)";
            }
            else if (def.key.contains("sector", Qt::CaseInsensitive)) {
                hint = "\n   💡 Sektoren variieren je nach Format";
            }
            else if (def.key.contains("bitrate", Qt::CaseInsensitive)) {
                hint = "\n   💡 Standard: 250000 (DD) oder 500000 (HD)";
            }
            else if (def.key.contains("rpm", Qt::CaseInsensitive)) {
                hint = "\n   💡 Standard: 300 RPM (PC/Amiga)";
            }
            
            errors << QString("⚠ %1: Ungültiger Wert '%2'%3")
                      .arg(def.label)
                      .arg(value.toString())
                      .arg(hint);
        }
    }
    
    // Parse raw JSON if present
    if (!m_rawParamsEdit->toPlainText().trimmed().isEmpty()) {
        QJsonParseError error;
        QJsonDocument::fromJson(m_rawParamsEdit->toPlainText().toUtf8(), &error);
        if (error.error != QJsonParseError::NoError) {
            QString jsonHint;
            
            // Spezifische JSON-Fehlerhinweise
            switch (error.error) {
                case QJsonParseError::UnterminatedString:
                    jsonHint = "\n   💡 Prüfen Sie fehlende Anführungszeichen (\")";
                    break;
                case QJsonParseError::MissingNameSeparator:
                    jsonHint = "\n   💡 Fehlt ein Doppelpunkt (:) zwischen Schlüssel und Wert?";
                    break;
                case QJsonParseError::UnterminatedArray:
                case QJsonParseError::UnterminatedObject:
                    jsonHint = "\n   💡 Fehlt eine schließende Klammer (] oder })?";
                    break;
                case QJsonParseError::MissingValueSeparator:
                    jsonHint = "\n   💡 Fehlt ein Komma (,) zwischen Einträgen?";
                    break;
                default:
                    jsonHint = "\n   💡 Prüfen Sie die JSON-Syntax";
                    break;
            }
            
            errors << QString("⚠ JSON-Fehler an Position %1:\n"
                             "   %2%3")
                      .arg(error.offset)
                      .arg(error.errorString())
                      .arg(jsonHint);
        }
    }
    
    return errors;
}

// ============================================================================
// Export/Import
// ============================================================================

QJsonObject ParameterPanelWidget::toJson() const
{
    QJsonObject obj;
    
    obj["profile"] = static_cast<int>(m_currentProfile);
    
    QJsonObject params;
    for (auto it = m_parameters.begin(); it != m_parameters.end(); ++it) {
        params[it.key()] = QJsonValue::fromVariant(it.value());
    }
    obj["parameters"] = params;
    
    // Track overrides
    if (!m_trackOverrides.isEmpty()) {
        QJsonObject overrides;
        for (auto it = m_trackOverrides.begin(); it != m_trackOverrides.end(); ++it) {
            QJsonObject trackParams;
            for (auto pit = it.value().begin(); pit != it.value().end(); ++pit) {
                trackParams[pit.key()] = QJsonValue::fromVariant(pit.value());
            }
            overrides[it.key()] = trackParams;
        }
        obj["track_overrides"] = overrides;
    }
    
    return obj;
}

bool ParameterPanelWidget::fromJson(const QJsonObject& json)
{
    if (json.contains("profile")) {
        setProfile(static_cast<ProfileType>(json["profile"].toInt()));
    }
    
    if (json.contains("parameters")) {
        QJsonObject params = json["parameters"].toObject();
        for (auto it = params.begin(); it != params.end(); ++it) {
            setParameter(it.key(), it.value().toVariant());
        }
    }
    
    if (json.contains("track_overrides")) {
        m_trackOverrides.clear();
        QJsonObject overrides = json["track_overrides"].toObject();
        for (auto it = overrides.begin(); it != overrides.end(); ++it) {
            QMap<QString, QVariant> trackParams;
            QJsonObject trackObj = it.value().toObject();
            for (auto pit = trackObj.begin(); pit != trackObj.end(); ++pit) {
                trackParams[pit.key()] = pit.value().toVariant();
            }
            m_trackOverrides[it.key()] = trackParams;
        }
    }
    
    return true;
}

QString ParameterPanelWidget::toYaml() const
{
    QString yaml;
    QTextStream ts(&yaml);
    
    ts << "# UFT Parameters\n";
    ts << "profile: " << m_profileCombo->currentText() << "\n\n";
    ts << "parameters:\n";
    
    for (auto it = m_parameters.begin(); it != m_parameters.end(); ++it) {
        ts << "  " << it.key() << ": " << it.value().toString() << "\n";
    }
    
    return yaml;
}

bool ParameterPanelWidget::fromYaml(const QString& yaml)
{
    // Simple YAML parser for key: value format
    QStringList lines = yaml.split('\n');
    bool inParams = false;
    
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#')) continue;
        
        if (trimmed == "parameters:") {
            inParams = true;
            continue;
        }
        
        if (inParams && trimmed.contains(':')) {
            int idx = trimmed.indexOf(':');
            QString key = trimmed.left(idx).trimmed();
            QString value = trimmed.mid(idx + 1).trimmed();
            setParameter(key, value);
        }
    }
    
    return true;
}

QString ParameterPanelWidget::toCLI() const
{
    QStringList args;
    args << "uft";
    
    // Add profile
    switch (m_currentProfile) {
        case ProfileType::FAST_READ: args << "--profile fast"; break;
        case ProfileType::SAFE_READ: args << "--profile safe"; break;
        case ProfileType::RECOVERY: args << "--profile recovery"; break;
        case ProfileType::FORENSIC: args << "--profile forensic"; break;
        default: break;
    }
    
    // Add changed parameters
    for (auto it = m_parameters.begin(); it != m_parameters.end(); ++it) {
        QString key = QString(it.key()).replace('_', '-');
        QVariant val = it.value();
        
        if (val.typeId() == QMetaType::Bool) {
            if (val.toBool()) {
                args << QString("--%1").arg(key);
            }
        } else {
            args << QString("--%1 %2").arg(key).arg(val.toString());
        }
    }
    
    return args.join(" ");
}

// ============================================================================
// Track Overrides
// ============================================================================

void ParameterPanelWidget::setTrackOverride(int track, int head, 
                                            const QString& key, const QVariant& value)
{
    QString trackKey = QString("%1_%2").arg(track).arg(head);
    m_trackOverrides[trackKey][key] = value;
}

void ParameterPanelWidget::clearTrackOverrides(int track, int head)
{
    QString trackKey = QString("%1_%2").arg(track).arg(head);
    m_trackOverrides.remove(trackKey);
}

void ParameterPanelWidget::clearAllTrackOverrides()
{
    m_trackOverrides.clear();
}

QMap<QString, QVariant> ParameterPanelWidget::getTrackOverrides(int track, int head) const
{
    QString trackKey = QString("%1_%2").arg(track).arg(head);
    return m_trackOverrides.value(trackKey);
}

// ============================================================================
// Slots
// ============================================================================

void ParameterPanelWidget::onProfileSelected(int index)
{
    if (index >= 0 && index < static_cast<int>(m_profiles.size())) {
        applyProfile(m_profiles[index].type);
    }
}

void ParameterPanelWidget::onAdvancedToggled(bool checked)
{
    m_showAdvanced = checked;
    updateParameterVisibility();
    emit advancedModeChanged(checked);
}

void ParameterPanelWidget::onExpertToggled(bool checked)
{
    if (checked && !m_expertMode) {
        if (!confirmExpertMode()) {
            m_expertCheck->setChecked(false);
            return;
        }
    }
    
    m_expertMode = checked;
    updateParameterVisibility();
    emit expertModeChanged(checked);
}

void ParameterPanelWidget::onParameterChanged()
{
    QWidget* widget = qobject_cast<QWidget*>(sender());
    if (!widget) return;
    
    QString key = widget->property("param_key").toString();
    if (key.isEmpty()) return;
    
    QVariant value;
    if (QComboBox* combo = qobject_cast<QComboBox*>(widget)) {
        value = combo->currentText();
    }
    else if (QCheckBox* check = qobject_cast<QCheckBox*>(widget)) {
        value = check->isChecked();
    }
    else if (QSpinBox* spin = qobject_cast<QSpinBox*>(widget)) {
        value = spin->value();
    }
    else if (QDoubleSpinBox* spin = qobject_cast<QDoubleSpinBox*>(widget)) {
        value = spin->value();
    }
    else if (QLineEdit* edit = qobject_cast<QLineEdit*>(widget)) {
        value = edit->text();
    }
    
    // Check for risky changes
    for (const auto& def : m_parameterDefs) {
        if (def.key == key && def.risk >= ParameterRisk::HIGH) {
            if (!confirmRiskyChange(def, value)) {
                // Revert
                setParameter(key, m_parameters[key]);
                return;
            }
        }
    }
    
    m_parameters[key] = value;
    m_currentProfile = ProfileType::CUSTOM;
    
    updateDependencies();
    emit parameterChanged(key, value);
}

void ParameterPanelWidget::onExportClicked()
{
    QString fileName = QFileDialog::getSaveFileName(
        this, "Export Parameters", "", "JSON (*.json);;YAML (*.yaml)");
    
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    
    QTextStream out(&file);
    if (fileName.endsWith(".yaml")) {
        out << toYaml();
    } else {
        QJsonDocument doc(toJson());
        out << doc.toJson(QJsonDocument::Indented);
    }
    
    emit exportRequested();
}

void ParameterPanelWidget::onImportClicked()
{
    QString fileName = QFileDialog::getOpenFileName(
        this, "Import Parameters", "", "JSON (*.json);;YAML (*.yaml)");
    
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    
    QString content = file.readAll();
    
    if (fileName.endsWith(".yaml")) {
        fromYaml(content);
    } else {
        QJsonDocument doc = QJsonDocument::fromJson(content.toUtf8());
        fromJson(doc.object());
    }
    
    emit importRequested();
}

void ParameterPanelWidget::onResetClicked()
{
    applyProfile(m_currentProfile);
}

// ============================================================================
// Helper Functions
// ============================================================================

bool ParameterPanelWidget::confirmExpertMode()
{
    return QMessageBox::warning(
        this, "Enable Expert Mode",
        "Expert mode allows changes that can cause:\n\n"
        "• Data corruption\n"
        "• Hardware damage\n"
        "• Unreliable results\n\n"
        "Only enable if you understand the risks.\n\n"
        "Enable Expert Mode?",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No) == QMessageBox::Yes;
}

bool ParameterPanelWidget::confirmRiskyChange(const ParameterDef& def, const QVariant& newValue)
{
    QString message = QString("Changing '%1' to '%2' may cause issues.\n\n"
                             "Risk Level: %3\n\n"
                             "Continue?")
                      .arg(def.label)
                      .arg(newValue.toString())
                      .arg(def.risk == ParameterRisk::CRITICAL ? "CRITICAL" : "HIGH");
    
    return QMessageBox::warning(
        this, "Risky Parameter Change",
        message,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No) == QMessageBox::Yes;
}

QString ParameterPanelWidget::riskToStyle(ParameterRisk risk) const
{
    switch (risk) {
        case ParameterRisk::HIGH:
            return "border: 1px solid orange;";
        case ParameterRisk::CRITICAL:
            return "border: 2px solid red; background-color: #FFF0F0;";
        default:
            return "";
    }
}

QString ParameterPanelWidget::riskToIcon(ParameterRisk risk) const
{
    switch (risk) {
        case ParameterRisk::MEDIUM:  return "⚠️";
        case ParameterRisk::HIGH:    return "🔶";
        case ParameterRisk::CRITICAL:return "🔴";
        default:                     return "";
    }
}

// ============================================================================
// Verbesserte Validierungs-Feedback
// ============================================================================

void ParameterPanelWidget::showValidationWarning(const QString& key, const QString& message)
{
    // Widget finden
    QWidget* widget = m_paramWidgets.value(key, nullptr);
    QLabel* label = m_paramLabels.value(key, nullptr);
    
    if (!widget) return;
    
    // Stil anwenden
    widget->setStyleSheet("border: 2px solid #F44336; background-color: #FFEBEE;");
    
    // Parameterdefinition finden für besseren Kontext
    QString paramLabel = key;
    QString hint;
    QString example;
    
    for (const auto& def : m_parameterDefs) {
        if (def.key == key) {
            paramLabel = def.label;
            
            // Kontextbezogene Hinweise generieren
            if (!def.minValue.isNull() && !def.maxValue.isNull()) {
                hint = QString("Gültiger Bereich: %1 - %2")
                      .arg(def.minValue.toString())
                      .arg(def.maxValue.toString());
                example = QString("Beispiel: %1")
                         .arg((def.minValue.toDouble() + def.maxValue.toDouble()) / 2);
            }
            
            if (!def.enumValues.isEmpty()) {
                hint = QString("Erlaubte Werte: %1")
                      .arg(def.enumValues.join(", "));
                example = QString("Beispiel: %1").arg(def.enumValues.first());
            }
            break;
        }
    }
    
    // Tooltip mit detaillierter Fehlermeldung
    QString tooltip = QString(
        "<div style='max-width: 300px;'>"
        "<h3 style='color: #F44336; margin: 0;'>⚠ Eingabefehler</h3>"
        "<p><b>%1:</b> %2</p>"
        "%3"
        "%4"
        "</div>")
        .arg(paramLabel)
        .arg(message)
        .arg(hint.isEmpty() ? "" : QString("<p style='color: #666;'>💡 %1</p>").arg(hint))
        .arg(example.isEmpty() ? "" : QString("<p style='font-style: italic;'>%1</p>").arg(example));
    
    widget->setToolTip(tooltip);
    
    // Label markieren
    if (label) {
        label->setStyleSheet("color: #F44336; font-weight: bold;");
    }
    
    // Nach kurzer Zeit Tooltip anzeigen
    QTimer::singleShot(100, [widget, tooltip]() {
        QToolTip::showText(widget->mapToGlobal(QPoint(0, widget->height())), 
                          tooltip, widget, QRect(), 5000);
    });
}
