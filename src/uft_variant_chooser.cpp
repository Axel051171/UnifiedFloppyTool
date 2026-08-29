/**
 * @file uft_variant_chooser.cpp
 * @brief Umsetzung zu uft_variant_chooser.h (MF-666)
 */

#include "uft_variant_chooser.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QVBoxLayout>

extern "C" {
#include "uft/uft_format_plugin.h"
#include "uft/uft_format_probe.h"
}

QString uftAskWriteVariant(QWidget *parent,
                           const QString &zielformat,
                           bool *abgebrochen)
{
    if (abgebrochen) *abgebrochen = false;

    const uft_format_plugin_t *plugin =
        uft_get_format_plugin_by_name(zielformat.toUtf8().constData());
    if (!plugin || !plugin->variants || plugin->variant_count == 0)
        return QString();          // nichts zu waehlen, kein Dialog

    const uft_format_variant_t *standard =
        uft_plugin_default_write_variant(plugin);
    if (!standard) {
        // Varianten ja, schreibbare nein. Das ist kein Dialog, das ist
        // eine Absage — und sie gehoert begruendet.
        QString grund;
        for (size_t i = 0; i < plugin->variant_count; i++) {
            const uft_format_variant_t &v = plugin->variants[i];
            if (v.write_note && v.write_note[0])
                grund += QStringLiteral("\n  %1: %2")
                             .arg(QString::fromUtf8(v.name),
                                  QString::fromUtf8(v.write_note));
        }
        QMessageBox::warning(parent,
            QObject::tr("Speichern"),
            QObject::tr("UFT kann keine Variante von %1 schreiben.%2")
                .arg(zielformat, grund));
        if (abgebrochen) *abgebrochen = true;
        return QString();
    }

    QDialog dlg(parent);
    dlg.setWindowTitle(QObject::tr("Zielvariante"));

    auto *lay = new QVBoxLayout(&dlg);
    lay->addWidget(new QLabel(
        QObject::tr("Unter welcher Variante soll %1 gespeichert werden?")
            .arg(zielformat), &dlg));

    auto *combo = new QComboBox(&dlg);
    int stdIndex = 0;
    int schreibbar = 0;

    for (size_t i = 0; i < plugin->variant_count; i++) {
        const uft_format_variant_t &v = plugin->variants[i];
        const QString name = QString::fromUtf8(v.name ? v.name : "?");

        combo->addItem(v.can_write
                           ? name
                           : QObject::tr("%1 — nicht schreibbar").arg(name));

        const int idx = combo->count() - 1;

        // Nicht schreibbare Varianten bleiben SICHTBAR und werden
        // unanwaehlbar. Sie zu verstecken naehme dem Benutzer die
        // Information, dass es sie gibt — und den Grund gleich mit.
        if (!v.can_write) {
            if (auto *m = qobject_cast<QStandardItemModel *>(combo->model())) {
                if (QStandardItem *it = m->item(idx))
                    it->setEnabled(false);
            }
            combo->setItemData(idx,
                v.write_note ? QString::fromUtf8(v.write_note)
                             : QObject::tr("Von UFT nicht schreibbar."),
                Qt::ToolTipRole);
        } else {
            schreibbar++;
            if (v.description)
                combo->setItemData(idx, QString::fromUtf8(v.description),
                                   Qt::ToolTipRole);
            if (&v == standard) stdIndex = idx;
        }
    }
    combo->setCurrentIndex(stdIndex);
    lay->addWidget(combo);

    // Genau eine Wahl ist keine Wahl. Das Feld bleibt sichtbar — es sagt,
    // WAS geschrieben wird — aber es taeuscht keine Entscheidung vor.
    if (schreibbar <= 1) {
        combo->setEnabled(false);
        lay->addWidget(new QLabel(
            QObject::tr("UFT schreibt derzeit nur diese eine Variante."),
            &dlg));
    }

    auto *knoepfe = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(knoepfe, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(knoepfe, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(knoepfe);

    if (dlg.exec() != QDialog::Accepted) {
        if (abgebrochen) *abgebrochen = true;
        return QString();
    }

    // Der angezeigte Text kann den Zusatz „— nicht schreibbar" tragen;
    // zurueck geht der REINE Name, denn den prueft uftSaveImageAs().
    const int gewaehlt = combo->currentIndex();
    if (gewaehlt >= 0 && (size_t)gewaehlt < plugin->variant_count)
        return QString::fromUtf8(plugin->variants[gewaehlt].name);
    return QString::fromUtf8(standard->name);
}
