/**
 * @file test_fremde_hand_liest.c
 * @brief Liest UFT Abbilder, die eine FREMDE HAND erzeugt hat? (MF-1020)
 *
 * ── Warum dieser Test die Stufe T1b tragen kann ──────────────────────
 *
 * `docs/VERIFICATION_PLAN.md` sagt: **T1b** heisst, ein kanonisches
 * Fremdwerkzeug hat die Datei erzeugt und UFT liest sie. Bis MF-1020
 * hatten **59 von 88** tier-gefuehrten Formaten ueberhaupt kein
 * Korpus-Abbild — gemessen, nicht geschaetzt. Der Grund war nicht
 * Nachlaessigkeit, sondern dass kein Orakel-Binary verfuegbar schien.
 *
 * **Es lag im Baum.** `tools/uft-scout/work/HxCFloppyEmulator/build/
 * hxcfe.exe` ist gebaut, laeuft, und seine Modulliste nennt fuer viele
 * Formate `RW` — es kann sie also **schreiben**. Damit ist der Kanal
 * *Oracle* nach MF-695 offen, ohne eine Zeile fremden Codes zu
 * uebernehmen: das Werkzeug wird **ausgefuehrt**, nicht portiert.
 *
 * ── Die Eingabe, und warum ihr Inhalt sich selbst benennt ────────────
 *
 * `tests/corpus_free/uft_pc160.img` ist **UFT-eigen** (rechtefrei):
 * 40 Spuren x 16 Sektoren x 256 Byte, und jeder Sektor traegt
 *
 *     Byte 0     Spurnummer
 *     Byte 1     Sektornummer (1-basiert)
 *     Byte 2..6  "UFT-K"
 *     Byte 7..   Muster (Spur*31 + Sektor*7 + Versatz) & 0xFF
 *
 * Das ist der Unterschied zwischen „der Leser hat etwas geliefert" und
 * „der Leser hat die RICHTIGE Stelle getroffen". Ein Abbild aus
 * Nullen oder aus Zufall koennte das nicht entscheiden — genau die
 * Unentscheidbarkeit, die die fuenf Fabrikationen ermoeglicht hat.
 *
 * ── Der Erzeugungsweg, reproduzierbar ───────────────────────────────
 *
 *     hxcfe -finput:uft_pc160.img -foutput:<ziel> -conv:<MODUL>
 *
 * mit den Modulen aus `hxcfe -modulelist`. Die vollen Angaben je Datei
 * (Werkzeug, Klon-Hash, Befehlszeile, Fremdcode-Befund) stehen in
 * `tests/corpus_manifest/manifest.json` — hier NICHT wiederholt, weil
 * eine Angabe an zwei Stellen zweimal driftet (MF-779).
 *
 * ── Was dieser Test NICHT tut ───────────────────────────────────────
 *
 * Er prueft **nicht**, ob UFT dieselben Bytes zurueckgibt wie hxcfe
 * sie hineingeschrieben hat — das waere ein Vergleich zweier
 * Umsetzungen desselben Containers und ist Sache der einzelnen
 * Formattests. Er prueft die Frage, die T1b stellt: **oeffnet UFT eine
 * Datei von fremder Hand, findet es die Spuren, und stehen dort die
 * Nutzdaten, die hineingegeben wurden?**
 *
 * ── Warum MFI hier NICHT steht, obwohl die Datei erzeugt ist ────────
 *
 * `hxcfe_pc160.mfi` liegt im Korpus, und der erste Lauf dieses Tests
 * hat sie geprueft — mit diesem Ergebnis:
 *
 *     mfi  0 von 2 Spuren getroffen; Spur 0: 1 Sektoren,
 *          Byte 0..2 = 78 9C ED
 *
 * `78 9C` ist der **zlib-Kopf**. UFTs MFI-Leser gibt den GEPACKTEN
 * Strom als Spurdaten aus, obwohl sein eigener Dateikopf sagt „Track
 * data is zlib-compressed" — das Wissen stand im Kommentar und nicht
 * im Code. Dazu meldete er 21 Zylinder, wo 40 in der Datei stehen.
 *
 * **Aber die Zusicherung hier waere auch nach dem Entpacken falsch, und
 * das ist mein eigener Fehler gewesen:** MFI ist ein **Flussformat**.
 * Nach `src/samdisk/mfi.cpp` (MIT, im Baum) ist jede Zelle ein LE32 mit
 * 28 Bit Zeit und 4 Bit magnetischer Ausrichtung, und die Summe aller
 * Zeiten einer Spur muss **200 000 000** sein. Ein Sektorinhalt kommt
 * dort erst durch einen MFM-Dekoder heraus — diese Tafel prueft
 * Sektoren und ist fuer MFI das falsche Mass.
 *
 * MFI wird deshalb mit einer **flussgerechten** Pruefung nachgezogen
 * (Zellenzahl, Gesamtzeit, Ausrichtungsbits), nicht hier. Der Befund
 * ist gemessen und steht als P3-326.
 *
 * ── Der wichtigste Fund dieser Welle betrifft die METHODE ───────────
 *
 * `v9t9` sollte hier stehen. hxcfe hat die Datei erzeugt, 184 320 Byte,
 * die richtige Groesse, und **Erfolg gemeldet**. UFTs Leser oeffnete
 * sie, meldete 40 Zylinder / 2 Koepfe / 9 Sektoren — alles richtig —
 * und lieferte als Inhalt `F6 F6 F6`.
 *
 * Gemessen an der Datei selbst: **100 % `0xF6`**, null Treffer fuer
 * `UFT-K`. hxcfe hatte eine **leer formatierte** Diskette geschrieben,
 * weil sein RAW-Lader die TI-Geometrie nicht erkennt und sein
 * Layout-Verzeichnis keinen TI-Eintrag hat (`-rawlist` durchsucht:
 * kein Treffer fuer ti99/TI_/texas; erfundene Layoutnamen werden
 * abgewiesen). Der V9T9-Schreiber fand also keine Spuren und fuellte
 * mit dem Formatier-Fuellbyte.
 *
 * **UFT ist hier nicht der Fehler.** Und ohne das
 * Selbstbeschreibungs-Muster in der Eingabe haette dieser Test `v9t9`
 * auf einer Fuellbyte-Diskette „gehoben": ein gruener Test, der nichts
 * beweist, aus einem Werkzeug, das Erfolg meldet. Genau die
 * Unentscheidbarkeit, gegen die das Muster gebaut ist.
 *
 * **Regel daraus:** ein erzeugtes Fixture ist erst dann ein Beleg, wenn
 * sein INHALT nachgewiesen ist. Fuer die Sektorformate hier leistet das
 * die Zusicherung selbst (sie sucht das Muster). Fuer gepackte
 * Container (`mfi`, `ipf`) ist der Nachweis nur ueber einen Entpacker
 * zu fuehren — deshalb tragen die beiden **keine** Stufe, obwohl ihre
 * Dateien im Korpus liegen.
 *
 * `v9t9` bleibt T3, mit benanntem Blocker (P3-327).
 *
 * Ohne Korpus ueberspringt er sich benannt, nicht schweigend gruen.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "tests/corpus_free"
#endif

static int gruen = 0;
static int rot = 0;
static int uebersprungen = 0;

static void pruefe(const char *name, int bedingung, const char *hinweis)
{
    if (bedingung) {
        printf("  [OK]   %s\n", name);
        gruen++;
    } else {
        printf("  [ROT]  %s%s%s\n", name, hinweis ? "  -- " : "",
               hinweis ? hinweis : "");
        rot++;
    }
}

/* Traegt dieser Sektor das Muster der Eingabe an (spur, sektor)? */
static int ist_uft_sektor(const uint8_t *d, size_t len, int spur, int sektor)
{
    if (!d || len < 8) return 0;
    if (d[0] != (uint8_t)spur) return 0;
    if (d[1] != (uint8_t)sektor) return 0;
    return memcmp(d + 2, "UFT-K", 5) == 0;
}

