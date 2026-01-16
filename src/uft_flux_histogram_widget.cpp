/**
 * @file uft_flux_histogram_widget.cpp
 * @brief GUI Flux Histogram Visualization Widget Implementation
 */

#include "uft_flux_histogram_widget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QToolTip>
#include <QFile>
#include <QTextStream>

#include <cmath>
#include <algorithm>

/*===========================================================================
 * UftFluxHistogramWidget
 *===========================================================================*/

UftFluxHistogramWidget::UftFluxHistogramWidget(QWidget *parent)
    : QWidget(parent)
    , m_binWidth(10)  // 10ns per bin default
    , m_maxCount(0)
    , m_totalSamples(0)
    , m_detectedEncoding(ENC_AUTO)
    , m_cellTime(0.0)
    , m_displayMode(MODE_LINEAR)
    , m_showPeaks(true)
    , m_showGrid(true)
    , m_visibleMin(0)
    , m_visibleMax(10000)  // 10 µs default
    , m_barColor(0x2196F3)      // Material Blue
    , m_peakColor(0xF44336)     // Material Red
    , m_gridColor(0x424242)     // Dark Gray
    , m_textColor(0xFFFFFF)     // White
    , m_backgroundColor(0x1E1E1E) // Dark background
    , m_hoveredBin(-1)
{
    setMinimumSize(400, 200);
    setMouseTracking(true);
    
    // Initialize bins
    int numBins = (m_visibleMax - m_visibleMin) / m_binWidth + 1;
    m_bins.resize(numBins, 0);
}

UftFluxHistogramWidget::~UftFluxHistogramWidget()
{
}

void UftFluxHistogramWidget::setFluxData(const double *flux_times, size_t count)
{
    buildHistogram(flux_times, count);
    detectPeaks();
    detectEncoding();
    update();
    emit histogramUpdated();
}

void UftFluxHistogramWidget::setFluxData(const QVector<double> &flux_times)
{
    setFluxData(flux_times.data(), flux_times.size());
}

void UftFluxHistogramWidget::setFluxDataRaw(const uint16_t *samples, size_t count,
                                            double sample_rate)
{
    // Convert raw samples to nanoseconds
    QVector<double> flux_times(count);
    double ns_per_sample = 1e9 / sample_rate;
    
    for (size_t i = 0; i < count; i++) {
        flux_times[i] = samples[i] * ns_per_sample;
    }
    
    setFluxData(flux_times);
}

uint32_t UftFluxHistogramWidget::detectedDataRate() const
{
    if (m_cellTime <= 0.0) return 0;
    return static_cast<uint32_t>(1e9 / m_cellTime);
}

void UftFluxHistogramWidget::clear()
{
    m_bins.fill(0);
    m_peaks.clear();
    m_maxCount = 0;
    m_totalSamples = 0;
    m_detectedEncoding = ENC_AUTO;
    m_cellTime = 0.0;
    update();
    emit histogramUpdated();
}

void UftFluxHistogramWidget::setDisplayMode(DisplayMode mode)
{
    m_displayMode = mode;
    update();
}

void UftFluxHistogramWidget::setEncodingHint(EncodingType enc)
{
    m_detectedEncoding = enc;
    update();
}

void UftFluxHistogramWidget::setBinWidth(int nsPerBin)
{
    if (nsPerBin < 1) nsPerBin = 1;
    if (nsPerBin > 1000) nsPerBin = 1000;
    m_binWidth = nsPerBin;
    
    // Resize bins
    int numBins = (m_visibleMax - m_visibleMin) / m_binWidth + 1;
    m_bins.resize(numBins, 0);
    m_bins.fill(0);
    
    update();
}

void UftFluxHistogramWidget::setVisibleRange(int minNs, int maxNs)
{
    m_visibleMin = qMax(0, minNs);
    m_visibleMax = qMax(m_visibleMin + 100, maxNs);
    
    // Resize bins
    int numBins = (m_visibleMax - m_visibleMin) / m_binWidth + 1;
    m_bins.resize(numBins, 0);
    
    update();
}

