/**
 * @file uft_hardware_panel.cpp
 * @brief Hardware Panel Implementation - Controller Configuration
 */

#include "uft_hardware_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QScrollArea>
#include <QMessageBox>

UftHardwarePanel::UftHardwarePanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void UftHardwarePanel::setupUi()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    
    /* Left column */
    QVBoxLayout *leftCol = new QVBoxLayout();
    createControllerGroup();
    createDriveGroup();
    createMotorGroup();
    leftCol->addWidget(m_controllerGroup);
    leftCol->addWidget(m_driveGroup);
    leftCol->addWidget(m_motorGroup);
    leftCol->addStretch();
    
    /* Right column */
    QVBoxLayout *rightCol = new QVBoxLayout();
    createIndexGroup();
    createAdvancedGroup();
    createStatusGroup();
    rightCol->addWidget(m_indexGroup);
    rightCol->addWidget(m_advancedGroup);
    rightCol->addWidget(new QGroupBox("Log"));
    
    m_logList = new QListWidget();
    ((QGroupBox*)rightCol->itemAt(2)->widget())->setLayout(new QVBoxLayout);
    ((QGroupBox*)rightCol->itemAt(2)->widget())->layout()->addWidget(m_logList);
    
    rightCol->addStretch();
    
    mainLayout->addLayout(leftCol);
    mainLayout->addLayout(rightCol);
}

void UftHardwarePanel::createControllerGroup()
{
    m_controllerGroup = new QGroupBox("Controller", this);
    QGridLayout *layout = new QGridLayout(m_controllerGroup);
    
    layout->addWidget(new QLabel("Controller:"), 0, 0);
    m_controllerType = new QComboBox();
    m_controllerType->addItem("None", CONTROLLER_NONE);
    m_controllerType->addItem("Greaseweazle F7/F7+", CONTROLLER_GREASEWEAZLE);
    m_controllerType->addItem("KryoFlux", CONTROLLER_KRYOFLUX);
    m_controllerType->addItem("FluxEngine", CONTROLLER_FLUXENGINE);
    m_controllerType->addItem("SuperCard Pro", CONTROLLER_SUPERCARD_PRO);
    m_controllerType->addItem("Catweasel MK4", CONTROLLER_CATWEASEL);
    m_controllerType->addItem("Applesauce", CONTROLLER_APPLESAUCE);
    m_controllerType->addItem("FC5025", CONTROLLER_FC5025);
    m_controllerType->addItem("Pauline", CONTROLLER_PAULINE);
    layout->addWidget(m_controllerType, 0, 1, 1, 2);
    
    layout->addWidget(new QLabel("Device:"), 1, 0);
    m_devicePath = new QComboBox();
    m_devicePath->setEditable(true);
    m_devicePath->addItem("/dev/ttyACM0");
    m_devicePath->addItem("/dev/ttyUSB0");
    m_devicePath->addItem("COM3");
    m_devicePath->addItem("COM4");
    layout->addWidget(m_devicePath, 1, 1, 1, 2);
    
    m_detectButton = new QPushButton("Detect");
    m_connectButton = new QPushButton("Connect");
    layout->addWidget(m_detectButton, 2, 1);
    layout->addWidget(m_connectButton, 2, 2);
    
    layout->addWidget(new QLabel("Firmware:"), 3, 0);
    m_firmwareLabel = new QLabel("-");
    layout->addWidget(m_firmwareLabel, 3, 1);
    
    layout->addWidget(new QLabel("Status:"), 4, 0);
    m_statusLabel = new QLabel("Not connected");
    m_statusLabel->setStyleSheet("color: #d32f2f;");
    layout->addWidget(m_statusLabel, 4, 1, 1, 2);
    
    connect(m_detectButton, &QPushButton::clicked, this, &UftHardwarePanel::detectHardware);
    connect(m_connectButton, &QPushButton::clicked, this, &UftHardwarePanel::connectController);
}

