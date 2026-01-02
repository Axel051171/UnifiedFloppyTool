#include "protectiontab.h"
#include "ui_tab_protection.h"
ProtectionTab::ProtectionTab(QWidget *parent) : QWidget(parent), ui(new Ui::TabProtection) {
    ui->setupUi(this);
}
ProtectionTab::~ProtectionTab() { delete ui; }
