#include "formattab.h"
#include "ui_tab_format.h"
FormatTab::FormatTab(QWidget *parent) : QWidget(parent), ui(new Ui::TabFormat) {
    ui->setupUi(this);
}
FormatTab::~FormatTab() { delete ui; }
