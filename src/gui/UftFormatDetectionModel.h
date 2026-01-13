/**
 * @file UftFormatDetectionModel.h
 * @brief Qt Model for Format Auto-Detection Results
 * 
 * P1-008: Format Auto-Detection GUI
 * 
 * Provides Qt model for displaying detection results in GUI
 * with confidence scores, warnings, and recommendations.
 */

#ifndef UFT_FORMAT_DETECTION_MODEL_H
#define UFT_FORMAT_DETECTION_MODEL_H

#include <QObject>
#include <QAbstractTableModel>
#include <QString>
#include <QList>
#include <QColor>

/* Forward declare C types */
extern "C" {
    struct uft_detect_result;
    typedef struct uft_detect_result uft_detect_result_t;
}

/**
 * @brief Format candidate for display
 */
struct UftFormatCandidate {
    QString formatId;       /* e.g. "ADF", "D64" */
    QString formatName;     /* e.g. "Amiga Disk File" */
    QString description;    /* Detailed description */
    int confidence;         /* 0-100 */
    QString heuristics;     /* Matched heuristics */
    bool isBest;           /* Is this the best match? */
    
    QColor confidenceColor() const {
        if (confidence >= 80) return QColor(76, 175, 80);    /* Green */
        if (confidence >= 60) return QColor(255, 193, 7);    /* Yellow */
        if (confidence >= 40) return QColor(255, 152, 0);    /* Orange */
        return QColor(244, 67, 54);                          /* Red */
    }
    
    QString confidenceText() const {
        if (confidence >= 80) return "High";
        if (confidence >= 60) return "Medium";
        if (confidence >= 40) return "Low";
        return "Uncertain";
    }
};

/**
 * @brief Warning message
 */
struct UftDetectionWarning {
    QString message;
    int severity;  /* 0=info, 1=warning, 2=error */
    
    QColor color() const {
        switch (severity) {
            case 0: return QColor(33, 150, 243);   /* Blue - info */
            case 1: return QColor(255, 193, 7);    /* Yellow - warning */
            case 2: return QColor(244, 67, 54);    /* Red - error */
            default: return QColor(158, 158, 158); /* Grey */
        }
    }
    
    QString icon() const {
        switch (severity) {
            case 0: return "ℹ";
            case 1: return "⚠";
            case 2: return "✗";
            default: return "•";
        }
    }
};

/**
 * @brief Table model for format detection results
 */
class UftFormatDetectionModel : public QAbstractTableModel
{
    Q_OBJECT
    
    Q_PROPERTY(QString bestFormat READ bestFormat NOTIFY resultsChanged)
    Q_PROPERTY(int bestConfidence READ bestConfidence NOTIFY resultsChanged)
    Q_PROPERTY(QString bestFormatName READ bestFormatName NOTIFY resultsChanged)
    Q_PROPERTY(bool hasResults READ hasResults NOTIFY resultsChanged)
    Q_PROPERTY(int warningCount READ warningCount NOTIFY resultsChanged)
    Q_PROPERTY(double detectionTime READ detectionTime NOTIFY resultsChanged)
    Q_PROPERTY(qint64 fileSize READ fileSize NOTIFY resultsChanged)

public:
    enum Column {
        ColFormat = 0,
        ColName,
        ColConfidence,
        ColHeuristics,
        ColCount
    };
    
    explicit UftFormatDetectionModel(QObject *parent = nullptr);
    ~UftFormatDetectionModel();
    
    /* QAbstractTableModel interface */
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    
    /* Properties */
    QString bestFormat() const { return m_bestFormat; }
    int bestConfidence() const { return m_bestConfidence; }
    QString bestFormatName() const { return m_bestFormatName; }
    bool hasResults() const { return !m_candidates.isEmpty(); }
    int warningCount() const { return m_warnings.size(); }
    double detectionTime() const { return m_detectionTimeMs; }
    qint64 fileSize() const { return m_fileSize; }
    
    /* Data access */
    const QList<UftFormatCandidate>& candidates() const { return m_candidates; }
    const QList<UftDetectionWarning>& warnings() const { return m_warnings; }
    UftFormatCandidate candidateAt(int index) const;
    
public slots:
    /**
     * @brief Detect format from file path
     */
    void detectFromFile(const QString &path);
    
    /**
     * @brief Detect format from buffer
     */
    void detectFromBuffer(const QByteArray &data, const QString &extension = QString());
    
    /**
     * @brief Clear results
     */
    void clear();
    
    /**
     * @brief Update from C result structure
     */
    void updateFromResult(const uft_detect_result_t *result);

signals:
    void resultsChanged();
    void detectionStarted(const QString &path);
    void detectionFinished(bool success);
    void errorOccurred(const QString &error);

private:
    void populateFromResult(const uft_detect_result_t *result);
    QString formatIdToString(int format) const;
    QString formatIdToName(int format) const;
    QString heuristicsToString(uint32_t flags) const;
    
    QList<UftFormatCandidate> m_candidates;
    QList<UftDetectionWarning> m_warnings;
    QString m_bestFormat;
    QString m_bestFormatName;
    int m_bestConfidence = 0;
    double m_detectionTimeMs = 0;
    qint64 m_fileSize = 0;
};

#endif /* UFT_FORMAT_DETECTION_MODEL_H */