/* Sucht in der Spur einen Sektor, der sich als (spur, sektor) ausgibt. */
static const uft_sector_t *finde_uft(const uft_track_t *t, int spur,
                                     int sektor)
{
    for (size_t i = 0; i < t->sector_count; i++)
        if (ist_uft_sektor(t->sectors[i].data, t->sectors[i].data_len,
                           spur, sektor))
            return &t->sectors[i];
    return NULL;
}

/* Die Plugins werden ueber ihr SYMBOL angesprochen, nicht ueber
 * `uft_get_format_plugin_by_name()`. Zwei Gruende, beide gemessen:
 *
 *  1. **Der Name ist nicht eindeutig.** `.name = "DSK"` steht zweimal
 *     im Baum — in `src/formats/dsk_cpc/uft_dsk_cpc.c:349` und in
 *     `src/formats/uft_format_registry.c:304`. Eine Namenssuche haette
 *     nicht gesagt, welches der beiden geantwortet hat.
 *  2. **Nur die Symbolbindung traegt die Stufe.**
 *     `scripts/gen_verification_tiers.py::_tests_by_symbol_ref()` ordnet
 *     einen Test seinem Plugin zu, indem es `uft_format_plugin_<sym>` im
 *     Testquelltext sucht. Ein Test, der nur Namens-Strings benutzt,
 *     zaehlt fuer KEIN Format — er waere gruen und wirkungslos.
 */
