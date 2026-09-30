/**
 * @file formattab.cpp
 * @brief The settings tab (MF-1618) — see formattab.h for what it keeps
 *        and why.
 *
 * Carried over from the tab it replaces, with their reasons intact:
 * the copy plan and its named modes (MF-1233/1235/1237/1238), the hidden-
 * field rule (MF-1306), the capability gate and the parameter gate with
 * "unknown hides nothing" (MF-661/1234/1320), the variant list from the
 * plugin (MF-1231/1308), and the plan source for the other tabs (MF-1265).
 */

#include "formattab.h"
#include "ui_tab_format.h"

#include <uft/uft_format_plugin.h>
#include <uft/uft_format_probe.h>
#include <uft/formats/ipf/uft_ipf_helper.h>   /* UFT_IPF_HELPER_ENV */

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>

/* The capsimg helper's own variable for the library path
 * (tools/capsimg-helper/README.md). UFT only passes it on. */
static const char *const kCapsLibEnv = "UFT_CAPSIMG_LIB";
static const char *const kWerkzeugGruppe = "Werkzeuge";

/* ── Systeme: die Gruppierung von Hand, der Bestand aus der Registry ──
 *
 * Kein Plugin traegt ein System (gemessen: `uft_format_plugin_t` hat kein
 * solches Feld). Die Gruppierung unten ist deshalb eine FESTLEGUNG — sie
 * stammt aus dem alten Reiter —, aber welche Formate erscheinen, sagt die
 * Registry: ein Name, der dort nicht registriert ist, wird nicht angeboten,
 * und jedes registrierte Format, das keiner Gruppe angehoert, steht unter
 * „Weitere Formate". Vorher bot der Reiter Namen an, zu denen es kein
 * Plugin gab ("360K", "2IMG", "NorthStar" …). */
struct SystemGruppe { const char *system; const char *formate; };
static const SystemGruppe kSysteme[] = {
    { "Commodore 64/128",            "D64 G64 D71 D81" },
    { "Commodore Plus/4",            "D64 D71 TAP" },
    { "Commodore VIC-20",            "D64 TAP PRG" },
    { "Commodore PET/CBM",           "D64 D80 D82 D67" },
    { "Amiga",                       "ADF ADZ HDF DMS IPF" },
    { "Apple II",                    "WOZ A2R NIB PO DO 2IMG DSK D13" },
    { "Apple III",                   "PO 2IMG DSK" },
    { "Macintosh (400K/800K)",       "DC42 IMG DART" },
    { "Atari ST/STE",                "ST STX MSA DIM STT IPF" },
    { "Atari 8-bit (400/800/XL/XE)", "ATR ATX XFD DCM PRO XEX" },
    { "ZX Spectrum",                 "TRD SCL TZX TAP DSK FDI TD0 UDI OPD MGT" },
    { "SAM Coupé",                   "MGT SAD DSK" },
    { "Amstrad CPC",                 "DSK EDSK RAW IPF SCP" },
    { "Amstrad PCW",                 "DSK EDSK IMG" },
    { "MSX",                         "DSK DMK IMG DIM" },
    { "BBC Micro",                   "SSD DSD ADF ADL UEF" },
    { "Acorn Archimedes",            "ADF ADL" },
    { "PC/DOS",                      "IMG IMA XDF DMF 2M TD0 IMD CQM 86F" },
    { "NEC PC-98",                   "D88 D77 NFD FDI HDM XDF" },
    { "Sharp X68000",                "XDF DIM D88" },
    { "FM Towns",                    "D88 D77 IMG" },
    { "TRS-80 (Model I/III/4)",      "DMK JV1 JV3 DSK IMD" },
    { "TRS-80 Color Computer",       "VDK DSK DMK JVC" },
    { "TI-99/4A",                    "DSK V9T9 PC99" },
    { "Thomson MO/TO",               "SAP HFE" },
    { "Oric Atmos",                  "DSK TAP" },
    { "Kaypro",                      "IMG TD0 IMD DSK" },
    { "Osborne",                     "IMG TD0 IMD" },
    { "North Star",                  "IMG TD0" },
    { "DEC PDP/VAX",                 "IMG TD0 IMD" },
    { "Heathkit/Zenith",             "IMG TD0 IMD" },
    { "Victor 9000",                 "IMG TD0 SCP" },
    { "Flux (raw)",                  "SCP HFE RAW KFRAW GWRAW A2R WOZ IPF FDI MFM MFI 86F" },
};
static const char *const kWeitere = "Weitere Formate";

static QStringList registrierteNamen()
{
    QStringList n;
    for (size_t i = 0; i < uft_registered_format_plugin_count(); i++) {
        const uft_format_plugin_t *p = uft_registered_format_plugin_at(i);
        if (p && p->name && *p->name) {
            const QString s = QString::fromUtf8(p->name);
            if (!n.contains(s)) n << s;
        }
    }
    return n;
}

static QStringList formateVon(const char *system, const QStringList &registriert)
{
    QStringList out;
    if (!system) return out;
    for (const SystemGruppe &g : kSysteme)
        if (qstrcmp(g.system, system) == 0)
            for (const QString &f : QString::fromLatin1(g.formate)
                                        .split(QLatin1Char(' '), Qt::SkipEmptyParts))
                if (registriert.contains(f) && !out.contains(f)) out << f;
    return out;
}

/* ── Die Felder ohne Leser (MF-1618) ─────────────────────────────────
 *
 * Eigentuemer-Antwort 2 vom 2026-09-29: sichtbar, gesperrt, markiert
 * „noch nicht verdrahtet". Die Liste ist gemessen, nicht geschaetzt: fuer
 * jedes Feld des Entwurfs ist in t5/settings/ZUORDNUNG_settings_redesign.md
 * der Leser gesucht (git grep ueber src/), und diese hier haben keinen.
 * `spinMaxRetries` ist seit MF-1619 verdrahtet (`uft_copy_plan_t.read_retries`
 * -> `decode_retries`).
 *
 * BERICHTIGT MF-1619 — `comboEncoding`: hier stand, es habe „einen Traeger
 * im Kern, aber noch keinen Weg dorthin". Gemessen hat der Traeger
 * (`flux_decoder_options_t.encoding`) genau EINEN Leser,
 * `flux_decode_track()`, und dessen Aufrufer ausserhalb seiner Datei sind
 * `uft_otdr_adaptive_decode.c` (ohne Aufrufer, MF-767) und
 * `uft_diag_gw.c` (nennt nur sich selbst). Keine Wandlung und kein
 * Lesevorgang fuehrt dorthin; die Wandler waehlen ihren Dekoder nach dem
 * ZIEL. Eine Verdrahtung waere eine Zusage ohne Tat — das Feld bleibt
 * markiert (P3-711). */
static const char *const kUnverdrahtet[] = {
    "comboEncoding", "comboRpm", "comboReadMode",
    "checkReadBetweenIndex", "checkIncludeRawFlux",
    "spinTrackStart", "spinTrackEnd", "checkAllTracks",
    "comboSectorsPerTrack", "comboInterleave",
    "checkPreserveTiming", "checkPreserveSync", "checkWeakBits",
    "checkDuplicateSectors", "checkProtectionSignals",
    "checkSectorCountAnomalies",
    "comboPrecomp", "checkSplicePoint", "checkEraseOddTracks",
    "comboErrors", "checkRetryOnError",
    "checkSkipBadSectors",
    "checkDiskStructure", "checkFilesystem", "checkAllocationMap",
    "spinClockAdjust", "checkAdaptivePll", "spinSyncTolerance",
};

QStringList FormatTab::unverdrahteteFelder()
{
    QStringList l;
    for (const char *n : kUnverdrahtet) l << QString::fromLatin1(n);
    return l;
}

bool FormatTab::istUnverdrahtet(const QWidget *w)
{
    return w && w->property("uft_unverdrahtet").toBool();
}

/* MF-1282: die Anordnung, die das Element wirklich enthaelt. */
static QLayout *layoutVon(QLayout *lay, QWidget *w, int *index)
{
    if (!lay) return nullptr;
    const int i = lay->indexOf(w);
    if (i >= 0) { *index = i; return lay; }
    for (int k = 0; k < lay->count(); ++k)
        if (QLayout *sub = lay->itemAt(k)->layout())
            if (QLayout *f = layoutVon(sub, w, index)) return f;
    return nullptr;
}

