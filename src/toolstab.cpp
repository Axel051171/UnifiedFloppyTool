#include "toolstab.h"
#include "ui_tab_tools.h"
ToolsTab::ToolsTab(QWidget *parent) : QWidget(parent), ui(new Ui::TabTools) {
    ui->setupUi(this);
}
ToolsTab::~ToolsTab() { delete ui; }
