/**
 * @file uft_flux_histogram_widget.h
 * @brief GUI Flux Histogram Visualization Widget
 * 
 * Features inspired by gwhist from qbarnes/gw2dmk:
 * - Real-time flux timing histogram display
 * - Peak detection and marking
 * - MFM/FM/GCR encoding detection
 * - Bit cell timing analysis
 * - Export to image/CSV
 * 
 * @version 1.0
 * @date 2026-01-16
 */

#ifndef UFT_FLUX_HISTOGRAM_WIDGET_H
#define UFT_FLUX_HISTOGRAM_WIDGET_H

#include <QWidget>
#include <QTimer>
#include <QPainter>
#include <QVector>
#include <QColor>

class QLabel;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QPushButton;
class QGroupBox;

/**
 * @brief Flux Histogram Display Widget
 * 
 * Displays flux timing histograms with:
 * - Bar graph visualization
 * - Peak markers
 * - Grid overlay
 * - Statistics panel
 */
class UftFluxHistogramWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Encoding types for visualization
     */
    enum EncodingType {
        ENC_AUTO,       ///< Auto-detect
        ENC_MFM,        ///< MFM (IBM PC, Atari ST, etc.)
        ENC_FM,         ///< FM (Single Density)
        ENC_GCR_C64,    ///< GCR (Commodore 64)
        ENC_GCR_APPLE,  ///< GCR (Apple II)
        ENC_M2FM,       ///< M2FM (Intel, DEC)
        ENC_AMIGA       ///< Amiga MFM
    };

    /**
     * @brief Display mode
     */
    enum DisplayMode {
        MODE_LINEAR,    ///< Linear scale
        MODE_LOG,       ///< Logarithmic scale
        MODE_SQRT       ///< Square root scale
    };

    explicit UftFluxHistogramWidget(QWidget *parent = nullptr);
    ~UftFluxHistogramWidget();

    /**
     * @brief Set flux timing data
     * 
     * @param flux_times  Array of flux transition times (in nanoseconds)
     * @param count       Number of samples
     */
    void setFluxData(const double *flux_times, size_t count);
    void setFluxData(const QVector<double> &flux_times);

    /**
     * @brief Set flux data from raw samples
     * 
     * @param samples     Raw sample data
     * @param count       Number of samples
     * @param sample_rate Sample rate (Hz)
     */
    void setFluxDataRaw(const uint16_t *samples, size_t count, 
                        double sample_rate);

    /**
     * @brief Get detected encoding type
     */
    EncodingType detectedEncoding() const { return m_detectedEncoding; }

    /**
     * @brief Get detected bit cell time in nanoseconds
     */
    double detectedCellTime() const { return m_cellTime; }

    /**
     * @brief Get detected data rate in bits/second
     */
    uint32_t detectedDataRate() const;

    /**
     * @brief Get histogram bin count
     */
    int binCount() const { return m_bins.size(); }

    /**
     * @brief Get maximum count in histogram
     */
    uint32_t maxCount() const { return m_maxCount; }

public slots:
    /**
     * @brief Clear histogram data
     */
    void clear();

    /**
     * @brief Set display mode
     */
    void setDisplayMode(DisplayMode mode);

    /**
     * @brief Set encoding hint
     */
    void setEncodingHint(EncodingType enc);

    /**
     * @brief Set bin width (nanoseconds per bin)
     */
    void setBinWidth(int nsPerBin);

    /**
     * @brief Set visible range
     */
    void setVisibleRange(int minNs, int maxNs);

    /**
     * @brief Auto-fit visible range to data
     */
    void autoFitRange();

    /**
     * @brief Show/hide peaks
     */
    void setShowPeaks(bool show);

    /**
     * @brief Show/hide grid
     */
    void setShowGrid(bool show);

    /**
     * @brief Export to image
     */
    void exportImage(const QString &filename);

    /**
     * @brief Export to CSV
     */
    void exportCSV(const QString &filename);

signals:
    /**
     * @brief Emitted when encoding is detected
     */
    void encodingDetected(EncodingType encoding, double cellTime);

    /**
     * @brief Emitted when histogram is updated
     */
    void histogramUpdated();

    /**
     * @brief Emitted when user clicks on histogram
     */
    void binClicked(int binIndex, int nsValue, uint32_t count);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    // Data
    QVector<uint32_t> m_bins;           ///< Histogram bins
    int m_binWidth;                      ///< Nanoseconds per bin
    uint32_t m_maxCount;                 ///< Maximum bin count
    uint64_t m_totalSamples;             ///< Total sample count

    // Peaks
    struct Peak {
        int position;       ///< Bin position
        uint32_t count;     ///< Count at peak
        double center;      ///< Weighted center (sub-bin)
        QString label;      ///< Label (1T, 1.5T, 2T, etc.)
    };
    QVector<Peak> m_peaks;

    // Detection results
    EncodingType m_detectedEncoding;
    double m_cellTime;                   ///< Bit cell time in ns

    // Display settings
    DisplayMode m_displayMode;
    bool m_showPeaks;
    bool m_showGrid;
    int m_visibleMin;                    ///< Visible range min (ns)
    int m_visibleMax;                    ///< Visible range max (ns)

    // Colors
    QColor m_barColor;
    QColor m_peakColor;
    QColor m_gridColor;
    QColor m_textColor;
    QColor m_backgroundColor;

    // Mouse interaction
    int m_hoveredBin;

    // Methods
    void buildHistogram(const double *flux_times, size_t count);
    void detectPeaks();
    void detectEncoding();
    void drawHistogram(QPainter &painter, const QRect &rect);
    void drawPeaks(QPainter &painter, const QRect &rect);
    void drawGrid(QPainter &painter, const QRect &rect);
    void drawStatistics(QPainter &painter, const QRect &rect);
    int nsToX(int ns, const QRect &rect) const;
    int xToNs(int x, const QRect &rect) const;
    double scaleCount(uint32_t count) const;
};

/**
 * @brief Flux Histogram Panel with controls
 */
class UftFluxHistogramPanel : public QWidget
{
    Q_OBJECT

public:
    explicit UftFluxHistogramPanel(QWidget *parent = nullptr);
    ~UftFluxHistogramPanel();

    /**
     * @brief Get histogram widget
     */
    UftFluxHistogramWidget* histogramWidget() { return m_histogram; }

    /**
     * @brief Load flux data from file
     */
    void loadFromFile(const QString &filename);

    /**
     * @brief Load flux data from track
     */
    void loadFromTrack(int track, int head);

public slots:
    void onTrackChanged(int track, int head);
    void onEncodingChanged(int index);
    void onModeChanged(int index);
    void onBinWidthChanged(int value);
    void onExportImage();
    void onExportCSV();

private:
    UftFluxHistogramWidget *m_histogram;
    
    // Controls
    QComboBox *m_encodingCombo;
    QComboBox *m_modeCombo;
    QSpinBox *m_binWidthSpin;
    QSpinBox *m_trackSpin;
    QSpinBox *m_headSpin;
    QCheckBox *m_showPeaksCheck;
    QCheckBox *m_showGridCheck;
    QPushButton *m_autoFitBtn;
    QPushButton *m_exportImageBtn;
    QPushButton *m_exportCSVBtn;
    
    // Info labels
    QLabel *m_encodingLabel;
    QLabel *m_cellTimeLabel;
    QLabel *m_dataRateLabel;
    QLabel *m_sampleCountLabel;

    void setupUi();
    void updateStatistics();
};

#endif /* UFT_FLUX_HISTOGRAM_WIDGET_H */