/* MF-1282: die Beschriftung eines Bedienelements — im Gitter die Zelle
 * links daneben, im Formular die LabelRole, im Kasten das vorige Element.
 * Steht dort kein QLabel, gibt es keine Beschriftung. */
static QLabel *beschriftungVon(QWidget *w)
{
    if (!w || !w->parentWidget()) return nullptr;
    int i = -1;
    QLayout *lay = layoutVon(w->parentWidget()->layout(), w, &i);
    if (!lay || i < 0) return nullptr;
    if (auto *f = qobject_cast<QFormLayout *>(lay)) {
        int r = -1;
        QFormLayout::ItemRole rolle = QFormLayout::FieldRole;
        f->getWidgetPosition(w, &r, &rolle);
        if (r < 0 || rolle != QFormLayout::FieldRole) return nullptr;
        QLayoutItem *it = f->itemAt(r, QFormLayout::LabelRole);
        return it ? qobject_cast<QLabel *>(it->widget()) : nullptr;
    }
    if (auto *g = qobject_cast<QGridLayout *>(lay)) {
        int r = 0, c = 0, rs = 0, cs = 0;
        g->getItemPosition(i, &r, &c, &rs, &cs);
        if (c <= 0) return nullptr;
        QLayoutItem *it = g->itemAtPosition(r, c - 1);
        return it ? qobject_cast<QLabel *>(it->widget()) : nullptr;
    }
    if (qobject_cast<QBoxLayout *>(lay)) {
        if (i == 0) return nullptr;
        QLayoutItem *it = lay->itemAt(i - 1);
        return it ? qobject_cast<QLabel *>(it->widget()) : nullptr;
    }
    return nullptr;
}

/* Die Wertelisten der Planachsen kommen aus dem Kern (MF-1233) — es gibt
 * sie genau einmal. */
static void fuelleAus(QComboBox *box, int anzahl,
                      const char *(*name)(int), int vorgabe)
{
    if (!box) return;
    box->blockSignals(true);
    box->clear();
    for (int i = 0; i < anzahl; i++) {
        const char *n = name(i);
        if (n) box->addItem(QString::fromUtf8(n), i);
    }
    const int idx = box->findData(vorgabe);
    if (idx >= 0) box->setCurrentIndex(idx);
    box->blockSignals(false);
}
static const char *nameLevel(int v)
{ return uft_copy_level_name(static_cast<uft_copy_level_t>(v)); }
static const char *nameStrategy(int v)
{ return uft_copy_strategy_name(static_cast<uft_read_strategy_t>(v)); }
static const char *namePreserve(int v)
{ return uft_copy_preservation_name(static_cast<uft_preservation_t>(v)); }
static const char *namePolicy(int v)
{ return uft_copy_policy_name(static_cast<uft_copy_policy_t>(v)); }
static const char *nameTrackMode(int v)
{ return uft_copy_track_mode_name(static_cast<uft_track_mode_t>(v)); }
static const char *nameFileSpecial(int v)
{ return uft_copy_file_special_name(static_cast<uft_file_special_t>(v)); }
static const char *nameVote(int v)
{ return uft_copy_vote_name(static_cast<uft_vote_method_t>(v)); }
static const char *nameExact(int v)
{ return uft_copy_exact_name(static_cast<uft_bitexact_kind_t>(v)); }

// ============================================================================
// Construction / Destruction
// ============================================================================

FormatTab::FormatTab(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TabFormat)
{
    ui->setupUi(this);

    fuelleAus(ui->comboPlanLevel,       UFT_COPY_LEVEL_N,    nameLevel,       UFT_COPY_SECTOR);
    fuelleAus(ui->comboPlanStrategy,    UFT_READ_STRATEGY_N, nameStrategy,    UFT_READ_STANDARD);
    fuelleAus(ui->comboPlanPreserve,    UFT_PRESERVE_N,      namePreserve,    UFT_PRESERVE_LOGICAL);
    fuelleAus(ui->comboPlanPolicy,      UFT_POLICY_N,        namePolicy,      UFT_POLICY_NORMAL);
    fuelleAus(ui->comboPlanTrackMode,   UFT_TRACK_MODE_N,    nameTrackMode,   UFT_TRACK_DECODED);
    fuelleAus(ui->comboPlanFileSpecial, UFT_FILE_SPECIAL_N,  nameFileSpecial, UFT_FILE_GENERIC);
    fuelleAus(ui->comboPlanVote,        UFT_VOTE_N,          nameVote,        UFT_VOTE_STRICT_MAJORITY);
    fuelleAus(ui->comboPlanExact,       UFT_EXACT_N,         nameExact,       UFT_EXACT_SECTOR);
    for (QComboBox *b : { ui->comboPlanLevel, ui->comboPlanStrategy,
                          ui->comboPlanPreserve, ui->comboPlanPolicy,
                          ui->comboPlanTrackMode, ui->comboPlanFileSpecial,
                          ui->comboPlanVote, ui->comboPlanExact })
        connect(b, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &FormatTab::onCopyPlanChanged);

    /* SHA-256 steht fest — der Kern bildet ihn immer (MF-1293). */
    ui->checkSHA256->setChecked(true);
    ui->checkSHA256->setEnabled(false);
    ui->checkSHA256->setToolTip(tr("SHA-256 steht fest und wird immer gebildet."));
    for (QCheckBox *c : { ui->checkHashSha512, ui->checkCRC32 })
        connect(c, &QCheckBox::toggled, this, &FormatTab::onCopyPlanToggled);

    ui->checkVerify->setToolTip(
        tr("Wirkt über die Richtlinie des Kopierplans: an heißt, die "
           "geschriebene Datei wird nach dem Wandeln erneut geöffnet und "
           "Sektor für Sektor gegen die Quelle gehalten (verify_after). "
           "Einstellbar im Modus „Benutzerdefiniert“."));
    connect(ui->checkVerify, &QCheckBox::toggled, this, &FormatTab::onVerifyToggled);

    /* MF-1619: wo die Zahl wirkt, steht an der Beschriftung — das
     * Parameter-Tor leert den Kurzhinweis des Feldes, sobald es frei ist. */
    ui->labelMaxRetries->setToolTip(
        tr("Leseversuche je Spur. Wirkt heute in der Wandlung SCP → D64: "
           "so viele Umdrehungen werden versucht (decode_retries). Legt die "
           "Lesestrategie die Zahl fest, ist das Feld gesperrt."));
    connect(ui->spinMaxRetries, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { applyCopyPlan(); });

    /* Die benannten Modi (MF-1237/1293): ein Knopf je Profil des Kerns,
     * verdrahtet ueber den Namen `btnModus_<Kennung>`. */
    for (size_t i = 0; i < uft_copy_profile_count(); i++) {
        const uft_copy_profile_t *p = uft_copy_profile(i);
        if (!p || !p->id) continue;
        const QString kennung = QString::fromUtf8(p->id);
        auto *b = findChild<QPushButton *>(QStringLiteral("btnModus_") + kennung);
        if (!b) {
            qWarning("[FormatTab] Modus '%s' hat keinen Knopf im Formular.", p->id);
            continue;
        }
        connect(b, &QPushButton::clicked, this, [this, kennung]() {
            wendeProfilAn(kennung);
            applyCopyPlan();
        });
    }
    /* „Benutzerdefiniert" steht NICHT im Kern: es ist die Abwesenheit eines
     * Profils. Die Achsen behalten die Werte des bisherigen Modus — der
     * Bediener faengt nicht bei Null an. */
    connect(ui->btnModus_custom, &QPushButton::clicked, this, [this]() {
        m_profil.clear();
        setPlanFrei(true);
        applyCopyPlan();
    });

    connect(ui->btnPlanJson, &QPushButton::toggled, this, &FormatTab::onPlanJsonToggled);
    connect(ui->btnPlanJsonKopieren, &QPushButton::clicked, this, &FormatTab::onPlanJsonKopieren);
    connect(ui->btnPlanJsonSichern, &QPushButton::clicked, this, &FormatTab::onPlanJsonSichern);

    connect(ui->comboSystem, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormatTab::onSystemChanged);
    connect(ui->comboFormat, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormatTab::onFormatChanged);
    connect(ui->comboVersion, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormatTab::onVersionChanged);

    connect(ui->btnSetupLaden, &QPushButton::clicked, this, &FormatTab::onSetupLaden);
    connect(ui->btnSetupSpeichern, &QPushButton::clicked, this, &FormatTab::onSetupSpeichern);
    connect(ui->btnSetupReset, &QPushButton::clicked, this, &FormatTab::onSetupReset);

    connect(ui->editHelperPath, &QLineEdit::editingFinished, this, &FormatTab::onHelperPfadGeaendert);
    connect(ui->editCapsLibPath, &QLineEdit::editingFinished, this, &FormatTab::onHelperPfadGeaendert);
    connect(ui->btnHelperPath, &QPushButton::clicked, this, &FormatTab::onHelperSuchen);
    connect(ui->btnCapsLibPath, &QPushButton::clicked, this, &FormatTab::onCapsLibSuchen);

    markiereUnverdrahtet();
    helferAusEinstellungen();
    fuelleSysteme();
    onSystemChanged(0);

    /* MF-1237: der Reiter oeffnet in einem BENANNTEN Modus. */
    wendeProfilAn(QStringLiteral("standardcopy"));
    applyCopyPlan();

    /* Einmalige Uebernahme der alten Voreinstellungen (Eigentuemer-
     * Vorgabe, MF-1618). Nur, wenn es eine presets.json gibt; die
     * Merkdatei im Setup-Ordner verhindert eine zweite Uebernahme. */
    {
        const QString alt = QStandardPaths::writableLocation(
                                QStandardPaths::AppDataLocation)
                            + QStringLiteral("/presets.json");
        const QString merk = setupOrdner() + QStringLiteral("/.presets_uebernommen");
        if (QFile::exists(alt) && !QFile::exists(merk)) {
            QStringList m;
            const int n = uebernimmAltePresets(alt, setupOrdner(), &m);
            if (n >= 0) {
                QFile f(merk);
                if (f.open(QIODevice::WriteOnly)) {
                    f.write(m.join(QLatin1Char('\n')).toUtf8());
                    f.close();
                }
                qInfo("[FormatTab] MF-1618: %d Voreinstellung(en) aus "
                      "presets.json als .uftsetup uebernommen", n);
            }
        }
    }

    /* MF-1265: der Reiter MELDET seinen Plan beim Kern an — ZULETZT, damit
     * die Auskunft nie ueber einen halb gebauten Reiter geht. */
    uft_copy_plan_set_quelle(
        [](void *ctx) -> uft_copy_plan_t {
            return static_cast<FormatTab *>(ctx)->copyPlan();
        },
        this);
}

