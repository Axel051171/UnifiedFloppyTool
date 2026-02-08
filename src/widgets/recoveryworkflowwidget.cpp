/**
 * @file recoveryworkflowwidget.cpp
 * @brief Recovery Workflow Widget - Full Implementation
 * @version 4.0.0
 */

#include "recoveryworkflowwidget.h"
#include <QHeaderView>
#include <QFont>

// ============================================================================
// Constructor / Destructor
// ============================================================================

RecoveryWorkflowWidget::RecoveryWorkflowWidget(QWidget *parent)
    : QWidget(parent)
    , m_currentTrack(-1)
    , m_currentHead(-1)
    , m_currentPass(0)
    , m_totalPasses(5)
    , m_maxPasses(10)
    , m_minConfidence(75)
    , m_isRunning(false)
{
    setupUI();
}

RecoveryWorkflowWidget::~RecoveryWorkflowWidget()
{
}

// ============================================================================
// Setup UI
// ============================================================================

void RecoveryWorkflowWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(10);
    
    // === Problem Detection Section ===
    m_problemGroup = new QGroupBox("🔍 Detected Problems", this);
    QVBoxLayout* problemLayout = new QVBoxLayout(m_problemGroup);
    
    m_problemTable = new QTableWidget(this);
    m_problemTable->setColumnCount(6);
    m_problemTable->setHorizontalHeaderLabels({
        "Track", "Problem", "Suggestion", "Est. Time", "Success %", "Description"
    });
    m_problemTable->horizontalHeader()->setStretchLastSection(true);
    m_problemTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_problemTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_problemTable->setAlternatingRowColors(true);
    m_problemTable->setMaximumHeight(150);
    connect(m_problemTable, &QTableWidget::cellClicked,
            this, &RecoveryWorkflowWidget::onProblemTableClicked);
    
    problemLayout->addWidget(m_problemTable);
    
    m_problemSummaryLabel = new QLabel("No problems detected.", this);
    m_problemSummaryLabel->setStyleSheet("color: green; font-weight: bold;");
    problemLayout->addWidget(m_problemSummaryLabel);
    
    m_mainLayout->addWidget(m_problemGroup);
    
    // === Control Section ===
    m_controlGroup = new QGroupBox("⚙️ Recovery Control", this);
    QHBoxLayout* controlLayout = new QHBoxLayout(m_controlGroup);
    
    m_applyButton = new QPushButton("✓ Apply Recommendations", this);
    m_applyButton->setToolTip("Apply suggested recovery settings for all problematic tracks");
    connect(m_applyButton, &QPushButton::clicked, this, &RecoveryWorkflowWidget::onApplyClicked);
    
    m_skipButton = new QPushButton("⏭ Skip Recovery", this);
    m_skipButton->setToolTip("Skip recovery and accept current results");
    connect(m_skipButton, &QPushButton::clicked, this, &RecoveryWorkflowWidget::onSkipClicked);
    
    m_customButton = new QPushButton("🔧 Custom...", this);
    m_customButton->setToolTip("Configure custom recovery settings");
    connect(m_customButton, &QPushButton::clicked, this, &RecoveryWorkflowWidget::onCustomClicked);
    
    m_startButton = new QPushButton("▶ Start Recovery", this);
    m_startButton->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;");
    connect(m_startButton, &QPushButton::clicked, this, &RecoveryWorkflowWidget::onStartClicked);
    
    m_cancelButton = new QPushButton("✕ Cancel", this);
    m_cancelButton->setEnabled(false);
    connect(m_cancelButton, &QPushButton::clicked, this, &RecoveryWorkflowWidget::onCancelClicked);
    
    controlLayout->addWidget(m_applyButton);
    controlLayout->addWidget(m_skipButton);
    controlLayout->addWidget(m_customButton);
    controlLayout->addStretch();
    controlLayout->addWidget(m_startButton);
    controlLayout->addWidget(m_cancelButton);
    
    m_mainLayout->addWidget(m_controlGroup);
    
    // === Progress Section ===
    m_progressGroup = new QGroupBox("📊 Progress", this);
    QGridLayout* progressLayout = new QGridLayout(m_progressGroup);
    
    m_currentTrackLabel = new QLabel("Track: -", this);
    m_currentPassLabel = new QLabel("Pass: -/-", this);
    
    m_passProgress = new QProgressBar(this);
    m_passProgress->setTextVisible(true);
    m_passProgress->setFormat("Pass: %p%");
    
    m_overallProgress = new QProgressBar(this);
    m_overallProgress->setTextVisible(true);
    m_overallProgress->setFormat("Overall: %p%");
    m_overallProgress->setStyleSheet(
        "QProgressBar { border: 1px solid grey; border-radius: 3px; text-align: center; }"
        "QProgressBar::chunk { background-color: #4CAF50; }");
    
    progressLayout->addWidget(m_currentTrackLabel, 0, 0);
    progressLayout->addWidget(m_currentPassLabel, 0, 1);
    progressLayout->addWidget(new QLabel("Pass Progress:"), 1, 0);
    progressLayout->addWidget(m_passProgress, 1, 1);
    progressLayout->addWidget(new QLabel("Overall Progress:"), 2, 0);
    progressLayout->addWidget(m_overallProgress, 2, 1);
    
    m_mainLayout->addWidget(m_progressGroup);
    
    // === Pass Results Section ===
    m_passGroup = new QGroupBox("📋 Pass Results", this);
    QVBoxLayout* passLayout = new QVBoxLayout(m_passGroup);
    
    m_passTable = new QTableWidget(this);
    m_passTable->setColumnCount(6);
    m_passTable->setHorizontalHeaderLabels({
        "Pass", "Sectors", "Confidence", "CRC", "Timing Var", "Status"
    });
    m_passTable->horizontalHeader()->setStretchLastSection(true);
    m_passTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_passTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_passTable->setAlternatingRowColors(true);
    m_passTable->setMaximumHeight(120);
    connect(m_passTable, &QTableWidget::cellClicked,
            this, &RecoveryWorkflowWidget::onPassTableClicked);
    
    passLayout->addWidget(m_passTable);
    
    m_mainLayout->addWidget(m_passGroup);
    
    // === Statistics Section ===
    m_statsGroup = new QGroupBox("📈 Statistics", this);
    QGridLayout* statsLayout = new QGridLayout(m_statsGroup);
    
    m_totalTracksLabel = new QLabel("Total: 0", this);
    m_goodTracksLabel = new QLabel("Good: 0", this);
    m_goodTracksLabel->setStyleSheet("color: green;");
    m_recoveredTracksLabel = new QLabel("Recovered: 0", this);
    m_recoveredTracksLabel->setStyleSheet("color: orange;");
    m_failedTracksLabel = new QLabel("Failed: 0", this);
    m_failedTracksLabel->setStyleSheet("color: red;");
    m_avgConfidenceLabel = new QLabel("Avg Confidence: 0%", this);
    
    m_confidenceBar = new QProgressBar(this);
    m_confidenceBar->setRange(0, 100);
    m_confidenceBar->setTextVisible(true);
    m_confidenceBar->setFormat("%p%");
    
    statsLayout->addWidget(m_totalTracksLabel, 0, 0);
    statsLayout->addWidget(m_goodTracksLabel, 0, 1);
    statsLayout->addWidget(m_recoveredTracksLabel, 0, 2);
    statsLayout->addWidget(m_failedTracksLabel, 0, 3);
    statsLayout->addWidget(m_avgConfidenceLabel, 1, 0, 1, 2);
    statsLayout->addWidget(m_confidenceBar, 1, 2, 1, 2);
    
    m_mainLayout->addWidget(m_statsGroup);
    
    setLayout(m_mainLayout);
}

