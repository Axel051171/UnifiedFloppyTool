#pragma once
#include <QWidget>
namespace Ui { class TabCatalog; }
class CatalogTab : public QWidget {
    Q_OBJECT
public:
    explicit CatalogTab(QWidget *parent = nullptr);
    ~CatalogTab();
private:
    Ui::TabCatalog *ui;
};