FormatTab::~FormatTab()
{
    /* MF-1265: abmelden, BEVOR irgendetwas freigegeben wird. */
    uft_copy_plan_set_quelle(nullptr, nullptr);
    delete ui;
}

// ============================================================================
// Markierung „noch nicht verdrahtet"
// ============================================================================

void FormatTab::markiereUnverdrahtet()
{
    const QString tip = tr("Noch nicht verdrahtet: dieser Wert erreicht heute "
                           "keinen Vorgang. Das Feld ist sichtbar, damit "
                           "sichtbar ist, was fehlt.");
    auto stern = [](QString t) {
        if (t.endsWith(QLatin1Char(':'))) { t.chop(1); return t + QStringLiteral(" *:"); }
        return t + QStringLiteral(" *");
    };
    int n = 0;
    for (const char *name : kUnverdrahtet) {
        QWidget *w = findChild<QWidget *>(QString::fromLatin1(name));
        if (!w) {
            qWarning("[FormatTab] MF-1618: unverdrahtetes Feld '%s' fehlt im "
                     "Formular.", name);
            continue;
        }
        w->setProperty("uft_unverdrahtet", true);
        w->setEnabled(false);
        w->setToolTip(tip);
        if (auto *c = qobject_cast<QCheckBox *>(w))
            c->setText(stern(c->text()));
        else if (QLabel *l = beschriftungVon(w)) {
            l->setText(stern(l->text()));
            l->setToolTip(tip);
        }
        n++;
    }
    qInfo("[FormatTab] MF-1618: %d Felder als \"noch nicht verdrahtet\" "
          "gesperrt.", n);
}

// ============================================================================
// System / Format / Variante
// ============================================================================

void FormatTab::fuelleSysteme()
{
    const QStringList reg = registrierteNamen();
    ui->comboSystem->blockSignals(true);
    ui->comboSystem->clear();
    ui->comboSystem->addItem(tr("Automatisch"), QString());
    QSet<QString> zugeordnet;
    for (const SystemGruppe &g : kSysteme) {
        const QStringList f = formateVon(g.system, reg);
        if (f.isEmpty()) continue;
        for (const QString &s : f) zugeordnet.insert(s);
        ui->comboSystem->addItem(QString::fromUtf8(g.system),
                                 QString::fromUtf8(g.system));
    }
    bool weitere = false;
    for (const QString &s : reg)
        if (!zugeordnet.contains(s)) { weitere = true; break; }
    if (weitere)
        ui->comboSystem->addItem(tr("Weitere Formate"), QString::fromLatin1(kWeitere));
    ui->comboSystem->blockSignals(false);
}

void FormatTab::fuelleFormate(const QString &system)
{
    const QStringList reg = registrierteNamen();
    QStringList formate;
    if (system == QLatin1String(kWeitere)) {
        QSet<QString> zugeordnet;
        for (const SystemGruppe &g : kSysteme)
            for (const QString &s : formateVon(g.system, reg)) zugeordnet.insert(s);
        for (const QString &s : reg)
            if (!zugeordnet.contains(s)) formate << s;
        formate.sort(Qt::CaseInsensitive);
    } else if (!system.isEmpty()) {
        formate = formateVon(system.toUtf8().constData(), reg);
    }

    ui->comboFormat->blockSignals(true);
    ui->comboFormat->clear();
    if (formate.isEmpty())
        ui->comboFormat->addItem(tr("Automatisch"), QString());
    for (const QString &f : formate) {
        ui->comboFormat->addItem(f, f);
        const uft_format_plugin_t *p =
            uft_get_format_plugin_by_name(f.toUtf8().constData());
        if (p && p->description)
            ui->comboFormat->setItemData(ui->comboFormat->count() - 1,
                                         QString::fromUtf8(p->description),
                                         Qt::ToolTipRole);
    }
    ui->comboFormat->blockSignals(false);
    onFormatChanged(ui->comboFormat->currentIndex());
}

/* MF-1231/1308: Varianten nur aus dem Plugin. Die handgepflegte Liste des
 * alten Reiters fuehrte Varianten, deren Geometrie in Felder ohne Leser
 * floss; sie ist mit ihnen gegangen. Wo das Plugin nichts nennt, gibt es
 * keine Auswahl. */
void FormatTab::fuelleVarianten(const QString &format)
{
    ui->comboVersion->blockSignals(true);
    ui->comboVersion->clear();
    const uft_format_plugin_t *plugin = format.isEmpty()
        ? nullptr : uft_get_format_plugin_by_name(format.toUtf8().constData());
    if (plugin && plugin->variants && plugin->variant_count > 0) {
        for (size_t i = 0; i < plugin->variant_count; i++) {
            const uft_format_variant_t &v = plugin->variants[i];
            QString text = QString::fromUtf8(v.name);
            if (!v.can_write)
                text += (v.write_note && v.write_note[0])
                    ? tr(" — nur lesen: %1").arg(QString::fromUtf8(v.write_note))
                    : tr(" — nur lesen");
            ui->comboVersion->addItem(text, QString::fromUtf8(v.name));
            if (v.is_write_default)
                ui->comboVersion->setCurrentIndex(ui->comboVersion->count() - 1);
        }
    }
    ui->comboVersion->blockSignals(false);
    const bool gibtWelche = ui->comboVersion->count() > 0;
    ui->comboVersion->setVisible(gibtWelche);
    ui->labelVersion->setVisible(gibtWelche);
}

void FormatTab::onSystemChanged(int index)
{
    fuelleFormate(ui->comboSystem->itemData(index).toString());
}

void FormatTab::onFormatChanged(int /*index*/)
{
    fuelleVarianten(getSelectedFormat());
    applyPluginCapabilities();
    /* MF-1233: die Faehigkeiten haengen am Format. */
    applyCopyPlan();
}