// ============================================================================
// Problem Management
// ============================================================================

void RecoveryWorkflowWidget::setProblems(const std::vector<RecoveryProblem>& problems)
{
    m_problems = problems;
    updateProblemTable();
    updateSummary();
}

void RecoveryWorkflowWidget::clearProblems()
{
    m_problems.clear();
    updateProblemTable();
    updateSummary();
}

void RecoveryWorkflowWidget::updateProblemTable()
{
    m_problemTable->setRowCount(m_problems.size());
    
    for (size_t i = 0; i < m_problems.size(); ++i) {
        const auto& p = m_problems[i];
        
        // Track
        QString trackStr = QString("%1.%2").arg(p.track).arg(p.head);
        if (p.sector >= 0) {
            trackStr += QString(" S%1").arg(p.sector);
        }
        m_problemTable->setItem(i, 0, new QTableWidgetItem(trackStr));
        
        // Problem type
        QTableWidgetItem* typeItem = new QTableWidgetItem(problemTypeToString(p.type));
        typeItem->setForeground(Qt::red);
        m_problemTable->setItem(i, 1, typeItem);
        
        // Suggestion
        m_problemTable->setItem(i, 2, new QTableWidgetItem(suggestionToString(p.suggestion)));
        
        // Estimated time
        m_problemTable->setItem(i, 3, new QTableWidgetItem(QString("+%1s").arg(p.estimatedTime)));
        
        // Success probability
        QTableWidgetItem* probItem = new QTableWidgetItem(QString("%1%").arg(p.successProbability));
        probItem->setForeground(confidenceColor(p.successProbability));
        m_problemTable->setItem(i, 4, probItem);
        
        // Description
        m_problemTable->setItem(i, 5, new QTableWidgetItem(p.description));
    }
    
    m_problemTable->resizeColumnsToContents();
}

