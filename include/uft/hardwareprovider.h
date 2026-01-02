diff --git a/src/hardware/hardwareprovider.h b/src/hardware/hardwareprovider.h
new file mode 100644
index 0000000000000000000000000000000000000000..80c9d976bab65d7dcb8fd4668b14f490e8f76f3b
--- /dev/null
+++ b/src/hardware/hardwareprovider.h
@@ -0,0 +1,49 @@
+#ifndef HARDWAREPROVIDER_H
+#define HARDWAREPROVIDER_H
+
+#include <QMetaType>
+#include <QObject>
+#include <QString>
+
+struct DetectedDriveInfo {
+    QString type;
+    int tracks = 0;
+    int heads = 0;
+    QString density;
+    QString rpm;
+    QString model;
+};
+
+struct HardwareInfo {
+    QString vendor;
+    QString product;
+    QString firmware;
+    QString clock;
+};
+
+Q_DECLARE_METATYPE(DetectedDriveInfo)
+Q_DECLARE_METATYPE(HardwareInfo)
+
+class HardwareProvider : public QObject
+{
+    Q_OBJECT
+
+public:
+    explicit HardwareProvider(QObject *parent = nullptr) : QObject(parent) {}
+    ~HardwareProvider() override = default;
+
+    virtual QString displayName() const = 0;
+    virtual void setHardwareType(const QString &hardwareType) = 0;
+    virtual void setDevicePath(const QString &devicePath) = 0;
+    virtual void setBaudRate(int baudRate) = 0;
+    virtual void detectDrive() = 0;
+    virtual void autoDetectDevice() = 0;
+
+signals:
+    void driveDetected(const DetectedDriveInfo &info);
+    void hardwareInfoUpdated(const HardwareInfo &info);
+    void statusMessage(const QString &message);
+    void devicePathSuggested(const QString &path);
+};
+
+#endif // HARDWAREPROVIDER_H