extern const uft_format_plugin_t uft_format_plugin_imd;
extern const uft_format_plugin_t uft_format_plugin_jv3;
extern const uft_format_plugin_t uft_format_plugin_dmk;
extern const uft_format_plugin_t uft_format_plugin_d88;
extern const uft_format_plugin_t uft_format_plugin_stx;
extern const uft_format_plugin_t uft_format_plugin_dsk_cpc;
/* MF-1021, Welle 2 */
extern const uft_format_plugin_t uft_format_plugin_st;
/* MF-1022: SAP kommt nicht von hxcfe, sondern von `libsap` aus `sap2`
 * (Alexandre Pukall / Eric Botcazou, GPL-2), lokal gebaut und
 * ausgefuehrt. Zwei Orakel in einer Tafel sind kein Widerspruch: die
 * Frage ist je Format, welche fremde Hand das Abbild geschrieben hat. */
extern const uft_format_plugin_t uft_format_plugin_sap_thomson;
int uft_sap_pruefsumme(const uft_disk_t *disk, int cyl, int sektor,
                       uint16_t *soll, uint16_t *ist);

typedef struct {
    const uft_format_plugin_t *p;
    const char *kurz;       /* fuer die Meldung */
    const char *datei;      /* im Korpus */
    const char *modul;      /* hxcfe-Modul */
} fall_t;

static const fall_t FAELLE[] = {
    { &uft_format_plugin_imd,     "imd",     "hxcfe_pc160.imd", "hxcfe IMD_IMG"        },
    { &uft_format_plugin_jv3,     "jv3",     "hxcfe_pc160.jv3", "hxcfe TRS80_JV3"      },
    { &uft_format_plugin_dmk,     "dmk",     "hxcfe_pc160.dmk", "hxcfe TRS80_DMK"      },
    { &uft_format_plugin_d88,     "d88",     "hxcfe_pc160.d88", "hxcfe NEC_D88"        },
    { &uft_format_plugin_stx,     "stx",     "hxcfe_pc160.stx", "hxcfe ATARIST_STX"    },
    { &uft_format_plugin_dsk_cpc, "dsk_cpc", "hxcfe_pc160.dsk", "hxcfe AMSTRADCPC_DSK" },
    /* MF-1021, Welle 2. Beide brauchen ihre EIGENE Geometrie — mit der
     * 160K-Eingabe der ersten Welle weist hxcfe sie ab:
     *   st    80 Spuren x 2 Koepfe x 9 Sektoren x 512 (uft_720k.img)
     *   v9t9  40 Spuren x 2 Koepfe x 9 Sektoren x 256 (uft_ti_dssd.img)
     * Eine Eingabe, die zum Format passt, ist keine Bequemlichkeit:
     * hxcfe erzeugt sonst gar nichts, und ein erzwungenes Layout waere
     * eine erfundene Diskette. */
    { &uft_format_plugin_st,      "st",      "hxcfe_720k.st",   "hxcfe ATARIST_ST"     },
    { &uft_format_plugin_sap_thomson, "sap", "sap2_thomson.sap", "libsap (sap2)" },
};
#define N_FAELLE ((int)(sizeof(FAELLE) / sizeof(FAELLE[0])))