void RecoveryWorkflowWidget::updateSummary()
{
    if (m_problems.empty()) {
        m_problemSummaryLabel->setText("✓ No problems detected - disk reads cleanly.");
        m_problemSummaryLabel->setStyleSheet("color: green; font-weight: bold;");
        m_applyButton->setEnabled(false);
        m_startButton->setEnabled(false);
    } else {
        int totalTime = 0;
        for (const auto& p : m_problems) {
            totalTime += p.estimatedTime;
        }
        
        m_problemSummaryLabel->setText(
            QString("⚠ Found %1 problematic track(s). Estimated additional time: ~%2 min %3 sec")
            .arg(m_problems.size())
            .arg(totalTime / 60)
            .arg(totalTime % 60));
        m_problemSummaryLabel->setStyleSheet("color: orange; font-weight: bold;");
        m_applyButton->setEnabled(true);
        m_startButton->setEnabled(true);
    }
}

// ============================================================================
// Recovery Control
// ============================================================================

void RecoveryWorkflowWidget::setRecoveryEnabled(bool enabled)
{
    m_applyButton->setEnabled(enabled && !m_problems.empty());
    m_skipButton->setEnabled(enabled);
    m_customButton->setEnabled(enabled);
    m_startButton->setEnabled(enabled && !m_problems.empty());
}

void RecoveryWorkflowWidget::setMaxPasses(int passes)
{
    m_maxPasses = passes;
}

void RecoveryWorkflowWidget::setMinConfidence(int confidence)
{
    m_minConfidence = confidence;
}

// ============================================================================
// Progress
// ============================================================================

void RecoveryWorkflowWidget::setCurrentTrack(int track, int head)
{
    m_currentTrack = track;
    m_currentHead = head;
    m_currentTrackLabel->setText(QString("Track: %1.%2").arg(track).arg(head));
}

void RecoveryWorkflowWidget::setCurrentPass(int pass, int totalPasses)
{
    m_currentPass = pass;
    m_totalPasses = totalPasses;
    m_currentPassLabel->setText(QString("Pass: %1/%2").arg(pass).arg(totalPasses));
}

void RecoveryWorkflowWidget::setPassProgress(int percent)
{
    m_passProgress->setValue(percent);
}

void RecoveryWorkflowWidget::setOverallProgress(int percent)
{
    m_overallProgress->setValue(percent);
}

// ============================================================================
// Results
// ============================================================================