void FormatTab::onVersionChanged(int /*index*/)
{
    applyCopyPlan();
}

QString FormatTab::getSelectedFormat() const
{
    return ui->comboFormat->currentData().toString();
}

bool FormatTab::waehleFormat(const QString &name)
{
    if (name.isEmpty()) {
        ui->comboSystem->setCurrentIndex(0);
        return true;
    }
    for (int s = 0; s < ui->comboSystem->count(); s++) {
        ui->comboSystem->setCurrentIndex(s);
        const int f = ui->comboFormat->findData(name);
        if (f >= 0) {
            ui->comboFormat->setCurrentIndex(f);
            return true;
        }
    }
    ui->comboSystem->setCurrentIndex(0);
    return false;
}

// ============================================================================
// Faehigkeits-Tor (MF-661/1320)
// ============================================================================
//
// Die Zuordnung Element -> Merkmal ist eine FESTLEGUNG und steht an einer
// Stelle. Im neuen Formular gibt es keine Fluss-, PLL- oder Schutzgruppe
// mehr; das Merkmal „Flux" haengt an den zwei Flussfeldern selbst.

namespace {
struct ElementMerkmal { const char *element; const char *merkmal; };
const ElementMerkmal kZuordnung[] = {
    { "groupWrite",            "Write" },
    { "checkIncludeRawFlux",   "Flux"  },
    { "checkReadBetweenIndex", "Flux"  },
};
} // namespace

void FormatTab::applyPluginCapabilities()
{
    const QString format = getSelectedFormat();
    const uft_format_plugin_t *plugin = format.isEmpty()
        ? nullptr : uft_get_format_plugin_by_name(format.toUtf8().constData());

    m_gruppenTorFehlendeAnker = 0;
    for (const ElementMerkmal &z : kZuordnung) {
        QWidget *w = findChild<QWidget *>(QString::fromLatin1(z.element));
        if (!w) {
            m_gruppenTorFehlendeAnker++;
            qWarning("[FormatTab] MF-1320: Faehigkeits-Tor findet seinen Anker "
                     "'%s' nicht (Merkmal \"%s\").", z.element, z.merkmal);
            continue;
        }
        const bool frei = !istUnverdrahtet(w);
        QLabel *besch = beschriftungVon(w);
        /* Kein Plugin: NICHTS ausblenden — auf Unwissen zu verstecken
         * naehme Funktion wegen eines Nachschlagefehlers von uns. */
        const uft_control_visibility_t sicht = plugin
            ? uft_plugin_control_visibility(plugin, z.merkmal) : UFT_CONTROL_SHOW;
        const bool zeigen = (sicht != UFT_CONTROL_HIDE);
        w->setVisible(zeigen);
        if (besch) besch->setVisible(zeigen);
        w->setEnabled(zeigen && frei);
        if (sicht == UFT_CONTROL_SHOW_LIMITED && frei) {
            const char *note = uft_plugin_feature_note(plugin, z.merkmal);
            w->setToolTip(note && *note
                ? tr("Eingeschränkt: %1").arg(QString::fromUtf8(note))
                : tr("Eingeschränkt unterstützt."));
        }
    }
}

// ============================================================================
// Kopierplan
// ============================================================================

/* Nicht verfuegbare Modi sind SICHTBAR und unanwaehlbar, mit Grund im
 * Kurzhinweis (MF-666/1237). */
void FormatTab::fuelleProfile()
{
    const QString fmt = getSelectedFormat();
    /* MF-1618: bei „Automatisch" ist das Format UNBEKANNT, nicht „kann
     * nichts". Der alte Reiter oeffnete stets mit einem Format, und die
     * Sperre „FluxCopy ohne Flussformat" (MF-1293) galt einem bekannten
     * Format ohne Fluss. Auf Unwissen wird nichts gesperrt (MF-1320) —
     * sonst ist „Kopiermodus zuerst" (Vorlage des Eigentuemers) fuer fuenf
     * der Modi unmoeglich. Gesperrt wird, sobald ein Format feststeht. */
    const uint32_t caps = copyPlanCapsBekannt() ? copyPlanCaps() : 0xFFFFFFFFu;
    const QByteArray f = fmt.toUtf8();
    for (size_t i = 0; i < uft_copy_profile_count(); i++) {
        const uft_copy_profile_t *p = uft_copy_profile(i);
        if (!p || !p->id) continue;
        auto *b = findChild<QPushButton *>(QStringLiteral("btnModus_") +
                                           QString::fromUtf8(p->id));
        if (!b) continue;
        const char *grund = nullptr;
        const bool ok = uft_copy_profile_available(
            p, caps, fmt.isEmpty() ? nullptr : f.constData(), &grund);
        b->setEnabled(ok);
        b->setToolTip(ok ? QString::fromUtf8(p->text ? p->text : p->name)
                         : tr("nicht verfügbar: %1")
                               .arg(QString::fromUtf8(grund ? grund : "")));
        b->setAutoExclusive(false);
        b->setChecked(ok && !m_profil.isEmpty() && m_profil == QString::fromUtf8(p->id));
        b->setAutoExclusive(true);
    }
    ui->btnModus_custom->setAutoExclusive(false);
    ui->btnModus_custom->setChecked(m_profil.isEmpty());
    ui->btnModus_custom->setAutoExclusive(true);
}

/* Eigentuemer-Antwort 1: die Achsen sind nur im Modus „Benutzerdefiniert"
 * editierbar. */
void FormatTab::setPlanFrei(bool frei)
{
    m_planFrei = frei;
    for (QComboBox *b : { ui->comboPlanLevel, ui->comboPlanStrategy,
                          ui->comboPlanPreserve, ui->comboPlanPolicy,
                          ui->comboPlanTrackMode, ui->comboPlanFileSpecial,
                          ui->comboPlanVote, ui->comboPlanExact })
        b->setEnabled(frei);
}

void FormatTab::wendeProfilAn(const QString &id)
{
    if (id.isEmpty()) {
        m_profil.clear();
        setPlanFrei(true);
        return;
    }
    const uft_copy_profile_t *p = uft_copy_profile_by_id(id.toUtf8().constData());
    if (!p) return;
    m_profil = id;
    m_basisProfil = id;
    auto setze = [](QComboBox *b, int wert) {
        const int i = b->findData(wert);
        if (i >= 0) { b->blockSignals(true); b->setCurrentIndex(i); b->blockSignals(false); }
    };
    setze(ui->comboPlanLevel,       p->plan.level);
    setze(ui->comboPlanStrategy,    p->plan.strategy);
    setze(ui->comboPlanPreserve,    p->plan.preservation);
    setze(ui->comboPlanPolicy,      p->plan.policy);
    setze(ui->comboPlanTrackMode,   p->plan.track_mode);
    setze(ui->comboPlanFileSpecial, p->plan.file_special);
    setze(ui->comboPlanVote,        p->plan.vote);
    setze(ui->comboPlanExact,       p->plan.exact_kind);
    setPlanFrei(false);
}

/* MF-1306: ein verborgenes Feld steuert keinen Wert bei. `isHidden()` auch
 * an den Vorfahren — wer eine Gruppe versteckt, setzt die Flagge nur dort. */
bool FormatTab::istVerborgen(const QWidget *w) const
{
    for (const QWidget *p = w; p && p != this; p = p->parentWidget())
        if (p->isHidden()) return true;
    return false;
}