void UftFluxHistogramWidget::autoFitRange()
{
    // Find first and last non-zero bins
    int firstNonZero = -1;
    int lastNonZero = -1;
    
    for (int i = 0; i < m_bins.size(); i++) {
        if (m_bins[i] > 0) {
            if (firstNonZero < 0) firstNonZero = i;
            lastNonZero = i;
        }
    }
    
    if (firstNonZero >= 0 && lastNonZero >= 0) {
        // Add 10% margin
        int range = (lastNonZero - firstNonZero) * m_binWidth;
        int margin = range / 10;
        
        m_visibleMin = qMax(0, firstNonZero * m_binWidth - margin);
        m_visibleMax = (lastNonZero + 1) * m_binWidth + margin;
    }
    
    update();
}

void UftFluxHistogramWidget::setShowPeaks(bool show)
{
    m_showPeaks = show;
    update();
}

void UftFluxHistogramWidget::setShowGrid(bool show)
{
    m_showGrid = show;
    update();
}

void UftFluxHistogramWidget::exportImage(const QString &filename)
{
    QPixmap pixmap(size());
    render(&pixmap);
    pixmap.save(filename);
}

void UftFluxHistogramWidget::exportCSV(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }
    
    QTextStream out(&file);
    out << "Time (ns),Count\n";
    
    for (int i = 0; i < m_bins.size(); i++) {
        int ns = m_visibleMin + i * m_binWidth;
        out << ns << "," << m_bins[i] << "\n";
    }
    
    file.close();
}

void UftFluxHistogramWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Background
    painter.fillRect(rect(), m_backgroundColor);
    
    // Drawing area (leave margins for labels)
    QRect drawRect = rect().adjusted(60, 20, -20, -40);
    
    // Grid
    if (m_showGrid) {
        drawGrid(painter, drawRect);
    }
    
    // Histogram bars
    drawHistogram(painter, drawRect);
    
    // Peaks
    if (m_showPeaks && !m_peaks.isEmpty()) {
        drawPeaks(painter, drawRect);
    }
    
    // Statistics
    drawStatistics(painter, rect());
}

void UftFluxHistogramWidget::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
    update();
}

void UftFluxHistogramWidget::mousePressEvent(QMouseEvent *event)
{
    QRect drawRect = rect().adjusted(60, 20, -20, -40);
    int ns = xToNs(event->pos().x(), drawRect);
    int bin = (ns - m_visibleMin) / m_binWidth;
    
    if (bin >= 0 && bin < m_bins.size()) {
        emit binClicked(bin, ns, m_bins[bin]);
    }
}

void UftFluxHistogramWidget::mouseMoveEvent(QMouseEvent *event)
{
    QRect drawRect = rect().adjusted(60, 20, -20, -40);
    int ns = xToNs(event->pos().x(), drawRect);
    int bin = (ns - m_visibleMin) / m_binWidth;
    
    if (bin >= 0 && bin < m_bins.size() && bin != m_hoveredBin) {
        m_hoveredBin = bin;
        
        // Show tooltip
        QString tip = QString("%1 ns: %2 samples")
            .arg(ns)
            .arg(m_bins[bin]);
        QToolTip::showText(event->globalPosition().toPoint(), tip, this);
        
        update();
    }
}

void UftFluxHistogramWidget::wheelEvent(QWheelEvent *event)
{
    int delta = event->angleDelta().y();
    int center = (m_visibleMin + m_visibleMax) / 2;
    int range = m_visibleMax - m_visibleMin;
    
    if (delta > 0) {
        // Zoom in
        range = range * 3 / 4;
    } else {
        // Zoom out
        range = range * 4 / 3;
    }
    
    range = qMax(100, qMin(100000, range));
    
    m_visibleMin = center - range / 2;
    m_visibleMax = center + range / 2;
    
    if (m_visibleMin < 0) {
        m_visibleMax -= m_visibleMin;
        m_visibleMin = 0;
    }
    
    update();
}

