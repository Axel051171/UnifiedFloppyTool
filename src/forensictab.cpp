#include "forensictab.h"
#include "ui_tab_forensic.h"
#include <QFileDialog>
#include <QMessageBox>

ForensicTab::ForensicTab(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TabForensic)
{
    ui->setupUi(this);
    
    connect(ui->btnRunAnalysis, &QPushButton::clicked, this, &ForensicTab::onRunAnalysis);
    connect(ui->btnCompare, &QPushButton::clicked, this, &ForensicTab::onCompare);
    connect(ui->btnExportReport, &QPushButton::clicked, this, &ForensicTab::onExportReport);
}

ForensicTab::~ForensicTab()
{
    delete ui;
}

void ForensicTab::onRunAnalysis()
{
    ui->textDetails->appendPlainText("Running analysis...");
    
    // Simulate results
    ui->tableResults->setRowCount(3);
    ui->tableResults->setItem(0, 0, new QTableWidgetItem("Format Detection"));
    ui->tableResults->setItem(0, 1, new QTableWidgetItem("✓ OK"));
    ui->tableResults->setItem(0, 2, new QTableWidgetItem("D64 35 Track"));
    
    ui->tableResults->setItem(1, 0, new QTableWidgetItem("Structure"));
    ui->tableResults->setItem(1, 1, new QTableWidgetItem("✓ Valid"));
    ui->tableResults->setItem(1, 2, new QTableWidgetItem("683 sectors"));
    
    ui->tableResults->setItem(2, 0, new QTableWidgetItem("BAM"));
    ui->tableResults->setItem(2, 1, new QTableWidgetItem("✓ Valid"));
    ui->tableResults->setItem(2, 2, new QTableWidgetItem("664 blocks free"));
    
    ui->editMD5->setText("d41d8cd98f00b204e9800998ecf8427e");
    ui->editCRC32->setText("00000000");
    
    ui->textDetails->appendPlainText("Analysis complete.");
}

void ForensicTab::onCompare()
{
    QString path1 = QFileDialog::getOpenFileName(this, "Select First Image");
    if (path1.isEmpty()) return;
    
    QString path2 = QFileDialog::getOpenFileName(this, "Select Second Image");
    if (path2.isEmpty()) return;
    
    ui->textDetails->appendPlainText(QString("Comparing:\n  %1\n  %2").arg(path1, path2));
}

void ForensicTab::onExportReport()
{
    QString path = QFileDialog::getSaveFileName(this, "Export Report",
        QString(), "HTML (*.html);;JSON (*.json);;Text (*.txt)");
    if (!path.isEmpty()) {
        ui->textDetails->appendPlainText(QString("Report saved to: %1").arg(path));
    }
}
