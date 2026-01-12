/**
 * @file hardwaretab.h
 * @brief Hardware Tab - Floppy Controller Management
 * 
 * P0-GUI-003 FIX: Full implementation
 */

#ifndef HARDWARETAB_H
#define HARDWARETAB_H

#include <QWidget>
#include <QTimer>

namespace Ui { class TabHardware; }

class HardwareTab : public QWidget
{
    Q_OBJECT

public:
    explicit HardwareTab(QWidget *parent = nullptr);
    ~HardwareTab();
    
    bool isConnected() const { return m_connected; }
    QString currentController() const { return m_controllerType; }

signals:
    void connectionChanged(bool connected);
    void statusMessage(const QString& message);

private slots:
    void onRefreshPorts();
    void onConnect();
    void onDetect();
    void onMotorOn();
    void onMotorOff();
    void onSeekTest();
    void onReadTest();
    void onRPMTest();
    void onCalibrate();
    void onControllerChanged(int index);

private:
    void setupConnections();
    void updateStatus(const QString& status, bool isError = false);
    void updateUIState();
    void detectSerialPorts();
    
    Ui::TabHardware *ui;
    
    bool m_connected;
    QString m_controllerType;
    QString m_portName;
    QString m_firmwareVersion;
};

#endif // HARDWARETAB_H