uft_copy_plan_t FormatTab::copyPlan() const
{
    uft_copy_plan_t p = uft_copy_plan_default();
    auto lies = [this](QComboBox *b, int vorgabe) {
        if (!b || b->currentIndex() < 0 || istVerborgen(b)) return vorgabe;
        const QVariant v = b->currentData();
        return v.isValid() ? v.toInt() : vorgabe;
    };
    auto gesetzt = [this](QCheckBox *c) {
        return c && !istVerborgen(c) && c->isChecked();
    };
    p.level        = static_cast<uft_copy_level_t>(lies(ui->comboPlanLevel, UFT_COPY_SECTOR));
    p.strategy     = static_cast<uft_read_strategy_t>(lies(ui->comboPlanStrategy, UFT_READ_STANDARD));
    p.preservation = static_cast<uft_preservation_t>(lies(ui->comboPlanPreserve, UFT_PRESERVE_LOGICAL));
    p.policy       = static_cast<uft_copy_policy_t>(lies(ui->comboPlanPolicy, UFT_POLICY_NORMAL));
    p.track_mode   = static_cast<uft_track_mode_t>(lies(ui->comboPlanTrackMode, UFT_TRACK_DECODED));
    p.file_special = static_cast<uft_file_special_t>(lies(ui->comboPlanFileSpecial, UFT_FILE_GENERIC));
    p.gcr          = UFT_GCR_COMMODORE;   /* kein Bedienelement (P3-522) */
    p.vote         = static_cast<uft_vote_method_t>(lies(ui->comboPlanVote, UFT_VOTE_STRICT_MAJORITY));
    p.exact_kind   = static_cast<uft_bitexact_kind_t>(lies(ui->comboPlanExact, UFT_EXACT_SECTOR));
    /* SHA-256 steht fest und wird gesetzt, nicht aus dem gesperrten Feld
     * gelesen. */
    p.hashes = (uint32_t)UFT_HASH_SHA256;
    if (gesetzt(ui->checkHashSha512)) p.hashes |= (uint32_t)UFT_HASH_SHA512;
    if (gesetzt(ui->checkCRC32))      p.hashes |= (uint32_t)UFT_HASH_CRC32;
    /* MF-1619: die Zahl der Leseversuche — nur, wenn das Feld sichtbar
     * und frei ist. Legt die Lesestrategie `read.retries` fest, sperrt
     * das Parameter-Tor das Feld, und dann entscheidet die Strategie. */
    if (!istVerborgen(ui->spinMaxRetries) && ui->spinMaxRetries->isEnabled()
        && !istUnverdrahtet(ui->spinMaxRetries)) {
        p.read_retries_gesetzt = true;
        p.read_retries = (uint32_t)ui->spinMaxRetries->value();
    }
    return p;
}

int FormatTab::leseUmdrehungen() const
{
    return ui->spinRevolutions->value();
}

bool FormatTab::copyPlanCapsBekannt() const
{
    const QString fmt = getSelectedFormat();
    return !fmt.isEmpty() &&
           uft_get_format_plugin_by_name(fmt.toUtf8().constData()) != nullptr;
}

/* Was das gewaehlte Format zusagt — und NUR das. Dateisystem, GCR und
 * CBM-BAM kann dieser Reiter nicht messen: er kennt kein geoeffnetes
 * Abbild. */
uint32_t FormatTab::copyPlanCaps() const
{
    uint32_t caps = 0;
    const QString fmt = getSelectedFormat();
    if (fmt.isEmpty()) return caps;
    const uft_format_plugin_t *pl = uft_get_format_plugin_by_name(fmt.toUtf8().constData());
    if (!pl) return caps;
    if (pl->capabilities & UFT_FORMAT_CAP_FLUX)      caps |= UFT_CAP_FLUX_IO;
    if (pl->capabilities & UFT_FORMAT_CAP_TIMING)    caps |= UFT_CAP_TIMING;
    if (pl->capabilities & UFT_FORMAT_CAP_WEAK_BITS) caps |= UFT_CAP_WEAK_BITS;
    if (pl->capabilities & UFT_FORMAT_CAP_MULTI_REV) caps |= UFT_CAP_MULTI_REV;
    return caps;
}

static uint32_t nicht_messbar()
{
    return (uint32_t)UFT_CAP_BITSTREAM_IO | (uint32_t)UFT_CAP_FILESYSTEM
         | (uint32_t)UFT_CAP_CBM_BAM      | (uint32_t)UFT_CAP_GCR;
}

void FormatTab::zeigeCaps()
{
    if (getSelectedFormat().isEmpty()) {
        ui->labelPlanCaps->setText(
            tr("Format: Automatisch — die Fähigkeiten stehen erst fest, "
               "wenn ein Abbild geöffnet ist. Ausgeblendet wird deshalb "
               "nichts."));
        return;
    }
    const uint32_t caps = copyPlanCaps();
    const uint32_t offen = nicht_messbar();
    QStringList traegt, traegtNicht, unbekannt;
    for (size_t i = 0; i < uft_copy_cap_count(); i++) {
        const uft_copy_caps_t c = uft_copy_cap_at(i);
        const char *n = uft_copy_cap_name(c);
        if (!n) continue;
        const QString name = QString::fromUtf8(n);
        if (offen & (uint32_t)c)     unbekannt   << name;
        else if (caps & (uint32_t)c) traegt      << name;
        else                         traegtNicht << name;
    }
    QStringList z;
    z << (traegt.isEmpty() ? tr("Das Format trägt: —")
                           : tr("Das Format trägt: %1").arg(traegt.join(", ")));
    if (!traegtNicht.isEmpty()) z << tr("trägt nicht: %1").arg(traegtNicht.join(", "));
    if (!unbekannt.isEmpty())
        z << tr("nicht feststellbar (dieser Reiter öffnet kein Abbild): %1")
                 .arg(unbekannt.join(", "));
    ui->labelPlanCaps->setText(z.join(QStringLiteral("\n")));
}

/* „Nach dem Wandeln nachpruefen" ist eine SICHT auf die Richtlinie: der
 * Kern kennt die Nachpruefung nur ueber `plan.policy` (NORMAL heisst ohne,
 * VERIFY und EVIDENCE heissen mit, uft_copy_plan.c). Ein eigenes Feld
 * daneben waere eine zweite Wahrheit (MF-1177). */
void FormatTab::verifyNachziehen(const uft_copy_plan_t &plan)
{
    ui->checkVerify->blockSignals(true);
    ui->checkVerify->setChecked(plan.policy != UFT_POLICY_NORMAL);
    ui->checkVerify->blockSignals(false);
    ui->checkVerify->setEnabled(m_planFrei);
}

void FormatTab::onVerifyToggled(bool an)
{
    const int jetzt = ui->comboPlanPolicy->currentData().toInt();
    int neu = jetzt;
    if (an && jetzt == UFT_POLICY_NORMAL) neu = UFT_POLICY_VERIFY;
    if (!an) neu = UFT_POLICY_NORMAL;
    const int i = ui->comboPlanPolicy->findData(neu);
    if (i >= 0) ui->comboPlanPolicy->setCurrentIndex(i);   /* -> applyCopyPlan */
}