void RecoveryWorkflowWidget::addPassResult(int track, int head, const RecoveryPassResult& result)
{
    // Find or create track result
    RecoveryTrackResult* trackResult = nullptr;
    for (auto& tr : m_results) {
        if (tr.track == track && tr.head == head) {
            trackResult = &tr;
            break;
        }
    }
    
    if (!trackResult) {
        RecoveryTrackResult newResult;
        newResult.track = track;
        newResult.head = head;
        newResult.finalConfidence = 0;
        newResult.weakBits = 0;
        newResult.recovered = false;
        m_results.push_back(newResult);
        trackResult = &m_results.back();
    }
    
    trackResult->passes.push_back(result);
    
    // Update pass table if this is current track
    if (track == m_currentTrack && head == m_currentHead) {
        updatePassTable();
    }
}

void RecoveryWorkflowWidget::setTrackResult(int track, int head, const RecoveryTrackResult& result)
{
    // Find and update or add
    bool found = false;
    for (auto& tr : m_results) {
        if (tr.track == track && tr.head == head) {
            tr = result;
            found = true;
            break;
        }
    }
    
    if (!found) {
        m_results.push_back(result);
    }
    
    if (track == m_currentTrack && head == m_currentHead) {
        updatePassTable();
    }
}

void RecoveryWorkflowWidget::clearResults()
{
    m_results.clear();
    m_passTable->setRowCount(0);
}

void RecoveryWorkflowWidget::updatePassTable()
{
    // Find current track results
    const RecoveryTrackResult* trackResult = nullptr;
    for (const auto& tr : m_results) {
        if (tr.track == m_currentTrack && tr.head == m_currentHead) {
            trackResult = &tr;
            break;
        }
    }
    
    if (!trackResult) {
        m_passTable->setRowCount(0);
        return;
    }
    
    m_passTable->setRowCount(trackResult->passes.size());
    
    for (size_t i = 0; i < trackResult->passes.size(); ++i) {
        const auto& pass = trackResult->passes[i];
        
        // Pass number
        m_passTable->setItem(i, 0, new QTableWidgetItem(QString::number(pass.passNumber)));
        
        // Sectors
        m_passTable->setItem(i, 1, new QTableWidgetItem(
            QString("%1/%2").arg(pass.goodSectors).arg(pass.totalSectors)));
        
        // Confidence
        QTableWidgetItem* confItem = new QTableWidgetItem(QString("%1%").arg(pass.confidence));
        confItem->setForeground(confidenceColor(pass.confidence));
        m_passTable->setItem(i, 2, confItem);
        
        // CRC
        QTableWidgetItem* crcItem = new QTableWidgetItem(pass.crcOk ? "✓ OK" : "✕ Error");
        crcItem->setForeground(pass.crcOk ? Qt::green : Qt::red);
        m_passTable->setItem(i, 3, crcItem);
        
        // Timing variance
        m_passTable->setItem(i, 4, new QTableWidgetItem(
            QString::number(pass.timingVariance, 'f', 3)));
        
        // Status
        QString status;
        if (pass.crcOk && pass.confidence >= m_minConfidence) {
            status = "✓ Good";
        } else if (pass.confidence >= 50) {
            status = "⚠ Usable";
        } else {
            status = "✕ Poor";
        }
        m_passTable->setItem(i, 5, new QTableWidgetItem(status));
    }
    
    m_passTable->resizeColumnsToContents();
}

// ============================================================================
// Statistics
// ============================================================================

void RecoveryWorkflowWidget::updateStatistics(int totalTracks, int goodTracks,
                                              int recoveredTracks, int failedTracks,
                                              double avgConfidence)
{
    m_totalTracksLabel->setText(QString("Total: %1").arg(totalTracks));
    m_goodTracksLabel->setText(QString("Good: %1").arg(goodTracks));
    m_recoveredTracksLabel->setText(QString("Recovered: %1").arg(recoveredTracks));
    m_failedTracksLabel->setText(QString("Failed: %1").arg(failedTracks));
    m_avgConfidenceLabel->setText(QString("Avg Confidence: %1%").arg(avgConfidence, 0, 'f', 1));
    
    m_confidenceBar->setValue(static_cast<int>(avgConfidence));
    
    // Color the confidence bar based on value
    QString color = avgConfidence >= 90 ? "#4CAF50" :
                   (avgConfidence >= 70 ? "#FFC107" : "#F44336");
    m_confidenceBar->setStyleSheet(
        QString("QProgressBar { border: 1px solid grey; border-radius: 3px; text-align: center; }"
                "QProgressBar::chunk { background-color: %1; }").arg(color));
}

