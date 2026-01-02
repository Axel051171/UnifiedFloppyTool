/**
 * @file settingsdialog.cpp
 * @brief Settings Dialog Implementation
 */

#include "settingsdialog.h"
#include "thememanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QSettings>
#include <QThread>
#include <QFrame>

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Settings"));
    setMinimumSize(400, 350);
    setModal(true);
    
    setupUI();
    loadSettings();
}

void SettingsDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(24, 24, 24, 24);

    //=========================================================================
    // Appearance Group
    //=========================================================================
    auto* grpAppearance = new QGroupBox(tr("Appearance"), this);
    auto* appearanceLayout = new QFormLayout(grpAppearance);
    appearanceLayout->setSpacing(12);

    // Theme Selector
    m_cmbTheme = new QComboBox(this);
    m_cmbTheme->addItem(tr("🌓 Auto (System)"), static_cast<int>(Theme::Auto));
    m_cmbTheme->addItem(tr("☀️ Light Mode"), static_cast<int>(Theme::Light));
    m_cmbTheme->addItem(tr("🌙 Dark Mode"), static_cast<int>(Theme::Dark));
    
    // Aktuelles Theme vorselektieren
    int currentTheme = static_cast<int>(ThemeManager::instance().configuredTheme());
    m_cmbTheme->setCurrentIndex(currentTheme);
    
    connect(m_cmbTheme, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onThemeChanged);
    
    appearanceLayout->addRow(tr("Theme:"), m_cmbTheme);

    // Animation Toggle
    m_chkAnimations = new QCheckBox(tr("Enable UI Animations"), this);
    m_chkAnimations->setChecked(true);
    appearanceLayout->addRow("", m_chkAnimations);

    mainLayout->addWidget(grpAppearance);

    //=========================================================================
    // Performance Group
    //=========================================================================
    auto* grpPerformance = new QGroupBox(tr("Performance"), this);
    auto* perfLayout = new QFormLayout(grpPerformance);
    perfLayout->setSpacing(12);

    // Thread Count
    m_spnThreads = new QSpinBox(this);
    m_spnThreads->setRange(1, QThread::idealThreadCount() * 2);
    m_spnThreads->setValue(QThread::idealThreadCount());
    m_spnThreads->setSuffix(tr(" threads"));
    
    auto* lblThreadHint = new QLabel(
        tr("(Detected: %1 cores)").arg(QThread::idealThreadCount()), this);
    lblThreadHint->setProperty("class", "subtle");
    
    auto* threadLayout = new QHBoxLayout();
    threadLayout->addWidget(m_spnThreads);
    threadLayout->addWidget(lblThreadHint);
    threadLayout->addStretch();
    
    perfLayout->addRow(tr("Worker Threads:"), threadLayout);

    // SIMD Toggle
    m_chkSIMD = new QCheckBox(tr("Enable SIMD Acceleration (SSE2/AVX2)"), this);
    m_chkSIMD->setChecked(true);
    perfLayout->addRow("", m_chkSIMD);

    mainLayout->addWidget(grpPerformance);

    //=========================================================================
    // Theme Preview
    //=========================================================================
    auto* grpPreview = new QGroupBox(tr("Preview"), this);
    auto* previewLayout = new QHBoxLayout(grpPreview);
    
    // Mini Preview Cards
    auto createPreviewCard = [this](const QString& title, bool isDark) {
        auto* card = new QFrame(this);
        card->setFixedSize(120, 80);
        card->setFrameStyle(QFrame::StyledPanel);
        
        if (isDark) {
            card->setStyleSheet(
                "QFrame { background-color: #1e1e2e; border: 2px solid #313244; border-radius: 8px; }"
            );
        } else {
            card->setStyleSheet(
                "QFrame { background-color: #f5f5f5; border: 2px solid #d0d0d0; border-radius: 8px; }"
            );
        }
        
        auto* cardLayout = new QVBoxLayout(card);
        auto* label = new QLabel(title, card);
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet(isDark ? "color: #cdd6f4;" : "color: #1a1a2e;");
        cardLayout->addWidget(label);
        
        return card;
    };
    
    previewLayout->addWidget(createPreviewCard(tr("Light"), false));
    previewLayout->addWidget(createPreviewCard(tr("Dark"), true));
    previewLayout->addStretch();
    
    mainLayout->addWidget(grpPreview);

    //=========================================================================
    // Spacer
    //=========================================================================
    mainLayout->addStretch();

    //=========================================================================
    // Buttons
    //=========================================================================
    auto* buttonLayout = new QHBoxLayout();
    
    m_btnReset = new QPushButton(tr("Reset to Defaults"), this);
    connect(m_btnReset, &QPushButton::clicked, this, &SettingsDialog::onResetClicked);
    
    m_btnApply = new QPushButton(tr("Apply"), this);
    m_btnApply->setProperty("primary", "true");
    connect(m_btnApply, &QPushButton::clicked, this, &SettingsDialog::onApplyClicked);
    
    m_btnClose = new QPushButton(tr("Close"), this);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);
    
    buttonLayout->addWidget(m_btnReset);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_btnApply);
    buttonLayout->addWidget(m_btnClose);
    
    mainLayout->addLayout(buttonLayout);
}

void SettingsDialog::onThemeChanged(int index)
{
    // Live Preview - Theme sofort anwenden
    Theme theme = static_cast<Theme>(m_cmbTheme->itemData(index).toInt());
    ThemeManager::instance().setTheme(theme);
}

void SettingsDialog::onApplyClicked()
{
    saveSettings();
}

void SettingsDialog::onResetClicked()
{
    m_cmbTheme->setCurrentIndex(0);  // Auto
    m_chkAnimations->setChecked(true);
    m_spnThreads->setValue(QThread::idealThreadCount());
    m_chkSIMD->setChecked(true);
    
    // Theme auf Auto setzen
    ThemeManager::instance().setTheme(Theme::Auto);
}

void SettingsDialog::loadSettings()
{
    QSettings settings("UFT", "UnifiedFloppyTool");
    
    // Theme wird vom ThemeManager geladen
    int themeIndex = static_cast<int>(ThemeManager::instance().configuredTheme());
    m_cmbTheme->setCurrentIndex(themeIndex);
    
    m_chkAnimations->setChecked(settings.value("appearance/animations", true).toBool());
    m_spnThreads->setValue(settings.value("performance/threads", QThread::idealThreadCount()).toInt());
    m_chkSIMD->setChecked(settings.value("performance/simd", true).toBool());
}

void SettingsDialog::saveSettings()
{
    QSettings settings("UFT", "UnifiedFloppyTool");
    
    // Theme wird vom ThemeManager gespeichert
    
    settings.setValue("appearance/animations", m_chkAnimations->isChecked());
    settings.setValue("performance/threads", m_spnThreads->value());
    settings.setValue("performance/simd", m_chkSIMD->isChecked());
}
