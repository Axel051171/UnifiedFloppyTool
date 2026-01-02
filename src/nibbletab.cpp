#include "nibbletab.h"
#include "ui_tab_nibble.h"

NibbleTab::NibbleTab(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TabNibble)
{
    ui->setupUi(this);
}

NibbleTab::~NibbleTab()
{
    delete ui;
}