// ============================================================================
// Slots
// ============================================================================

void RecoveryWorkflowWidget::onProblemTableClicked(int row, int column)
{
    Q_UNUSED(column);
    if (row >= 0 && row < static_cast<int>(m_problems.size())) {
        emit problemSelected(row);
        emit trackSelected(m_problems[row].track, m_problems[row].head);
    }
}

void RecoveryWorkflowWidget::onPassTableClicked(int row, int column)
{
    Q_UNUSED(row);
    Q_UNUSED(column);
    // Could show detailed pass info
}

void RecoveryWorkflowWidget::onStartClicked()
{
    m_isRunning = true;
    m_startButton->setEnabled(false);
    m_cancelButton->setEnabled(true);
    m_applyButton->setEnabled(false);
    m_skipButton->setEnabled(false);
    m_customButton->setEnabled(false);
    emit startRecoveryClicked();
}

void RecoveryWorkflowWidget::onCancelClicked()
{
    m_isRunning = false;
    m_startButton->setEnabled(true);
    m_cancelButton->setEnabled(false);
    m_applyButton->setEnabled(!m_problems.empty());
    m_skipButton->setEnabled(true);
    m_customButton->setEnabled(true);
    emit cancelRecoveryClicked();
}

void RecoveryWorkflowWidget::onApplyClicked()
{
    emit applyRecommendationsClicked();
}

void RecoveryWorkflowWidget::onSkipClicked()
{
    emit skipRecoveryClicked();
}

void RecoveryWorkflowWidget::onCustomClicked()
{
    emit customSettingsClicked();
}

// ============================================================================
// Helper Functions
// ============================================================================

QString RecoveryWorkflowWidget::problemTypeToString(RecoveryProblemType type) const
{
    switch (type) {
        case RecoveryProblemType::CRC_ERROR:      return "CRC Error";
        case RecoveryProblemType::MISSING_SECTOR: return "Missing Sector";
        case RecoveryProblemType::WEAK_BITS:      return "Weak Bits";
        case RecoveryProblemType::TIMING_DRIFT:   return "Timing Drift";
        case RecoveryProblemType::PROTECTION:     return "Protection";
        case RecoveryProblemType::HEADER_ERROR:   return "Header Error";
        case RecoveryProblemType::SYNC_ERROR:     return "Sync Error";
        default:                                  return "Unknown";
    }
}

QString RecoveryWorkflowWidget::suggestionToString(RecoverySuggestion suggestion) const
{
    switch (suggestion) {
        case RecoverySuggestion::NONE:             return "None";
        case RecoverySuggestion::MULTI_PASS:       return "Multi-Pass (5x)";
        case RecoverySuggestion::WEIGHTED_VOTING:  return "Weighted Voting";
        case RecoverySuggestion::ADAPTIVE_PLL:     return "Adaptive PLL";
        case RecoverySuggestion::ADJACENT_RECOVERY:return "Adjacent Recovery";
        case RecoverySuggestion::PROTECTION_DB:    return "Use Protection DB";
        case RecoverySuggestion::MANUAL_OVERRIDE:  return "Manual Override";
        default:                                   return "Unknown";
    }
}

QColor RecoveryWorkflowWidget::confidenceColor(int confidence) const
{
    if (confidence >= 90) return QColor(0, 180, 0);      // Green
    if (confidence >= 70) return QColor(200, 150, 0);   // Orange
    if (confidence >= 50) return QColor(200, 100, 0);   // Dark orange
    return QColor(200, 0, 0);                           // Red
}