void UftFluxHistogramWidget::buildHistogram(const double *flux_times, size_t count)
{
    m_bins.fill(0);
    m_maxCount = 0;
    m_totalSamples = count;
    
    for (size_t i = 0; i < count; i++) {
        int ns = static_cast<int>(flux_times[i]);
        int bin = (ns - m_visibleMin) / m_binWidth;
        
        if (bin >= 0 && bin < m_bins.size()) {
            m_bins[bin]++;
            if (m_bins[bin] > m_maxCount) {
                m_maxCount = m_bins[bin];
            }
        }
    }
}

void UftFluxHistogramWidget::detectPeaks()
{
    m_peaks.clear();
    
    if (m_maxCount == 0) return;
    
    // Simple peak detection: find local maxima above threshold
    uint32_t threshold = m_maxCount / 20;  // 5% of max
    int minDistance = 500 / m_binWidth;    // 500ns minimum between peaks
    
    for (int i = 2; i < m_bins.size() - 2; i++) {
        uint32_t val = m_bins[i];
        
        if (val < threshold) continue;
        
        // Check if local maximum
        if (val > m_bins[i-1] && val > m_bins[i+1] &&
            val > m_bins[i-2] && val > m_bins[i+2]) {
            
            // Check distance from previous peak
            bool tooClose = false;
            for (const Peak &p : m_peaks) {
                if (abs(p.position - i) < minDistance) {
                    tooClose = true;
                    break;
                }
            }
            
            if (!tooClose) {
                // Compute weighted center
                double weightedSum = 0.0;
                uint32_t totalWeight = 0;
                for (int j = i - 2; j <= i + 2; j++) {
                    weightedSum += j * m_bins[j];
                    totalWeight += m_bins[j];
                }
                double center = (totalWeight > 0) ? (weightedSum / totalWeight) : i;
                
                Peak peak;
                peak.position = i;
                peak.count = val;
                peak.center = center;
                peak.label = "";
                
                m_peaks.append(peak);
            }
        }
    }
    
    // Sort peaks by position
    std::sort(m_peaks.begin(), m_peaks.end(), 
              [](const Peak &a, const Peak &b) { return a.position < b.position; });
}

void UftFluxHistogramWidget::detectEncoding()
{
    if (m_peaks.size() < 2) {
        m_detectedEncoding = ENC_AUTO;
        m_cellTime = 0.0;
        return;
    }
    
    // Get peak positions in ns
    QVector<double> peakNs;
    for (const Peak &p : m_peaks) {
        peakNs.append(m_visibleMin + p.center * m_binWidth);
    }
    
    // MFM detection: 3 peaks at 1T, 1.5T, 2T ratio
    if (m_peaks.size() >= 3) {
        double p1 = peakNs[0];
        double p2 = peakNs[1];
        double p3 = peakNs[2];
        
        // Check for MFM ratio (1 : 1.5 : 2)
        double ratio1 = p2 / p1;
        double ratio2 = p3 / p1;
        
        if (ratio1 > 1.3 && ratio1 < 1.7 &&    // 1.5 ± 0.2
            ratio2 > 1.8 && ratio2 < 2.2) {    // 2.0 ± 0.2
            
            m_detectedEncoding = ENC_MFM;
            m_cellTime = p1;  // First peak is 1T
            
            // Label peaks
            m_peaks[0].label = "1T";
            m_peaks[1].label = "1.5T";
            m_peaks[2].label = "2T";
            
            emit encodingDetected(m_detectedEncoding, m_cellTime);
            return;
        }
    }
    
    // FM detection: 2 peaks at 1T, 2T ratio
    if (m_peaks.size() >= 2) {
        double p1 = peakNs[0];
        double p2 = peakNs[1];
        
        double ratio = p2 / p1;
        
        if (ratio > 1.8 && ratio < 2.2) {  // 2.0 ± 0.2
            m_detectedEncoding = ENC_FM;
            m_cellTime = p1;
            
            m_peaks[0].label = "1T";
            m_peaks[1].label = "2T";
            
            emit encodingDetected(m_detectedEncoding, m_cellTime);
            return;
        }
    }
    
    // GCR detection: typically has different pattern
    // For now, assume first peak is base timing
    m_detectedEncoding = ENC_AUTO;
    m_cellTime = peakNs.isEmpty() ? 0.0 : peakNs[0];
}

