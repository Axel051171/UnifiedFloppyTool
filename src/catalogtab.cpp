#include "catalogtab.h"
#include "ui_tab_catalog.h"
CatalogTab::CatalogTab(QWidget *parent) : QWidget(parent), ui(new Ui::TabCatalog) {
    ui->setupUi(this);
}
CatalogTab::~CatalogTab() { delete ui; }
