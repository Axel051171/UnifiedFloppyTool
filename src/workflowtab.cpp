/**
 * @file workflowtab.cpp
 * @brief Workflow Tab - Mission Control Implementation with Worker Thread
 */
#include "workflowtab.h"
#include "ui_tab_workflow.h"
#include "decodejob.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QThread>

WorkflowTab::WorkflowTab(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TabWorkflow)
    , m_isRunning(false)
    , m_workerThread(nullptr)
    , m_decodeJob(nullptr)
{
    ui->setupUi(this);
    connectSignals();
    updateSourceStatus();
    updateDestinationStatus();
}

WorkflowTab::~WorkflowTab()
{
    // Cleanup worker thread if running
    if (m_workerThread && m_workerThread->isRunning()) {
        if (m_decodeJob) {
            m_decodeJob->requestCancel();
        }
        m_workerThread->quit();
        m_workerThread->wait(3000);
    }
    delete ui;
}

void WorkflowTab::connectSignals()
{
    // Source buttons
    connect(ui->btnSourceFlux, &QPushButton::clicked, this, &WorkflowTab::onSourceFluxClicked);
    connect(ui->btnSourceUSB, &QPushButton::clicked, this, &WorkflowTab::onSourceUSBClicked);
    connect(ui->btnSourceFile, &QPushButton::clicked, this, &WorkflowTab::onSourceFileClicked);
    
    // Destination buttons
    connect(ui->btnDestFlux, &QPushButton::clicked, this, &WorkflowTab::onDestFluxClicked);
    connect(ui->btnDestUSB, &QPushButton::clicked, this, &WorkflowTab::onDestUSBClicked);
    connect(ui->btnDestFile, &QPushButton::clicked, this, &WorkflowTab::onDestFileClicked);
    
    // Start/Abort button
    connect(ui->btnStart, &QPushButton::clicked, this, &WorkflowTab::onStartAbortClicked);
}

void WorkflowTab::onSourceFluxClicked()
{
    ui->labelSourceStatus->setText("Mode: Flux Device\nDevice: Greaseweazle v4.1 (COM3)\nStatus: Ready");
}

void WorkflowTab::onSourceUSBClicked()
{
    ui->labelSourceStatus->setText("Mode: USB Device\nDevice: Generic USB Floppy\nStatus: Ready");
}

void WorkflowTab::onSourceFileClicked()
{
    QString filename = QFileDialog::getOpenFileName(
        this, "Select Source File", QString(),
        "All Supported (*.scp *.hfe *.img *.d64 *.adf);;All Files (*.*)"
    );
    if (!filename.isEmpty()) {
        ui->labelSourceStatus->setText(QString("Mode: Image File\nFile: %1\nStatus: Ready").arg(filename));
    }
}

void WorkflowTab::onDestFluxClicked()
{
    ui->labelDestStatus->setText("Mode: Flux Device\nDevice: Greaseweazle v4.1 (COM3)\nStatus: Ready");
}

void WorkflowTab::onDestUSBClicked()
{
    ui->labelDestStatus->setText("Mode: USB Device\nDevice: Generic USB Floppy\nStatus: Ready");
}

void WorkflowTab::onDestFileClicked()
{
    QString filename = QFileDialog::getSaveFileName(
        this, "Select Destination File", QString(),
        "All Supported (*.scp *.hfe *.img *.d64 *.adf);;All Files (*.*)"
    );
    if (!filename.isEmpty()) {
        ui->labelDestStatus->setText(QString("Mode: Image File\nFile: %1\nAuto-increment: Enabled").arg(filename));
    }
}

void WorkflowTab::onStartAbortClicked()
{
    if (!m_isRunning) {
        // Start operation with worker thread
        m_isRunning = true;
        ui->btnStart->setText("⏹ ABBRECHEN");
        ui->btnStart->setStyleSheet("background-color: #f44336; color: white;");
        
        // Create worker thread
        m_workerThread = new QThread(this);
        m_decodeJob = new DecodeJob();
        m_decodeJob->moveToThread(m_workerThread);
        
        // Connect signals
        connect(m_workerThread, &QThread::started, m_decodeJob, &DecodeJob::run);
        connect(m_decodeJob, &DecodeJob::progress, this, [this](int pct) {
            qDebug() << "Progress:" << pct << "%";
            // TODO: Update progress bar in UI
        });
        connect(m_decodeJob, &DecodeJob::stageChanged, this, [this](const QString& stage) {
            qDebug() << "Stage:" << stage;
            ui->labelSourceStatus->setText(ui->labelSourceStatus->text() + "\n" + stage);
        });
        connect(m_decodeJob, &DecodeJob::finished, this, [this](const QString& result) {
            QMessageBox::information(this, "Success", result);
            onStartAbortClicked();  // Reset UI
        });
        connect(m_decodeJob, &DecodeJob::error, this, [this](const QString& error) {
            QMessageBox::warning(this, "Error", error);
            onStartAbortClicked();  // Reset UI
        });
        connect(m_decodeJob, &DecodeJob::finished, m_workerThread, &QThread::quit);
        connect(m_decodeJob, &DecodeJob::finished, m_decodeJob, &QObject::deleteLater);
        connect(m_workerThread, &QThread::finished, m_workerThread, &QObject::deleteLater);
        
        // Start worker
        m_workerThread->start();
        
    } else {
        // Abort operation
        if (m_decodeJob) {
            m_decodeJob->requestCancel();
        }
        
        m_isRunning = false;
        ui->btnStart->setText("▶ START");
        ui->btnStart->setStyleSheet("background-color: #4CAF50; color: white;");
    }
}

void WorkflowTab::updateSourceStatus()
{
    ui->labelSourceStatus->setText("Mode: Not configured\nPlease select source...");
}

void WorkflowTab::updateDestinationStatus()
{
    ui->labelDestStatus->setText("Mode: Not configured\nPlease select destination...");
}