void UftFluxHistogramWidget::drawHistogram(QPainter &painter, const QRect &rect)
{
    if (m_maxCount == 0) return;
    
    int barWidth = qMax(1, rect.width() / m_bins.size());
    
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_barColor);
    
    for (int i = 0; i < m_bins.size(); i++) {
        if (m_bins[i] == 0) continue;
        
        int x = rect.left() + i * rect.width() / m_bins.size();
        double scaledCount = scaleCount(m_bins[i]);
        int barHeight = static_cast<int>(scaledCount * rect.height());
        
        QRect barRect(x, rect.bottom() - barHeight, barWidth, barHeight);
        
        // Highlight hovered bin
        if (i == m_hoveredBin) {
            painter.setBrush(m_barColor.lighter(130));
        } else {
            painter.setBrush(m_barColor);
        }
        
        painter.drawRect(barRect);
    }
}

void UftFluxHistogramWidget::drawPeaks(QPainter &painter, const QRect &rect)
{
    painter.setPen(QPen(m_peakColor, 2));
    
    QFont font = painter.font();
    font.setPointSize(9);
    painter.setFont(font);
    
    for (const Peak &peak : m_peaks) {
        int x = rect.left() + static_cast<int>(peak.center) * rect.width() / m_bins.size();
        
        // Vertical line at peak
        painter.drawLine(x, rect.top(), x, rect.bottom());
        
        // Label
        if (!peak.label.isEmpty()) {
            int ns = m_visibleMin + static_cast<int>(peak.center * m_binWidth);
            QString text = QString("%1\n%2 ns").arg(peak.label).arg(ns);
            
            painter.setPen(m_textColor);
            painter.drawText(x - 20, rect.top() + 5, 40, 30, 
                            Qt::AlignCenter, text);
            painter.setPen(QPen(m_peakColor, 2));
        }
    }
}

void UftFluxHistogramWidget::drawGrid(QPainter &painter, const QRect &rect)
{
    painter.setPen(QPen(m_gridColor, 1, Qt::DotLine));
    
    // Vertical grid lines (time)
    int step = 1000;  // 1 µs steps
    if (m_visibleMax - m_visibleMin > 20000) step = 2000;
    if (m_visibleMax - m_visibleMin > 50000) step = 5000;
    if (m_visibleMax - m_visibleMin < 5000) step = 500;
    
    for (int ns = m_visibleMin; ns <= m_visibleMax; ns += step) {
        int x = nsToX(ns, rect);
        painter.drawLine(x, rect.top(), x, rect.bottom());
        
        // Label
        painter.setPen(m_textColor);
        QString label;
        if (ns >= 1000) {
            label = QString("%1 µs").arg(ns / 1000.0, 0, 'f', 1);
        } else {
            label = QString("%1 ns").arg(ns);
        }
        painter.drawText(x - 25, rect.bottom() + 5, 50, 15, 
                        Qt::AlignCenter, label);
        painter.setPen(QPen(m_gridColor, 1, Qt::DotLine));
    }
    
    // Horizontal grid lines (count)
    for (int i = 1; i <= 4; i++) {
        int y = rect.bottom() - i * rect.height() / 4;
        painter.drawLine(rect.left(), y, rect.right(), y);
        
        // Label
        painter.setPen(m_textColor);
        uint32_t count = static_cast<uint32_t>(m_maxCount * i / 4);
        painter.drawText(rect.left() - 55, y - 7, 50, 15, 
                        Qt::AlignRight | Qt::AlignVCenter, 
                        QString::number(count));
        painter.setPen(QPen(m_gridColor, 1, Qt::DotLine));
    }
}

