/**
 * @file toolstab.h
 * @brief Tools Tab - Disk Utilities
 * 
 * P0-GUI-006 FIX: Full implementation
 */

#ifndef TOOLSTAB_H
#define TOOLSTAB_H

#include <QWidget>

namespace Ui { class TabTools; }

class ToolsTab : public QWidget
{
    Q_OBJECT

public:
    explicit ToolsTab(QWidget *parent = nullptr);
    ~ToolsTab();

signals:
    void statusMessage(const QString& message);

private slots:
    // Analysis tools
    void onDiskInfo();
    void onHexView();
    void onTrackView();
    void onFluxView();
    void onSectorEdit();
    void onAnalyze();
    
    // Conversion tools
    void onConvert();
    void onRepair();
    void onCompare();
    void onCreateBlank();
    
    // Batch operations
    void onBatchStart();
    void onBatchStop();
    
    // Browse buttons
    void onBrowseConvertSource();
    void onBrowseConvertTarget();
    void onBrowseRepair();
    void onBrowseCompareA();
    void onBrowseCompareB();
    void onBrowseAnalyze();
    void onBrowseBatch();
    
    // Output
    void onClearOutput();
    void onSaveOutput();

private:
    void setupConnections();
    void appendOutput(const QString& text);
    
    Ui::TabTools *ui;
    bool m_batchRunning;
};

#endif // TOOLSTAB_H
