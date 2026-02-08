/**
 * @file AnalyzerToolbar.cpp
 * @brief Kompakte Analyzer-Toolbar Implementierung
 */

#include "AnalyzerToolbar.h"
#include <QStyle>
#include <QTimer>

AnalyzerToolbar::AnalyzerToolbar(QWidget *parent)
    : QToolBar(parent)
    , m_hasResult(false)
    , m_analyzing(false)
{
    setObjectName("AnalyzerToolbar");
    setWindowTitle(tr("Track Analyzer"));
    setMovable(false);
    
    setupUi();
    setupConnections();
    clearResult();
}

void AnalyzerToolbar::setupUi()
{
    // Platform label
    m_platformLabel = new QLabel(this);
    m_platformLabel->setStyleSheet("font-weight: bold; padding: 0 8px;");
    m_platformLabel->setCursor(Qt::PointingHandCursor);
    m_platformLabel->setToolTip(tr("Click for details"));
    addWidget(m_platformLabel);
    
    // Separator 1
    m_sep1 = new QFrame(this);
    m_sep1->setFrameShape(QFrame::VLine);
    m_sep1->setFrameShadow(QFrame::Sunken);
    addWidget(m_sep1);
    
    // Encoding label
    m_encodingLabel = new QLabel(this);
    m_encodingLabel->setStyleSheet("color: #666; padding: 0 8px;");
    addWidget(m_encodingLabel);
    
    // Separator 2
    m_sep2 = new QFrame(this);
    m_sep2->setFrameShape(QFrame::VLine);
    m_sep2->setFrameShadow(QFrame::Sunken);
    addWidget(m_sep2);
    
    // Protection label
    m_protectionLabel = new QLabel(this);
    m_protectionLabel->setStyleSheet("padding: 0 8px;");
    addWidget(m_protectionLabel);
    
    // Separator 3
    m_sep3 = new QFrame(this);
    m_sep3->setFrameShape(QFrame::VLine);
    m_sep3->setFrameShadow(QFrame::Sunken);
    addWidget(m_sep3);
    
    // Mode combo
    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem("📄 Normal", static_cast<int>(CopyMode::Normal));
    m_modeCombo->addItem("📀 Track", static_cast<int>(CopyMode::TrackCopy));
    m_modeCombo->addItem("💾 Nibble", static_cast<int>(CopyMode::NibbleCopy));
    m_modeCombo->addItem("⚡ Flux", static_cast<int>(CopyMode::FluxCopy));
    m_modeCombo->addItem("🔀 Mixed", static_cast<int>(CopyMode::Mixed));
    m_modeCombo->setMinimumWidth(100);
    m_modeCombo->setToolTip(tr("Copy mode"));
    addWidget(m_modeCombo);
    
    addSeparator();
    
    // Confidence bar
    m_confidenceBar = new QProgressBar(this);
    m_confidenceBar->setRange(0, 100);
    m_confidenceBar->setTextVisible(true);
    m_confidenceBar->setFormat("%p%");
    m_confidenceBar->setFixedWidth(80);
    m_confidenceBar->setToolTip(tr("Detection confidence"));
    addWidget(m_confidenceBar);
    
    addSeparator();
    
    // Analyze button
    m_analyzeBtn = new QPushButton(tr("🔍 Quick Scan"), this);
    m_analyzeBtn->setToolTip(tr("Run quick analysis on first tracks"));
    addWidget(m_analyzeBtn);
    
    // Full analyze button
    m_fullAnalyzeBtn = new QPushButton(tr("📊 Full"), this);
    m_fullAnalyzeBtn->setToolTip(tr("Analyze all tracks (slower)"));
    addWidget(m_fullAnalyzeBtn);
    
    addSeparator();
    
    // Apply button
    m_applyBtn = new QPushButton(tr("✓ Apply"), this);
    m_applyBtn->setToolTip(tr("Apply settings to XCopy panel"));
    m_applyBtn->setEnabled(false);
    addWidget(m_applyBtn);
}

void AnalyzerToolbar::setupConnections()
{
    // Platform label click
    connect(m_platformLabel, &QLabel::linkActivated, this, [this]() {
        emit showDetailsRequested();
    });
    
    // Mode changed
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        emit modeChanged(getSelectedMode());
    });
    
    // Analyze buttons
    connect(m_analyzeBtn, &QPushButton::clicked, this, &AnalyzerToolbar::analyzeRequested);
    connect(m_fullAnalyzeBtn, &QPushButton::clicked, this, &AnalyzerToolbar::fullAnalysisRequested);
    
    // Apply button
    connect(m_applyBtn, &QPushButton::clicked, this, [this]() {
        emit applyRequested(getSelectedMode());
    });
}