void UftFluxHistogramWidget::drawStatistics(QPainter &painter, const QRect &rect)
{
    // Draw encoding info in top-right corner
    painter.setPen(m_textColor);
    
    QFont font = painter.font();
    font.setPointSize(10);
    font.setBold(true);
    painter.setFont(font);
    
    QString encStr;
    switch (m_detectedEncoding) {
        case ENC_MFM:       encStr = "MFM"; break;
        case ENC_FM:        encStr = "FM"; break;
        case ENC_GCR_C64:   encStr = "GCR (C64)"; break;
        case ENC_GCR_APPLE: encStr = "GCR (Apple)"; break;
        case ENC_M2FM:      encStr = "M2FM"; break;
        case ENC_AMIGA:     encStr = "Amiga MFM"; break;
        default:            encStr = "Auto"; break;
    }
    
    QString info = QString("Encoding: %1").arg(encStr);
    if (m_cellTime > 0) {
        info += QString(" | Cell: %1 ns | Rate: %2 kbps")
            .arg(m_cellTime, 0, 'f', 0)
            .arg(detectedDataRate() / 1000);
    }
    info += QString(" | Samples: %1").arg(m_totalSamples);
    
    painter.drawText(rect.adjusted(65, 2, -5, -rect.height() + 18), 
                    Qt::AlignRight, info);
}

int UftFluxHistogramWidget::nsToX(int ns, const QRect &rect) const
{
    return rect.left() + (ns - m_visibleMin) * rect.width() / (m_visibleMax - m_visibleMin);
}

int UftFluxHistogramWidget::xToNs(int x, const QRect &rect) const
{
    return m_visibleMin + (x - rect.left()) * (m_visibleMax - m_visibleMin) / rect.width();
}

double UftFluxHistogramWidget::scaleCount(uint32_t count) const
{
    if (m_maxCount == 0) return 0.0;
    
    double normalized = static_cast<double>(count) / m_maxCount;
    
    switch (m_displayMode) {
        case MODE_LOG:
            return log10(1.0 + normalized * 9.0);  // 0-1 mapped to log scale
        case MODE_SQRT:
            return sqrt(normalized);
        case MODE_LINEAR:
        default:
            return normalized;
    }
}

/*===========================================================================
 * UftFluxHistogramPanel
 *===========================================================================*/

UftFluxHistogramPanel::UftFluxHistogramPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

UftFluxHistogramPanel::~UftFluxHistogramPanel()
{
}

void UftFluxHistogramPanel::loadFromFile(const QString &filename)
{
    // TODO: Load flux data from SCP, RAW, or other flux formats
    Q_UNUSED(filename);
}

void UftFluxHistogramPanel::loadFromTrack(int track, int head)
{
    // TODO: Load from current disk image
    Q_UNUSED(track);
    Q_UNUSED(head);
}

void UftFluxHistogramPanel::onTrackChanged(int track, int head)
{
    loadFromTrack(track, head);
}

void UftFluxHistogramPanel::onEncodingChanged(int index)
{
    m_histogram->setEncodingHint(
        static_cast<UftFluxHistogramWidget::EncodingType>(index));
}

void UftFluxHistogramPanel::onModeChanged(int index)
{
    m_histogram->setDisplayMode(
        static_cast<UftFluxHistogramWidget::DisplayMode>(index));
}

void UftFluxHistogramPanel::onBinWidthChanged(int value)
{
    m_histogram->setBinWidth(value);
}

void UftFluxHistogramPanel::onExportImage()
{
    QString filename = QFileDialog::getSaveFileName(this, 
        tr("Export Histogram Image"), QString(),
        tr("PNG Images (*.png);;All Files (*)"));
    
    if (!filename.isEmpty()) {
        m_histogram->exportImage(filename);
    }
}

