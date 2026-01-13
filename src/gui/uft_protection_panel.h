/**
 * @file uft_protection_panel.h
 * @brief Copy Protection Detection Panel
 */

#ifndef UFT_PROTECTION_PANEL_H
#define UFT_PROTECTION_PANEL_H

#include <QWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QListWidget>
#include <QTreeWidget>
#include <QPlainTextEdit>
#include <QLabel>
#include <QProgressBar>

class UftProtectionPanel : public QWidget
{
    Q_OBJECT

public:
    explicit UftProtectionPanel(QWidget *parent = nullptr);

    struct DetectionResult {
        QString protectionName;
        QString platform;
        int confidence;
        QString details;
    };

    QList<DetectionResult> getResults() const { return m_results; }

signals:
    void detectionComplete(const QString &protectionName, int confidence);

public slots:
    void runDetection();
    void showDetails(QTreeWidgetItem *item, int column);

private:
    void setupUi();
    void createDetectionGroup();
    void createPlatformGroup();
    void createResultsGroup();
    void populateProtectionList();

    QGroupBox *m_detectionGroup;
    QCheckBox *m_autoDetect;
    QCheckBox *m_deepScan;
    QComboBox *m_scanMode;
    QPushButton *m_detectButton;
    QProgressBar *m_progress;

    QGroupBox *m_platformGroup;
    QCheckBox *m_c64, *m_amiga, *m_atariST, *m_apple2, *m_pc;

    QGroupBox *m_resultsGroup;
    QTreeWidget *m_protectionTree;
    QPlainTextEdit *m_detailsView;
    QLabel *m_statusLabel;

    QListWidget *m_knownProtections;
    QList<DetectionResult> m_results;
};

#endif // UFT_PROTECTION_PANEL_H