void AnalyzerToolbar::setAnalysisResult(const ToolbarAnalysisResult &result)
{
    m_result = result;
    m_hasResult = true;
    m_analyzing = false;
    
    // Platform
    m_platformLabel->setText(QString("<a href='#'>%1</a>").arg(result.platform));
    
    // Encoding
    m_encodingLabel->setText(result.encoding);
    
    // Protection
    if (result.protectionDetected) {
        m_protectionLabel->setText(QString("🛡️ %1").arg(result.protectionName));
        m_protectionLabel->setStyleSheet("color: #c00; font-weight: bold; padding: 0 8px;");
    } else {
        m_protectionLabel->setText("✓ No Protection");
        m_protectionLabel->setStyleSheet("color: #080; padding: 0 8px;");
    }
    
    // Mode
    int modeIndex = m_modeCombo->findData(static_cast<int>(result.recommendedMode));
    if (modeIndex >= 0) {
        m_modeCombo->setCurrentIndex(modeIndex);
    }
    
    // Confidence
    m_confidenceBar->setValue(result.confidence);
    m_confidenceBar->setStyleSheet(QString(
        "QProgressBar { border: 1px solid #999; border-radius: 3px; background: #eee; }"
        "QProgressBar::chunk { background: %1; }"
    ).arg(getConfidenceColor(result.confidence).name()));
    
    // Enable apply
    m_applyBtn->setEnabled(true);
    
    // Re-enable analyze buttons
    m_analyzeBtn->setEnabled(true);
    m_analyzeBtn->setText(tr("🔍 Quick Scan"));
    m_fullAnalyzeBtn->setEnabled(true);
}

void AnalyzerToolbar::clearResult()
{
    m_hasResult = false;
    m_analyzing = false;
    
    m_platformLabel->setText(tr("No analysis"));
    m_encodingLabel->setText("-");
    m_protectionLabel->setText("-");
    m_protectionLabel->setStyleSheet("color: #666; padding: 0 8px;");
    m_confidenceBar->setValue(0);
    m_modeCombo->setCurrentIndex(0);
    m_applyBtn->setEnabled(false);
}

CopyMode AnalyzerToolbar::getSelectedMode() const
{
    return static_cast<CopyMode>(m_modeCombo->currentData().toInt());
}

void AnalyzerToolbar::setApplyEnabled(bool enabled)
{
    m_applyBtn->setEnabled(enabled && m_hasResult);
}

void AnalyzerToolbar::setAnalyzing(bool analyzing)
{
    m_analyzing = analyzing;
    
    if (analyzing) {
        m_analyzeBtn->setEnabled(false);
        m_analyzeBtn->setText(tr("⏳ Analyzing..."));
        m_fullAnalyzeBtn->setEnabled(false);
        m_platformLabel->setText(tr("Analyzing..."));
        m_protectionLabel->setText("-");
        
        // Start confidence animation
        m_confidenceBar->setRange(0, 0);  // Indeterminate
    } else {
        m_analyzeBtn->setEnabled(true);
        m_analyzeBtn->setText(tr("🔍 Quick Scan"));
        m_fullAnalyzeBtn->setEnabled(true);
        m_confidenceBar->setRange(0, 100);
    }
}

QString AnalyzerToolbar::getModeIcon(CopyMode mode) const
{
    switch (mode) {
        case CopyMode::Normal:     return "📄";
        case CopyMode::TrackCopy:  return "📀";
        case CopyMode::NibbleCopy: return "💾";
        case CopyMode::FluxCopy:   return "⚡";
        case CopyMode::Mixed:      return "🔀";
    }
    return "?";
}

QString AnalyzerToolbar::getModeName(CopyMode mode) const
{
    switch (mode) {
        case CopyMode::Normal:     return tr("Normal");
        case CopyMode::TrackCopy:  return tr("Track");
        case CopyMode::NibbleCopy: return tr("Nibble");
        case CopyMode::FluxCopy:   return tr("Flux");
        case CopyMode::Mixed:      return tr("Mixed");
    }
    return "?";
}

QColor AnalyzerToolbar::getConfidenceColor(int confidence) const
{
    if (confidence >= 90) return QColor("#2a2");  // Green
    if (confidence >= 70) return QColor("#6a2");  // Yellow-green
    if (confidence >= 50) return QColor("#aa2");  // Yellow
    if (confidence >= 30) return QColor("#a62");  // Orange
    return QColor("#a22");                         // Red
}

void AnalyzerToolbar::updateConfidenceAnimation()
{
    // Called by timer during analysis for animation effect
    if (m_analyzing) {
        // Animation handled by indeterminate mode
    }
}
