/**
 * @file uft_spurausrichtung.h
 * @brief "Track Alignment Issues" hat keinen Erkenner (P3-602, MF-1460)
 *
 * Gemessen: die einzige C64-Schutzanalyse, die laeuft
 * (`ufm_c64_prot_analyze()`), kennt sieben Merkmale — schwache Bits,
 * kurze/lange Spur, Halbspurdaten, illegales GCR, langer Sync,
 * Sektoranomalie — und keines davon ist Spurausrichtung. Einen Erkenner
 * ohne benannte Quelle zu schreiben verbietet die EINFRIER-REGEL.
 *
 * Das Kaestchen steht in ZWEI Formularen (`forms/tab_protection.ui`,
 * `forms/tab_format.ui`) und wurde in keinem gelesen. Es wird darum
 * abgeschaltet und sagt warum, statt einen Haken anzunehmen, der nichts
 * bewirkt. Nicht entfernt: es ist der benannte Platz fuer den Erkenner,
 * der fehlt. Eine Stelle, zwei Aufrufer — und nur ein Header, damit
 * FormatTab nicht ProtectionTab mitbauen muss.
 */
#ifndef UFT_SPURAUSRICHTUNG_H
#define UFT_SPURAUSRICHTUNG_H

#include <QCheckBox>
#include <QCoreApplication>

inline void uftSperreSpurausrichtung(QCheckBox *kasten)
{
    if (!kasten) return;
    kasten->setChecked(false);
    kasten->setEnabled(false);
    kasten->setToolTip(QCoreApplication::translate("UftSpurausrichtung",
        "Nicht verfuegbar: im Baum gibt es keinen Erkenner fuer "
        "Spurausrichtung. Die C64-Schutzanalyse misst schwache Bits, "
        "kurze/lange Spuren, Halbspurdaten, illegales GCR, langen Sync "
        "und Sektoranomalien (P3-602)."));
}

#endif /* UFT_SPURAUSRICHTUNG_H */