int main(void)
{
    printf("=== Liest UFT Abbilder von fremder Hand? (MF-1020) ===\n");

    if (uft_register_all_formats() != UFT_OK) {
        printf("  [ROT]  uft_register_all_formats() schlug fehl\n");
        return 1;
    }

    /* Zuerst die Eingabe selbst: steht das Muster wirklich drin? Ohne
     * das ist jede weitere Aussage wertlos. */
    {
        char pfad[600];
        snprintf(pfad, sizeof(pfad), "%s/uft_pc160.img", UFT_CORPUS_DIR);
        FILE *f = fopen(pfad, "rb");
        if (!f) {
            printf("  [SKIP] %s fehlt — Korpus nicht vorhanden\n", pfad);
            printf("\n%d gruen, %d rot, %d uebersprungen\n",
                   gruen, rot, uebersprungen + 1);
            return 0;
        }
        uint8_t s0[256];
        size_t n = fread(s0, 1, sizeof(s0), f);
        fclose(f);
        char h[160];
        snprintf(h, sizeof(h), "%zu Byte gelesen, Byte 0..6 = "
                 "%02X %02X %c%c%c%c%c", n, s0[0], s0[1],
                 s0[2], s0[3], s0[4], s0[5], s0[6]);
        pruefe("die Eingabe traegt ihr Selbstbeschreibungs-Muster",
               n == 256 && ist_uft_sektor(s0, n, 0, 1), h);
    }

    for (int i = 0; i < N_FAELLE; i++) {
        char pfad[600];
        snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR,
                 FAELLE[i].datei);
        FILE *f = fopen(pfad, "rb");
        if (!f) {
            printf("  [SKIP] %-16s fehlt im Korpus\n", FAELLE[i].datei);
            uebersprungen++;
            continue;
        }
        fclose(f);

        const uft_format_plugin_t *p = FAELLE[i].p;
        char name[200], h[220];
        /* Der Erzeuger steht je Fall in `modul` — nicht pauschal
         * „von hxcfe", denn SAP kommt von libsap. Eine Meldung, die
         * das falsche Werkzeug nennt, ist eine falsche Aussage. */
        snprintf(name, sizeof(name), "%-7s liest ein Abbild aus %s",
                 FAELLE[i].kurz, FAELLE[i].modul);
        if (!p->open || !p->read_track) {
            snprintf(h, sizeof(h), "Plugin hat kein open/read_track");
            pruefe(name, 0, h);
            continue;
        }

        uft_disk_t d;
        memset(&d, 0, sizeof(d));
        uft_error_t rc = p->open(&d, pfad, true);
        if (rc != UFT_OK) {
            snprintf(h, sizeof(h), "open lieferte %d", (int)rc);
            pruefe(name, 0, h);
            continue;
        }

        /* Spur 0 und eine Spur weiter innen — eine allein koennte per
         * Zufall am richtigen Versatz liegen. */
        int getroffen = 0, spuren_gelesen = 0;
        const int proben[2] = { 0, 17 };
        char detail[160] = "";
        for (int k = 0; k < 2; k++) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            if (p->read_track(&d, proben[k], 0, &t) == UFT_OK) {
                spuren_gelesen++;
                /* Sektor 1 der Spur muss sich als (Spur, 1) ausgeben. */
                if (finde_uft(&t, proben[k], 1)) getroffen++;
                else if (t.sector_count > 0 && detail[0] == '\0')
                    snprintf(detail, sizeof(detail),
                             "Spur %d: %zu Sektoren, Byte 0..2 = "
                             "%02X %02X %02X", proben[k],
                             (size_t)t.sector_count,
                             t.sectors[0].data[0], t.sectors[0].data[1],
                             t.sectors[0].data[2]);
                else if (detail[0] == '\0')
                    snprintf(detail, sizeof(detail),
                             "Spur %d: 0 Sektoren, %zu Rohbyte",
                             proben[k], (size_t)t.raw_size);
            }
            free(t.sectors);
            free(t.raw_data);
        }
        snprintf(h, sizeof(h), "%u Zyl, %u Koepfe; %d von 2 Spuren "
                 "getroffen%s%s", d.geometry.cylinders, d.geometry.heads,
                 getroffen, detail[0] ? "; " : "", detail);
        pruefe(name, getroffen == 2 && spuren_gelesen == 2, h);
        if (p->close) p->close(&d);
    }

    /* ── Die Pruefsummen stehen in der DATEI, nicht in unserem Code ──
     *
     * MF-1022: SAP legt je Sektor eine Pukall-Pruefsumme ab, gebildet
     * ueber die vier Kopfbytes UND die ENTSCHLUESSELTEN Daten. Der
     * Parametersatz (gespiegeltes CCITT 0x8408, Start 0xFFFF, kein
     * Abschluss-XOR) ist aus dem VERHALTEN von `libsap` abgeleitet, nicht
     * aus seinem Quelltext abgeschrieben — eine Tafel abzuschreiben waere
     * eine GPL-2-Uebernahme.
     *
     * Gehen alle 16 Pruefsummen einer Spur auf, ist das eine Aussage
     * ueber die Wirklichkeit: `libsap` hat sie geschrieben, UFT rechnet
     * sie nach. Derselbe Weg wie MF-869 (FM-CRCs auf der Diskette) und
     * MF-1013 (GCR-Pruefsummen auf der Diskette). */
    {
        char pfad[600];
        snprintf(pfad, sizeof(pfad), "%s/sap2_thomson.sap", UFT_CORPUS_DIR);
        FILE *f = fopen(pfad, "rb");
        if (!f) {
            printf("  [SKIP] sap2_thomson.sap fehlt im Korpus\n");
            uebersprungen++;
        } else {
            fclose(f);
            uft_disk_t d;
            memset(&d, 0, sizeof(d));
            if (uft_format_plugin_sap_thomson.open(&d, pfad, true) == UFT_OK) {
                int gut = 0, schlecht = 0;
                uint16_t erst_soll = 0, erst_ist = 0;
                for (int sek = 0; sek < 16; sek++) {
                    uint16_t soll = 0, ist = 0;
                    int rc2 = uft_sap_pruefsumme(&d, 0, sek, &soll, &ist);
                    if (sek == 0) { erst_soll = soll; erst_ist = ist; }
                    if (rc2 == 0) gut++;
                    else schlecht++;
                }
                char h2[200];
                snprintf(h2, sizeof(h2),
                         "%d gut, %d schlecht; Sektor 0: Datei %04X, "
                         "nachgerechnet %04X", gut, schlecht,
                         erst_soll, erst_ist);
                pruefe("sap     — alle 16 Pukall-Pruefsummen der Spur 0 "
                       "gehen auf", gut == 16 && schlecht == 0, h2);
                uft_format_plugin_sap_thomson.close(&d);
            } else {
                pruefe("sap     — alle 16 Pukall-Pruefsummen der Spur 0 "
                       "gehen auf", 0, "open schlug fehl");
            }
        }
    }

    printf("\n%d gruen, %d rot, %d uebersprungen\n",
           gruen, rot, uebersprungen);
    return rot ? 1 : 0;
}