void UftFluxHistogramPanel::onExportCSV()
{
    QString filename = QFileDialog::getSaveFileName(this,
        tr("Export Histogram Data"), QString(),
        tr("CSV Files (*.csv);;All Files (*)"));
    
    if (!filename.isEmpty()) {
        m_histogram->exportCSV(filename);
    }
}

void UftFluxHistogramPanel::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Toolbar
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    
    // Track selection
    toolbarLayout->addWidget(new QLabel(tr("Track:")));
    m_trackSpin = new QSpinBox();
    m_trackSpin->setRange(0, 83);
    toolbarLayout->addWidget(m_trackSpin);
    
    toolbarLayout->addWidget(new QLabel(tr("Head:")));
    m_headSpin = new QSpinBox();
    m_headSpin->setRange(0, 1);
    toolbarLayout->addWidget(m_headSpin);
    
    toolbarLayout->addSpacing(20);
    
    // Encoding hint
    toolbarLayout->addWidget(new QLabel(tr("Encoding:")));
    m_encodingCombo = new QComboBox();
    m_encodingCombo->addItem(tr("Auto"), UftFluxHistogramWidget::ENC_AUTO);
    m_encodingCombo->addItem(tr("MFM"), UftFluxHistogramWidget::ENC_MFM);
    m_encodingCombo->addItem(tr("FM"), UftFluxHistogramWidget::ENC_FM);
    m_encodingCombo->addItem(tr("GCR (C64)"), UftFluxHistogramWidget::ENC_GCR_C64);
    m_encodingCombo->addItem(tr("GCR (Apple)"), UftFluxHistogramWidget::ENC_GCR_APPLE);
    m_encodingCombo->addItem(tr("M2FM"), UftFluxHistogramWidget::ENC_M2FM);
    m_encodingCombo->addItem(tr("Amiga MFM"), UftFluxHistogramWidget::ENC_AMIGA);
    toolbarLayout->addWidget(m_encodingCombo);
    
    // Display mode
    toolbarLayout->addWidget(new QLabel(tr("Scale:")));
    m_modeCombo = new QComboBox();
    m_modeCombo->addItem(tr("Linear"), UftFluxHistogramWidget::MODE_LINEAR);
    m_modeCombo->addItem(tr("Logarithmic"), UftFluxHistogramWidget::MODE_LOG);
    m_modeCombo->addItem(tr("Square Root"), UftFluxHistogramWidget::MODE_SQRT);
    toolbarLayout->addWidget(m_modeCombo);
    
    // Bin width
    toolbarLayout->addWidget(new QLabel(tr("Bin:")));
    m_binWidthSpin = new QSpinBox();
    m_binWidthSpin->setRange(1, 100);
    m_binWidthSpin->setValue(10);
    m_binWidthSpin->setSuffix(tr(" ns"));
    toolbarLayout->addWidget(m_binWidthSpin);
    
    toolbarLayout->addStretch();
    
    // Checkboxes
    m_showPeaksCheck = new QCheckBox(tr("Show Peaks"));
    m_showPeaksCheck->setChecked(true);
    toolbarLayout->addWidget(m_showPeaksCheck);
    
    m_showGridCheck = new QCheckBox(tr("Show Grid"));
    m_showGridCheck->setChecked(true);
    toolbarLayout->addWidget(m_showGridCheck);
    
    // Buttons
    m_autoFitBtn = new QPushButton(tr("Auto Fit"));
    toolbarLayout->addWidget(m_autoFitBtn);
    
    m_exportImageBtn = new QPushButton(tr("Export Image"));
    toolbarLayout->addWidget(m_exportImageBtn);
    
    m_exportCSVBtn = new QPushButton(tr("Export CSV"));
    toolbarLayout->addWidget(m_exportCSVBtn);
    
    mainLayout->addLayout(toolbarLayout);
    
    // Histogram widget
    m_histogram = new UftFluxHistogramWidget();
    mainLayout->addWidget(m_histogram, 1);
    
    // Info bar
    QHBoxLayout *infoLayout = new QHBoxLayout();
    
    m_encodingLabel = new QLabel(tr("Encoding: -"));
    infoLayout->addWidget(m_encodingLabel);
    
    m_cellTimeLabel = new QLabel(tr("Cell Time: -"));
    infoLayout->addWidget(m_cellTimeLabel);
    
    m_dataRateLabel = new QLabel(tr("Data Rate: -"));
    infoLayout->addWidget(m_dataRateLabel);
    
    m_sampleCountLabel = new QLabel(tr("Samples: 0"));
    infoLayout->addWidget(m_sampleCountLabel);
    
    infoLayout->addStretch();
    
    mainLayout->addLayout(infoLayout);
    
    // Connections
    connect(m_trackSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            [this](int v) { onTrackChanged(v, m_headSpin->value()); });
    connect(m_headSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            [this](int v) { onTrackChanged(m_trackSpin->value(), v); });
    connect(m_encodingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UftFluxHistogramPanel::onEncodingChanged);
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UftFluxHistogramPanel::onModeChanged);
    connect(m_binWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &UftFluxHistogramPanel::onBinWidthChanged);
    connect(m_showPeaksCheck, &QCheckBox::toggled,
            m_histogram, &UftFluxHistogramWidget::setShowPeaks);
    connect(m_showGridCheck, &QCheckBox::toggled,
            m_histogram, &UftFluxHistogramWidget::setShowGrid);
    connect(m_autoFitBtn, &QPushButton::clicked,
            m_histogram, &UftFluxHistogramWidget::autoFitRange);
    connect(m_exportImageBtn, &QPushButton::clicked,
            this, &UftFluxHistogramPanel::onExportImage);
    connect(m_exportCSVBtn, &QPushButton::clicked,
            this, &UftFluxHistogramPanel::onExportCSV);
    
    connect(m_histogram, &UftFluxHistogramWidget::encodingDetected,
            [this](UftFluxHistogramWidget::EncodingType enc, double cellTime) {
                (void)enc; (void)cellTime;  // Parameters available for future use
                updateStatistics();
            });
    connect(m_histogram, &UftFluxHistogramWidget::histogramUpdated,
            this, &UftFluxHistogramPanel::updateStatistics);
}