void FormatTab::applyCopyPlan()
{
    const uint32_t caps = copyPlanCaps();
    const bool capsBekannt = copyPlanCapsBekannt();
    fuelleProfile();

    {   /* Aktueller Modus */
        const uft_copy_profile_t *pr = m_profil.isEmpty()
            ? nullptr : uft_copy_profile_by_id(m_profil.toUtf8().constData());
        const uft_copy_profile_t *basis =
            uft_copy_profile_by_id(m_basisProfil.toUtf8().constData());
        QString t;
        if (pr)
            t = QStringLiteral("<b>%1</b><br/>%2")
                    .arg(QString::fromUtf8(pr->name).toHtmlEscaped(),
                         QString::fromUtf8(pr->text ? pr->text : "").toHtmlEscaped());
        else if (basis)
            t = tr("<b>Benutzerdefiniert</b><br/>ausgehend von %1. Die Achsen "
                   "des Kopierplans sind frei einstellbar.")
                    .arg(QString::fromUtf8(basis->name).toHtmlEscaped());
        else
            t = tr("<b>Benutzerdefiniert</b>");
        ui->labelProfileText->setText(t);
    }

    /* MF-1234: „Automatisch" VOR jeder Regel aufloesen. */
    const uft_copy_plan_t gewaehlt = copyPlan();
    const uft_copy_plan_t plan = uft_copy_plan_resolve(&gewaehlt, caps);

    /* Befunde — harte zuerst, weil sie den Auftrag sperren. Bei
     * unbekanntem Format (MF-1618) wird mit allen Faehigkeiten geprueft:
     * ein „so nicht ausfuehrbar" wegen einer Faehigkeit, die niemand
     * gemessen hat, waere erfunden. Was dann bleibt, sind die Befunde des
     * Plans selbst; die Faehigkeitspruefung folgt mit dem Format. */
    uft_copy_finding_t f[16];
    const size_t n = uft_copy_plan_check(&plan, capsBekannt ? caps : 0xFFFFFFFFu, f, 16);
    const size_t gezeigt = (n > 16) ? 16 : n;
    QStringList hart, weich;
    for (size_t i = 0; i < gezeigt; i++)
        (f[i].hard ? hart : weich) << QString::fromUtf8(f[i].text ? f[i].text : "");
    QStringList befunde;
    if (!capsBekannt)
        befunde << tr("Format: Automatisch — ob das Abbild trägt, was der "
                      "Modus verlangt, wird geprüft, sobald das Format "
                      "feststeht.");
    if (gewaehlt.level == UFT_COPY_AUTO) {
        const char *a = uft_copy_level_name(plan.level);
        befunde << (capsBekannt
            ? tr("Automatisch aufgelöst zu: %1").arg(QString::fromUtf8(a ? a : "?"))
            : tr("Ebene Automatisch: wird aufgelöst, sobald das Format feststeht."));
    }
    if (!hart.isEmpty()) befunde << tr("So nicht ausführbar: ") + hart.join(QStringLiteral("  "));
    if (!weich.isEmpty()) befunde << tr("Hinweis: ") + weich.join(QStringLiteral("  "));
    if (n > gezeigt) befunde << tr("(%1 weitere Befunde)").arg(n - gezeigt);
    ui->labelPlanFindings->setText(befunde.isEmpty() ? tr("Keine Befunde.")
                                                     : befunde.join(QStringLiteral("\n")));

    uft_copy_enforced_t e[96];
    const size_t m = uft_copy_plan_enforced(&plan, e, 96);
    const size_t emax = (m > 96) ? 96 : m;
    QStringList zeilen;
    for (size_t i = 0; i < emax; i++)
        zeilen << QStringLiteral("%1 = %2").arg(QString::fromUtf8(e[i].param),
                                                QString::fromUtf8(e[i].value));
    ui->labelPlanForced->setText(zeilen.isEmpty()
        ? tr("Der Plan erzwingt nichts.")
        : tr("Der Plan setzt fest: ") + zeilen.join(QStringLiteral(", ")));

    /* Die Feinheiten: sichtbar nur, wo sie etwas bedeuten (MF-1235). */
    struct { QWidget *w; bool zeigen; } planZeilen[] = {
        { ui->comboPlanTrackMode,   plan.level == UFT_COPY_TRACK },
        { ui->comboPlanFileSpecial, plan.level == UFT_COPY_FILE },
        { ui->comboPlanVote,        plan.strategy == UFT_READ_CONSENSUS },
        { ui->comboPlanExact,       plan.preservation == UFT_PRESERVE_BIT_EXACT },
    };
    for (const auto &z : planZeilen) {
        z.w->setVisible(z.zeigen);
        if (QLabel *lab = beschriftungVon(z.w)) lab->setVisible(z.zeigen);
    }

    /* „Commodore BAM" nur, wenn das Format BAM und Dateisystem zusagt. */
    {
        const bool bam = (caps & (uint32_t)UFT_CAP_CBM_BAM) &&
                         (caps & (uint32_t)UFT_CAP_FILESYSTEM);
        QComboBox *fs = ui->comboPlanFileSpecial;
        const int idx = fs->findData(UFT_FILE_BAM);
        fs->blockSignals(true);
        if (idx >= 0 && !bam) {
            if (fs->currentIndex() == idx) fs->setCurrentIndex(fs->findData(UFT_FILE_GENERIC));
            fs->removeItem(idx);
        } else if (idx < 0 && bam) {
            const char *nm = uft_copy_file_special_name(UFT_FILE_BAM);
            fs->insertItem(UFT_FILE_BAM, QString::fromUtf8(nm ? nm : "BAM"), UFT_FILE_BAM);
        }
        fs->blockSignals(false);
    }

    /* Das Parameter-Tor: Widget -> Planparameter, an EINER Stelle.
     *   FORBIDDEN/HIDDEN -> ausblenden, FORCED/READONLY -> sperren.
     * Ein nicht verdrahtetes Feld gibt das Tor nie frei — es steuert nur,
     * ob es zu sehen ist. */
    struct { const char *param; QWidget *w; } bindung[] = {
        { "read.revolutions",         ui->spinRevolutions },
        { "encoding",                 ui->comboEncoding },
        { "rpm",                      ui->comboRpm },
        { "read.retries",             ui->spinMaxRetries },
        { "preserve_weak_bits",       ui->checkWeakBits },
        { "evidence.hash_algorithms", ui->checkHashSha512 },
        { "source.format",            ui->comboFormat },
    };
    /* MF-1320: auf UNWISSEN ueber das Format wird nichts ausgeblendet.
     * MF-1618 BERICHTIGT die Umsetzung: bisher wurde bei unbekanntem
     * Format JEDES HIDDEN/FORBIDDEN aufgehoben — auch das der EBENE, das
     * mit dem Format nichts zu tun hat (auf der Dateiebene ist
     * `read.revolutions` verboten, egal welches Format). Jetzt fragt das
     * Tor bei unbekanntem Format mit ALLEN Faehigkeiten: kein Feld faellt
     * wegen einer Faehigkeit weg, die niemand gemessen hat, die Regeln der
     * Ebene gelten weiter. Aufgefallen ist es, weil der neue Reiter auf
     * „Automatisch" oeffnet statt zufaellig auf D64. */
    const uint32_t torCaps = capsBekannt ? caps : 0xFFFFFFFFu;
    m_parameterTorFehlendeAnker = 0;
    for (const auto &b : bindung) {
        if (!b.w) { m_parameterTorFehlendeAnker++; continue; }
        const char *wert = nullptr;
        const uft_copy_pstate_t z =
            uft_copy_param_state_caps(&plan, torCaps, b.param, &wert);
        const bool sichtbar = !(z == UFT_PSTATE_FORBIDDEN || z == UFT_PSTATE_HIDDEN);
        if (QLabel *besch = beschriftungVon(b.w)) besch->setVisible(sichtbar);
        b.w->setVisible(sichtbar);
        if (!sichtbar) continue;
        const bool frei = !istUnverdrahtet(b.w);
        switch (z) {
        case UFT_PSTATE_FORCED:
            b.w->setEnabled(false);
            if (frei) b.w->setToolTip(tr("Vom Kopierplan festgelegt: %1")
                                          .arg(QString::fromUtf8(wert ? wert : "")));
            break;
        case UFT_PSTATE_READONLY:
            b.w->setEnabled(false);
            if (frei) b.w->setToolTip(tr("Gemessener Quellwert, nicht einstellbar."));
            break;
        default:
            b.w->setEnabled(frei);
            if (frei) b.w->setToolTip(QString());
            break;
        }
    }

    /* Der Hashsatz gehoert zur Beweisrichtlinie (MF-1293) — die GRUPPE wird
     * versteckt, nicht die Haken. */
    ui->unter_nachweis_Pruefsummen->setVisible(plan.policy == UFT_POLICY_EVIDENCE);

    verifyNachziehen(plan);
    zeigeCaps();
    if (!ui->textPlanJson->isHidden())
        ui->textPlanJson->setPlainText(planJson());
}

void FormatTab::onCopyPlanChanged(int) { applyCopyPlan(); }
void FormatTab::onCopyPlanToggled(bool) { applyCopyPlan(); }

QString FormatTab::planJson() const
{
    const uft_copy_plan_t roh = copyPlan();
    const uft_copy_plan_t p   = uft_copy_plan_resolve(&roh, copyPlanCaps());
    const size_t n = uft_copy_plan_to_json(&p, nullptr, 0);
    if (n == 0) return QString();
    QByteArray b(int(n) + 1, '\0');
    (void)uft_copy_plan_to_json(&p, b.data(), (size_t)b.size());
    return QString::fromUtf8(b.constData());
}

void FormatTab::onPlanJsonToggled(bool checked)
{
    ui->textPlanJson->setVisible(checked);
    if (checked) ui->textPlanJson->setPlainText(planJson());
    ui->btnPlanJson->setText(checked ? tr("JSON verbergen") : tr("Plan als JSON zeigen"));
}

void FormatTab::onPlanJsonKopieren()
{
    if (QClipboard *c = QGuiApplication::clipboard()) c->setText(planJson());
}

