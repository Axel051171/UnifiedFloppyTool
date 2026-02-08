/**
 * @file UftFormatDetectionWidget.h
 * @brief Widget for displaying Format Auto-Detection Results
 * 
 * P1-008: Format Auto-Detection GUI Widget
 */

#ifndef UFT_FORMAT_DETECTION_WIDGET_H
#define UFT_FORMAT_DETECTION_WIDGET_H

#include <QWidget>
#include <QTableView>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QListWidget>

class UftFormatDetectionModel;

/**
 * @brief Widget displaying format detection results
 * 
 * Shows:
 * - Best match with confidence bar
 * - All candidates in a table
 * - Warnings/info messages
 * - File info (size, detection time)
 */
class UftFormatDetectionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit UftFormatDetectionWidget(QWidget *parent = nullptr);
    ~UftFormatDetectionWidget();
    
    /**
     * @brief Get the underlying model
     */
    UftFormatDetectionModel* model() const { return m_model; }
    
    /**
     * @brief Get selected format (if any)
     */
    QString selectedFormat() const;
    
    /**
     * @brief Is auto-apply enabled?
     */
    bool autoApply() const { return m_autoApply; }
    void setAutoApply(bool enabled) { m_autoApply = enabled; }

public slots:
    /**
     * @brief Start detection from file
     */
    void detectFile(const QString &path);
    
    /**
     * @brief Clear results
     */
    void clear();
    
    /**
     * @brief Apply selected format (emits formatSelected)
     */
    void applySelection();

signals:
    /**
     * @brief Emitted when user selects/confirms a format
     */
    void formatSelected(const QString &formatId, const QString &formatName, int confidence);
    
    /**
     * @brief Emitted when detection starts
     */
    void detectionStarted();
    
    /**
     * @brief Emitted when detection completes
     */
    void detectionCompleted(bool success);

private slots:
    void onResultsChanged();
    void onDetectionFinished(bool success);
    void onTableSelectionChanged();
    void onTableDoubleClicked(const QModelIndex &index);

private:
    void setupUi();
    void updateBestMatch();
    void updateWarnings();
    void updateFileInfo();
    
    UftFormatDetectionModel *m_model;
    
    /* Best match display */
    QGroupBox *m_bestMatchGroup;
    QLabel *m_bestFormatLabel;
    QLabel *m_bestNameLabel;
    QProgressBar *m_confidenceBar;
    QLabel *m_confidenceLabel;
    
    /* Candidates table */
    QGroupBox *m_candidatesGroup;
    QTableView *m_candidatesTable;
    
    /* Warnings */
    QGroupBox *m_warningsGroup;
    QListWidget *m_warningsList;
    
    /* Info */
    QLabel *m_fileInfoLabel;
    
    /* Actions */
    QPushButton *m_applyButton;
    QPushButton *m_clearButton;
    
    bool m_autoApply = false;
};

#endif /* UFT_FORMAT_DETECTION_WIDGET_H */