void UftFluxHistogramPanel::updateStatistics()
{
    QString encStr;
    switch (m_histogram->detectedEncoding()) {
        case UftFluxHistogramWidget::ENC_MFM:       encStr = "MFM"; break;
        case UftFluxHistogramWidget::ENC_FM:        encStr = "FM"; break;
        case UftFluxHistogramWidget::ENC_GCR_C64:   encStr = "GCR (C64)"; break;
        case UftFluxHistogramWidget::ENC_GCR_APPLE: encStr = "GCR (Apple)"; break;
        case UftFluxHistogramWidget::ENC_M2FM:      encStr = "M2FM"; break;
        case UftFluxHistogramWidget::ENC_AMIGA:     encStr = "Amiga MFM"; break;
        default:                                     encStr = "-"; break;
    }
    
    m_encodingLabel->setText(tr("Encoding: %1").arg(encStr));
    
    double cellTime = m_histogram->detectedCellTime();
    if (cellTime > 0) {
        m_cellTimeLabel->setText(tr("Cell Time: %1 ns").arg(cellTime, 0, 'f', 1));
        m_dataRateLabel->setText(tr("Data Rate: %1 kbps")
            .arg(m_histogram->detectedDataRate() / 1000));
    } else {
        m_cellTimeLabel->setText(tr("Cell Time: -"));
        m_dataRateLabel->setText(tr("Data Rate: -"));
    }
    
    // m_sampleCountLabel->setText(tr("Samples: %1").arg(m_histogram->totalSamples()));
}
