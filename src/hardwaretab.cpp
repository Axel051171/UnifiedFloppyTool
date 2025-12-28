#include "hardwaretab.h"
#include "ui_tab_hardware.h"
HardwareTab::HardwareTab(QWidget *parent) : QWidget(parent), ui(new Ui::TabHardware) {
    ui->setupUi(this);
}
HardwareTab::~HardwareTab() { delete ui; }
