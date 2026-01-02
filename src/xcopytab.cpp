#include "xcopytab.h"
#include "ui_tab_xcopy.h"
#include <QFileDialog>

XCopyTab::XCopyTab(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TabXCopy)
{
    ui->setupUi(this);
    
    connect(ui->btnBrowseSource, &QPushButton::clicked, this, &XCopyTab::onBrowseSource);
    connect(ui->btnBrowseDest, &QPushButton::clicked, this, &XCopyTab::onBrowseDest);
    connect(ui->btnStartCopy, &QPushButton::clicked, this, &XCopyTab::onStartCopy);
    connect(ui->btnStopCopy, &QPushButton::clicked, this, &XCopyTab::onStopCopy);
}

XCopyTab::~XCopyTab()
{
    delete ui;
}

void XCopyTab::onBrowseSource()
{
    QString path = QFileDialog::getOpenFileName(this, "Select Source Image",
        QString(), "Disk Images (*.d64 *.adf *.img *.scp *.hfe);;All Files (*)");
    if (!path.isEmpty()) {
        ui->editSourceFile->setText(path);
    }
}

void XCopyTab::onBrowseDest()
{
    QString path = QFileDialog::getSaveFileName(this, "Select Destination",
        QString(), "Disk Images (*.d64 *.adf *.img *.scp *.hfe);;All Files (*)");
    if (!path.isEmpty()) {
        ui->editDestFile->setText(path);
    }
}

void XCopyTab::onStartCopy()
{
    ui->btnStartCopy->setEnabled(false);
    ui->btnStopCopy->setEnabled(true);
    // TODO: Start copy operation
}

void XCopyTab::onStopCopy()
{
    ui->btnStartCopy->setEnabled(true);
    ui->btnStopCopy->setEnabled(false);
    // TODO: Stop copy operation
}