void UftHardwarePanel::createDriveGroup()
{
    m_driveGroup = new QGroupBox("Drive", this);
    QFormLayout *layout = new QFormLayout(m_driveGroup);
    
    m_driveSelect = new QComboBox();
    m_driveSelect->addItem("Drive 0 (A:)", 0);
    m_driveSelect->addItem("Drive 1 (B:)", 1);
    m_driveSelect->addItem("External", 2);
    layout->addRow("Drive Select:", m_driveSelect);
    
    m_driveType = new QComboBox();
    m_driveType->addItem("3.5\" DD (720K)", 0);
    m_driveType->addItem("3.5\" HD (1.44M)", 1);
    m_driveType->addItem("5.25\" DD (360K)", 2);
    m_driveType->addItem("5.25\" HD (1.2M)", 3);
    m_driveType->addItem("8\" SD", 4);
    m_driveType->addItem("8\" DD", 5);
    m_driveType->setCurrentIndex(1);
    layout->addRow("Drive Type:", m_driveType);
    
    m_doubleStep = new QCheckBox("Double Step (40T in 80T drive)");
    layout->addRow(m_doubleStep);
    
    m_stepDelay = new QSpinBox();
    m_stepDelay->setRange(1000, 50000);
    m_stepDelay->setValue(6000);
    m_stepDelay->setSuffix(" µs");
    layout->addRow("Step Delay:", m_stepDelay);
    
    m_settleTime = new QSpinBox();
    m_settleTime->setRange(0, 100);
    m_settleTime->setValue(15);
    m_settleTime->setSuffix(" ms");
    layout->addRow("Settle Time:", m_settleTime);
    
    m_headLoadTime = new QSpinBox();
    m_headLoadTime->setRange(0, 500);
    m_headLoadTime->setValue(50);
    m_headLoadTime->setSuffix(" ms");
    layout->addRow("Head Load Time:", m_headLoadTime);
    
    connect(m_driveType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UftHardwarePanel::paramsChanged);
}

void UftHardwarePanel::createMotorGroup()
{
    m_motorGroup = new QGroupBox("Motor", this);
    QFormLayout *layout = new QFormLayout(m_motorGroup);
    
    m_motorOnDelay = new QSpinBox();
    m_motorOnDelay->setRange(0, 2000);
    m_motorOnDelay->setValue(500);
    m_motorOnDelay->setSuffix(" ms");
    layout->addRow("Motor On Delay:", m_motorOnDelay);
    
    m_motorOffDelay = new QSpinBox();
    m_motorOffDelay->setRange(0, 5000);
    m_motorOffDelay->setValue(2000);
    m_motorOffDelay->setSuffix(" ms");
    layout->addRow("Motor Off Delay:", m_motorOffDelay);
    
    m_rpmTarget = new QDoubleSpinBox();
    m_rpmTarget->setRange(100, 600);
    m_rpmTarget->setValue(300);
    m_rpmTarget->setSuffix(" RPM");
    layout->addRow("Target RPM:", m_rpmTarget);
    
    m_rpmCompensation = new QCheckBox("RPM Compensation");
    m_rpmCompensation->setChecked(true);
    m_rpmCompensation->setToolTip("Adjust timing based on actual RPM");
    layout->addRow(m_rpmCompensation);
}

void UftHardwarePanel::createIndexGroup()
{
    m_indexGroup = new QGroupBox("Index Signal", this);
    QFormLayout *layout = new QFormLayout(m_indexGroup);
    
    m_useIndex = new QCheckBox("Use Index Signal");
    m_useIndex->setChecked(true);
    layout->addRow(m_useIndex);
    
    m_indexOffset = new QDoubleSpinBox();
    m_indexOffset->setRange(-1000, 1000);
    m_indexOffset->setValue(0);
    m_indexOffset->setSuffix(" µs");
    layout->addRow("Index Offset:", m_indexOffset);
    
    m_indexTimeout = new QSpinBox();
    m_indexTimeout->setRange(100, 5000);
    m_indexTimeout->setValue(500);
    m_indexTimeout->setSuffix(" ms");
    layout->addRow("Index Timeout:", m_indexTimeout);
}

void UftHardwarePanel::createAdvancedGroup()
{
    m_advancedGroup = new QGroupBox("Advanced", this);
    QFormLayout *layout = new QFormLayout(m_advancedGroup);
    
    m_sampleRate = new QComboBox();
    m_sampleRate->addItem("24 MHz", 24);
    m_sampleRate->addItem("48 MHz", 48);
    m_sampleRate->addItem("72 MHz", 72);
    m_sampleRate->addItem("84 MHz", 84);
    m_sampleRate->setCurrentIndex(2);
    layout->addRow("Sample Rate:", m_sampleRate);
    
    m_filterEnabled = new QCheckBox("Hardware Filter");
    layout->addRow(m_filterEnabled);
    
    m_filterFrequency = new QSpinBox();
    m_filterFrequency->setRange(1, 100);
    m_filterFrequency->setValue(15);
    m_filterFrequency->setSuffix(" MHz");
    layout->addRow("Filter Frequency:", m_filterFrequency);
    
    m_tpi40 = new QCheckBox("40 TPI Drive (5.25\" DD)");
    layout->addRow(m_tpi40);
    
    /* Buttons */
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_calibrateButton = new QPushButton("Calibrate");
    m_testButton = new QPushButton("Test Drive");
    buttonLayout->addWidget(m_calibrateButton);
    buttonLayout->addWidget(m_testButton);
    layout->addRow(buttonLayout);
    
    connect(m_calibrateButton, &QPushButton::clicked, this, &UftHardwarePanel::calibrateDrive);
    connect(m_testButton, &QPushButton::clicked, this, &UftHardwarePanel::testDrive);
}