void FormatTab::onPlanJsonSichern()
{
    const QString pfad = QFileDialog::getSaveFileName(
        this, tr("Kopierplan sichern"), QStringLiteral("kopierplan.json"),
        tr("JSON (*.json);;Alle Dateien (*)"));
    if (pfad.isEmpty()) return;
    QFile f(pfad);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Kopierplan sichern"),
                             tr("Die Datei ließ sich nicht schreiben:\n%1").arg(f.errorString()));
        return;
    }
    const QByteArray roh = planJson().toUtf8();
    if (f.write(roh) != roh.size())
        QMessageBox::warning(this, tr("Kopierplan sichern"),
                             tr("Es wurden nicht alle Daten geschrieben:\n%1").arg(f.errorString()));
    f.close();
}

// ============================================================================
// .uftsetup (MF-1618)
// ============================================================================

namespace {
struct AchsenFeld {
    const char *schluessel;
    int anzahl;
    const char *(*name)(int);
};
const AchsenFeld kAchsen[] = {
    { "level",        UFT_COPY_LEVEL_N,    nameLevel },
    { "strategy",     UFT_READ_STRATEGY_N, nameStrategy },
    { "preservation", UFT_PRESERVE_N,      namePreserve },
    { "policy",       UFT_POLICY_N,        namePolicy },
    { "track_mode",   UFT_TRACK_MODE_N,    nameTrackMode },
    { "file_special", UFT_FILE_SPECIAL_N,  nameFileSpecial },
    { "vote",         UFT_VOTE_N,          nameVote },
    { "exact_kind",   UFT_EXACT_N,         nameExact },
};
/* Die wirksamen Einzelwerte eines Setups. Nur Felder mit Leser — ein
 * Setup soll nicht wieder zur Ablage fuer Werte werden, die nichts tun. */
const char *const kSetupWerte[] = { "spinRevolutions", "spinMaxRetries" };
} // namespace

static QComboBox *achsenFeld(Ui::TabFormat *ui, int i)
{
    QComboBox *f[] = { ui->comboPlanLevel, ui->comboPlanStrategy,
                       ui->comboPlanPreserve, ui->comboPlanPolicy,
                       ui->comboPlanTrackMode, ui->comboPlanFileSpecial,
                       ui->comboPlanVote, ui->comboPlanExact };
    return f[i];
}

QString FormatTab::setupOrdner()
{
    const QString d = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                      + QStringLiteral("/setups");
    QDir().mkpath(d);
    return d;
}

QJsonObject FormatTab::setupAlsJson() const
{
    QJsonObject o;
    o[QStringLiteral("uftsetup")] = 1;
    o[QStringLiteral("modus")] = m_profil;
    o[QStringLiteral("basis")] = m_basisProfil;
    QJsonObject plan;
    for (int i = 0; i < int(sizeof kAchsen / sizeof kAchsen[0]); i++) {
        const QComboBox *b = achsenFeld(ui, i);
        const char *n = kAchsen[i].name(b->currentData().toInt());
        plan[QString::fromLatin1(kAchsen[i].schluessel)] = QString::fromUtf8(n ? n : "");
    }
    o[QStringLiteral("plan")] = plan;
    QJsonArray hashes;
    if (ui->checkHashSha512->isChecked()) hashes << QStringLiteral("sha512");
    if (ui->checkCRC32->isChecked())      hashes << QStringLiteral("crc32");
    o[QStringLiteral("hashes")] = hashes;
    o[QStringLiteral("system")]   = ui->comboSystem->currentData().toString();
    o[QStringLiteral("format")]   = getSelectedFormat();
    o[QStringLiteral("variante")] = ui->comboVersion->currentData().toString();
    QJsonObject werte;
    for (const char *name : kSetupWerte) {
        const QString k = QString::fromLatin1(name);
        if (auto *s = findChild<QSpinBox *>(k)) werte[k] = s->value();
    }
    o[QStringLiteral("werte")] = werte;
    return o;
}

bool FormatTab::setupAnwenden(const QJsonObject &o, QString *fehler,
                              QStringList *uebergangen)
{
    if (o.value(QStringLiteral("uftsetup")).toInt() != 1) {
        if (fehler) *fehler = tr("Keine .uftsetup-Datei der Fassung 1.");
        return false;
    }
    /* 1. Kopiermodus */
    const QString modus = o.value(QStringLiteral("modus")).toString();
    if (!modus.isEmpty()) {
        if (!uft_copy_profile_by_id(modus.toUtf8().constData())) {
            if (fehler) *fehler = tr("Unbekannter Kopiermodus „%1“.").arg(modus);
            return false;
        }
        wendeProfilAn(modus);
    } else {
        const QString basis = o.value(QStringLiteral("basis")).toString();
        if (!basis.isEmpty() && uft_copy_profile_by_id(basis.toUtf8().constData()))
            wendeProfilAn(basis);
        m_profil.clear();
        setPlanFrei(true);
        const QJsonObject plan = o.value(QStringLiteral("plan")).toObject();
        for (int i = 0; i < int(sizeof kAchsen / sizeof kAchsen[0]); i++) {
            const QString s = plan.value(QString::fromLatin1(kAchsen[i].schluessel)).toString();
            if (s.isEmpty()) continue;
            for (int v = 0; v < kAchsen[i].anzahl; v++) {
                const char *n = kAchsen[i].name(v);
                if (n && s == QString::fromUtf8(n)) {
                    QComboBox *b = achsenFeld(ui, i);
                    const int idx = b->findData(v);
                    if (idx >= 0) { b->blockSignals(true); b->setCurrentIndex(idx); b->blockSignals(false); }
                    break;
                }
            }
        }
    }
    /* 2. System, Format, Variante */
    const QString format = o.value(QStringLiteral("format")).toString();
    if (!waehleFormat(format) && uebergangen)
        *uebergangen << tr("Format „%1“ (nicht registriert)").arg(format);
    const QString variante = o.value(QStringLiteral("variante")).toString();
    if (!variante.isEmpty()) {
        const int v = ui->comboVersion->findData(variante);
        if (v >= 0) ui->comboVersion->setCurrentIndex(v);
        else if (uebergangen) *uebergangen << tr("Variante „%1“").arg(variante);
    }
    /* 3. Sichtbarkeit neu berechnen */
    applyCopyPlan();
    /* 4. Werte — nur in Felder, die hier sichtbar und verdrahtet sind */
    const QJsonArray hashes = o.value(QStringLiteral("hashes")).toArray();
    ui->checkHashSha512->setChecked(hashes.contains(QStringLiteral("sha512")));
    ui->checkCRC32->setChecked(hashes.contains(QStringLiteral("crc32")));
    const QJsonObject werte = o.value(QStringLiteral("werte")).toObject();
    for (auto it = werte.begin(); it != werte.end(); ++it) {
        QWidget *w = findChild<QWidget *>(it.key());
        auto *s = qobject_cast<QSpinBox *>(w);
        if (!s || istVerborgen(s) || istUnverdrahtet(s) || !s->isEnabled()) {
            if (uebergangen) *uebergangen << it.key();
            continue;
        }
        s->setValue(it.value().toInt());
    }
    applyCopyPlan();
    return true;
}

void FormatTab::onSetupLaden()
{
    const QString pfad = QFileDialog::getOpenFileName(
        this, tr("Setup laden"), setupOrdner(), tr("UFT-Setup (*.uftsetup)"));
    if (pfad.isEmpty()) return;
    QFile f(pfad);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Setup laden"),
                             tr("Die Datei ließ sich nicht öffnen:\n%1").arg(f.errorString()));
        return;
    }
    QJsonParseError pe;
    const QJsonDocument d = QJsonDocument::fromJson(f.readAll(), &pe);
    if (!d.isObject()) {
        QMessageBox::warning(this, tr("Setup laden"),
                             tr("Kein gültiges Setup:\n%1").arg(pe.errorString()));
        return;
    }
    QString fehler;
    QStringList uebergangen;
    if (!setupAnwenden(d.object(), &fehler, &uebergangen)) {
        QMessageBox::warning(this, tr("Setup laden"), fehler);
        return;
    }
    if (!uebergangen.isEmpty())
        QMessageBox::information(this, tr("Setup laden"),
            tr("Geladen. Nicht angewandt, weil hier ohne Wirkung oder nicht "
               "vorhanden:\n%1").arg(uebergangen.join(QStringLiteral("\n"))));
}

