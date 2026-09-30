/**
 * @file formattab.h
 * @brief The settings tab: copy mode, copy plan, system/format, and the
 *        settings that reach an operation (MF-1618).
 *
 * Rebuilt from the owner's Qt Designer draft (2026-09-29) and his four
 * answers of the same day; see forms/tab_format.ui for the list. What this
 * class keeps from the tab it replaces is exactly what had a reader:
 * the copy plan (profiles, axes, findings, forced values, JSON), the
 * revolution count for the capture workflow, the plan source for the
 * other tabs (MF-1265), and the two gates (plugin capabilities, plan
 * parameters). The ~80 fields of the old form that reached nothing are
 * gone with the old form (owner: "nimm dann das alt gui überall raus");
 * fields of the new form that reach nothing yet are visible, disabled,
 * and marked "noch nicht verdrahtet".
 */

#ifndef FORMATTAB_H
#define FORMATTAB_H

#include <QWidget>
#include <QJsonObject>
#include <QString>
#include <QStringList>

extern "C" {
    #include "uft/core/uft_copy_plan.h"   /* MF-1233: der Kopierplan */
}

namespace Ui { class TabFormat; }

class FormatTab : public QWidget {
    Q_OBJECT

public:
    explicit FormatTab(QWidget *parent = nullptr);
    ~FormatTab() override;

    /** Der Plan, wie ihn die Auswahlfelder gerade beschreiben. */
    uft_copy_plan_t copyPlan() const;
    /** Umdrehungen fuer die Aufnahme im Arbeitsablauf (MF-1364). */
    int leseUmdrehungen() const;

    /** Faehigkeiten des gewaehlten Formats fuer `uft_copy_plan_check()`.
     *  Was dieser Reiter nicht messen kann, bleibt ungesetzt. */
    uint32_t copyPlanCaps() const;
    /** MF-1320: trennt "kann nichts davon" von "nicht nachschlagbar". */
    bool copyPlanCapsBekannt() const;

    /** Der aufgeloeste Plan als JSON (MF-1238). */
    QString planJson() const;

    /** Name des gewaehlten Formats, leer bei "Automatisch". */
    QString getSelectedFormat() const;
    /** Waehlt ein Format ueber seinen Registry-Namen (samt System). */
    bool waehleFormat(const QString &name);

    /* MF-1320 (E-15): fehlende Anker der beiden Tore, gezaehlt. */
    int gruppenTorFehlendeAnker() const { return m_gruppenTorFehlendeAnker; }
    int parameterTorFehlendeAnker() const { return m_parameterTorFehlendeAnker; }

    /* ── .uftsetup (MF-1618) ─────────────────────────────────────────── */
    /** Kopiermodus, Plan, Formatwahl und die wirksamen Werte. */
    QJsonObject setupAlsJson() const;
    /** Laedt in der Reihenfolge der Vorlage: Modus, System/Format/
     *  Variante, Sichtbarkeit, Werte. Werte fuer Felder, die hier
     *  verborgen oder nicht verdrahtet sind, werden NICHT angewandt;
     *  ihre Namen stehen in @p uebergangen. */
    bool setupAnwenden(const QJsonObject &o, QString *fehler,
                       QStringList *uebergangen = nullptr);
    /** Ordner fuer .uftsetup-Dateien. */
    static QString setupOrdner();
    /** Einmalige Uebernahme der Benutzer-Voreinstellungen aus der alten
     *  presets.json. Die Quelle bleibt unangetastet.
     *  @return Zahl der geschriebenen Setups, -1 bei Schreibfehler */
    static int uebernimmAltePresets(const QString &presetsPfad,
                                    const QString &zielOrdner,
                                    QStringList *meldungen = nullptr);

    /** Die Felder, die heute keinen Vorgang erreichen (objectNames). */
    static QStringList unverdrahteteFelder();

public slots:
    void setHardwareVerbunden(bool verbunden);
    void setHardwareGeraet(const QString &name, const QString &firmware);

private slots:
    void onSystemChanged(int index);
    void onFormatChanged(int index);
    void onVersionChanged(int index);
    void onCopyPlanChanged(int index);
    void onCopyPlanToggled(bool checked);
    void onVerifyToggled(bool checked);
    void onPlanJsonToggled(bool checked);
    void onPlanJsonKopieren();
    void onPlanJsonSichern();
    void onSetupLaden();
    void onSetupSpeichern();
    void onSetupReset();
    void onHelperPfadGeaendert();
    void onHelperSuchen();
    void onCapsLibSuchen();

private:
    Ui::TabFormat *ui;
    QString m_profil;           /**< leer = Benutzerdefiniert */
    QString m_basisProfil;
    bool    m_planFrei = false;
    bool    m_hardwareVerbunden = false;
    int m_gruppenTorFehlendeAnker = -1;    /**< -1 = Tor lief noch nicht */
    int m_parameterTorFehlendeAnker = -1;

    void fuelleSysteme();
    void fuelleFormate(const QString &system);
    void fuelleVarianten(const QString &format);
    void applyPluginCapabilities();
    void applyCopyPlan();
    void fuelleProfile();
    void wendeProfilAn(const QString &id);
    void setPlanFrei(bool frei);
    void zeigeCaps();
    void markiereUnverdrahtet();
    void verifyNachziehen(const uft_copy_plan_t &plan);
    void helferAusEinstellungen();
    void helferStatusZeigen();
    bool istVerborgen(const QWidget *w) const;
    static bool istUnverdrahtet(const QWidget *w);
};

#endif // FORMATTAB_H
