#ifndef HARDWARETAB_H
#define HARDWARETAB_H
#include <QWidget>
namespace Ui { class TabHardware; }
class HardwareTab : public QWidget {
    Q_OBJECT
public:
    explicit HardwareTab(QWidget *parent = nullptr);
    ~HardwareTab();
private:
    Ui::TabHardware *ui;
};
#endif