void FormatTab::onSetupSpeichern()
{
    QString pfad = QFileDialog::getSaveFileName(
        this, tr("Setup speichern"), setupOrdner() + QStringLiteral("/setup.uftsetup"),
        tr("UFT-Setup (*.uftsetup)"));
    if (pfad.isEmpty()) return;
    if (!pfad.endsWith(QStringLiteral(".uftsetup"))) pfad += QStringLiteral(".uftsetup");
    QFile f(pfad);
    const QByteArray roh = QJsonDocument(setupAlsJson()).toJson();
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(roh) != roh.size())
        QMessageBox::warning(this, tr("Setup speichern"),
                             tr("Die Datei ließ sich nicht schreiben:\n%1").arg(f.errorString()));
}

void FormatTab::onSetupReset()
{
    wendeProfilAn(QStringLiteral("standardcopy"));
    ui->comboSystem->setCurrentIndex(0);
    ui->checkHashSha512->setChecked(false);
    ui->checkCRC32->setChecked(false);
    ui->spinRevolutions->setValue(3);
    applyCopyPlan();
}

/* Die alte presets.json: je Benutzer-Voreinstellung ein .uftsetup. Was in
 * ihr steht und heute keinen Leser hat (Spuren, Halbspuren, GCR-Art,
 * Schutzerkennung, Timing, PLL, Kopiermodus alter Art), wird NICHT
 * uebernommen und im Setup unter "nicht_uebernommen" genannt — still
 * verschwinden soll es nicht. */
int FormatTab::uebernimmAltePresets(const QString &presetsPfad,
                                    const QString &zielOrdner,
                                    QStringList *meldungen)
{
    QFile f(presetsPfad);
    if (!f.open(QIODevice::ReadOnly)) return 0;
    const QJsonDocument d = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!d.isObject()) return 0;
    QDir().mkpath(zielOrdner);
    int n = 0;
    const QJsonObject root = d.object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        const QJsonObject p = it.value().toObject();
        QJsonObject s;
        s[QStringLiteral("uftsetup")] = 1;
        s[QStringLiteral("modus")] = QStringLiteral("standardcopy");
        s[QStringLiteral("format")] = p.value(QStringLiteral("format")).toString();
        s[QStringLiteral("variante")] = p.value(QStringLiteral("version")).toString();
        s[QStringLiteral("herkunft")] = QStringLiteral("presets.json");
        QJsonObject weg;
        for (const char *k : { "system", "encoding", "tracks", "heads", "density",
                               "halfTracks", "preserveTiming", "adaptivePLL",
                               "copyMode", "gcrType", "detectProtection" })
            if (p.contains(QLatin1String(k))) weg[QLatin1String(k)] = p.value(QLatin1String(k));
        s[QStringLiteral("nicht_uebernommen")] = weg;
        QString name = it.key();
        name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9 ._-]")),
                     QStringLiteral("_"));
        if (name.trimmed().isEmpty()) name = QStringLiteral("voreinstellung");
        QString ziel = zielOrdner + QLatin1Char('/') + name + QStringLiteral(".uftsetup");
        for (int k = 2; QFile::exists(ziel); k++)
            ziel = zielOrdner + QLatin1Char('/') + name + QStringLiteral("_%1.uftsetup").arg(k);
        QFile o(ziel);
        const QByteArray roh = QJsonDocument(s).toJson();
        if (!o.open(QIODevice::WriteOnly) || o.write(roh) != roh.size()) return -1;
        o.close();
        if (meldungen) *meldungen << QStringLiteral("%1 -> %2").arg(it.key(), ziel);
        n++;
    }
    return n;
}

// ============================================================================
// IPF / CT-Raw: Hilfsprogramm und SPS-Bibliothek (MF-1613/1618)
// ============================================================================
//
// Der Pfad wirkt ueber die Umgebung, die `uft_ipf_helper.c` liest
// (UFT_IPF_HELPER) und die das Hilfsprogramm an seine Bibliothek
// weitergibt (UFT_CAPSIMG_LIB). Das Feld zeigt den WIRKSAMEN Wert: ist
// nichts gespeichert, aber die Umgebung gesetzt, steht dort der Wert der
// Umgebung.

void FormatTab::helferAusEinstellungen()
{
    QSettings s;
    s.beginGroup(QLatin1String(kWerkzeugGruppe));
    const QString h = s.value(QStringLiteral("ipfHelfer")).toString();
    const QString l = s.value(QStringLiteral("capsimgBibliothek")).toString();
    s.endGroup();
    if (!h.isEmpty()) qputenv(UFT_IPF_HELPER_ENV, QFile::encodeName(h));
    if (!l.isEmpty()) qputenv(kCapsLibEnv, QFile::encodeName(l));
    ui->editHelperPath->setText(QString::fromLocal8Bit(qgetenv(UFT_IPF_HELPER_ENV)));
    ui->editCapsLibPath->setText(QString::fromLocal8Bit(qgetenv(kCapsLibEnv)));
    helferStatusZeigen();
}

void FormatTab::onHelperPfadGeaendert()
{
    const QString h = ui->editHelperPath->text().trimmed();
    const QString l = ui->editCapsLibPath->text().trimmed();
    QSettings s;
    s.beginGroup(QLatin1String(kWerkzeugGruppe));
    s.setValue(QStringLiteral("ipfHelfer"), h);
    s.setValue(QStringLiteral("capsimgBibliothek"), l);
    s.endGroup();
    if (h.isEmpty()) qunsetenv(UFT_IPF_HELPER_ENV);
    else qputenv(UFT_IPF_HELPER_ENV, QFile::encodeName(h));
    if (l.isEmpty()) qunsetenv(kCapsLibEnv);
    else qputenv(kCapsLibEnv, QFile::encodeName(l));
    helferStatusZeigen();
}

void FormatTab::onHelperSuchen()
{
    const QString p = QFileDialog::getOpenFileName(this, tr("Hilfsprogramm wählen"),
                                                   ui->editHelperPath->text());
    if (p.isEmpty()) return;
    ui->editHelperPath->setText(QDir::toNativeSeparators(p));
    onHelperPfadGeaendert();
}

void FormatTab::onCapsLibSuchen()
{
    const QString p = QFileDialog::getOpenFileName(
        this, tr("SPS-Bibliothek wählen"), ui->editCapsLibPath->text(),
        tr("Bibliothek (*.dll *.so *.so.* *.dylib);;Alle Dateien (*)"));
    if (p.isEmpty()) return;
    ui->editCapsLibPath->setText(QDir::toNativeSeparators(p));
    onHelperPfadGeaendert();
}

void FormatTab::helferStatusZeigen()
{
    const QString h = ui->editHelperPath->text().trimmed();
    const QString l = ui->editCapsLibPath->text().trimmed();
    QString t;
    if (h.isEmpty()) {
        t = tr("Nicht eingerichtet. IPF liest UFT dann mit seinem eigenen Leser; "
               "CT-Raw ist nur über das Hilfsprogramm lesbar.");
    } else if (!QFileInfo(h).isFile()) {
        t = tr("Hilfsprogramm nicht gefunden: %1").arg(h);
    } else {
        const QString dll = l.isEmpty()
            ? QFileInfo(h).absoluteDir().filePath(QStringLiteral("CAPSImg.dll")) : l;
        t = QFileInfo(dll).isFile()
            ? tr("Bereit: IPF und CT-Raw werden über die SPS-Bibliothek gelesen.")
            : tr("Hilfsprogramm gefunden, die SPS-Bibliothek aber nicht (%1). "
                 "Ohne sie sagt das Hilfsprogramm ab, und UFT liest IPF selbst.")
                  .arg(dll);
    }
    ui->labelHelperStatus->setText(t);
}

// ============================================================================
// Hardware-Status (Kopfzeile)
// ============================================================================

void FormatTab::setHardwareVerbunden(bool verbunden)
{
    m_hardwareVerbunden = verbunden;
    ui->labelHardwareStatus->setText(verbunden ? tr("● Hardware verbunden")
                                               : tr("● Keine Hardware verbunden"));
}

void FormatTab::setHardwareGeraet(const QString &name, const QString &firmware)
{
    if (!m_hardwareVerbunden || name.isEmpty()) return;
    ui->labelHardwareStatus->setText(firmware.isEmpty()
        ? tr("● %1").arg(name)
        : tr("● %1 (Firmware %2)").arg(name, firmware));
}