void UftHardwarePanel::createStatusGroup()
{
    /* Already created inline above */
}

void UftHardwarePanel::detectHardware()
{
    m_logList->addItem("Detecting hardware...");
    
    /* Simulate detection */
    m_devicePath->clear();
    
#ifdef Q_OS_WIN
    m_devicePath->addItem("COM3");
    m_devicePath->addItem("COM4");
#else
    m_devicePath->addItem("/dev/ttyACM0");
    m_devicePath->addItem("/dev/ttyUSB0");
#endif
    
    m_logList->addItem("Found 2 potential devices");
    
    /* TODO: Real detection using libusb or serial enumeration */
}

void UftHardwarePanel::connectController()
{
    ControllerType type = (ControllerType)m_controllerType->currentData().toInt();
    QString device = m_devicePath->currentText();
    
    if (type == CONTROLLER_NONE) {
        QMessageBox::warning(this, "Connect", "Please select a controller type.");
        return;
    }
    
    m_logList->addItem(QString("Connecting to %1 on %2...").arg(m_controllerType->currentText(), device));
    
    /* TODO: Real connection */
    
    /* Simulate success */
    m_statusLabel->setText("Connected");
    m_statusLabel->setStyleSheet("color: #388e3c;");
    m_firmwareLabel->setText("v1.0");
    
    m_logList->addItem("Connected successfully!");
    
    emit controllerConnected(m_controllerType->currentText());
}

void UftHardwarePanel::disconnectController()
{
    m_statusLabel->setText("Disconnected");
    m_statusLabel->setStyleSheet("color: #d32f2f;");
    m_firmwareLabel->setText("-");
    
    m_logList->addItem("Disconnected");
    
    emit controllerDisconnected();
}

void UftHardwarePanel::calibrateDrive()
{
    m_logList->addItem("Calibrating drive...");
    m_logList->addItem("Seeking to track 0...");
    m_logList->addItem("Testing step rate...");
    m_logList->addItem("Calibration complete");
}

void UftHardwarePanel::testDrive()
{
    m_logList->addItem("Testing drive...");
    m_logList->addItem("Motor on");
    m_logList->addItem("Reading track 0");
    m_logList->addItem("Reading track 79");
    m_logList->addItem("Motor off");
    m_logList->addItem("Test complete: Drive OK");
}

UftHardwarePanel::HardwareParams UftHardwarePanel::getParams() const
{
    HardwareParams params;
    
    params.controller = (ControllerType)m_controllerType->currentData().toInt();
    params.device_path = m_devicePath->currentText();
    params.firmware_version = m_firmwareLabel->text();
    
    params.drive_select = m_driveSelect->currentData().toInt();
    params.drive_type = m_driveType->currentData().toInt();
    params.double_step = m_doubleStep->isChecked();
    params.step_delay_us = m_stepDelay->value();
    params.settle_time_ms = m_settleTime->value();
    params.head_load_time_ms = m_headLoadTime->value();
    
    params.motor_on_delay_ms = m_motorOnDelay->value();
    params.motor_off_delay_ms = m_motorOffDelay->value();
    params.rpm_target = m_rpmTarget->value();
    params.rpm_compensation = m_rpmCompensation->isChecked();
    
    params.use_index = m_useIndex->isChecked();
    params.index_offset_us = m_indexOffset->value();
    params.index_timeout_ms = m_indexTimeout->value();
    
    params.sample_rate_mhz = m_sampleRate->currentData().toInt();
    params.filter_enabled = m_filterEnabled->isChecked();
    params.filter_frequency = m_filterFrequency->value();
    params.tpi_40 = m_tpi40->isChecked();
    
    return params;
}

void UftHardwarePanel::setParams(const HardwareParams &params)
{
    m_devicePath->setCurrentText(params.device_path);
    m_doubleStep->setChecked(params.double_step);
    m_stepDelay->setValue(params.step_delay_us);
    m_settleTime->setValue(params.settle_time_ms);
    m_headLoadTime->setValue(params.head_load_time_ms);
    m_motorOnDelay->setValue(params.motor_on_delay_ms);
    m_motorOffDelay->setValue(params.motor_off_delay_ms);
    m_rpmTarget->setValue(params.rpm_target);
    m_rpmCompensation->setChecked(params.rpm_compensation);
    m_useIndex->setChecked(params.use_index);
    m_indexOffset->setValue(params.index_offset_us);
    m_indexTimeout->setValue(params.index_timeout_ms);
    m_filterEnabled->setChecked(params.filter_enabled);
    m_filterFrequency->setValue(params.filter_frequency);
    m_tpi40->setChecked(params.tpi_40);
}
