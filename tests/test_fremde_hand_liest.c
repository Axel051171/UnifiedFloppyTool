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
#include "uft/formats/kryoflux_checker.h"

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
extern const uft_format_plugin_t uft_format_plugin_mfi;
extern const uft_format_plugin_t uft_format_plugin_kfx;
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

    /* ── MFI: ein Flussformat, das seine Geometrie selbst ansagt ────
     *
     * MF-1023. `mfi` kann hier nicht in der Sektortafel oben stehen —
     * MFI speichert je Spur einen **zlib-gepackten** Strom von
     * 32-Bit-Flusszellen, keine Sektoren. Die Zusicherungen sind
     * deshalb andere, und sie pruefen genau das, was der Leser heute
     * leisten KANN und was er absagen MUSS.
     *
     * Die Zahlen stehen in der Datei und sind unabhaengig nachgelesen
     * (Python, gegen `src/samdisk/mfi.cpp`): Kennung
     * "MAMEFLOPPYIMAGE\0", `cyl_count = 42` (Aufloesung 0),
     * `head_count = 1`, 42 belegte Spureintraege.
     *
     * **Vorzustand, gemessen:** 21 Zylinder / 2 Koepfe — der Leser
     * rechnete `count / 2` und `count % 2`, ignorierte also die
     * Kopfzahl der Datei. Und `read_track` gab den gepackten Strom als
     * Sektor aus, beginnend mit `78 9C` (zlib-Kopf). */
    {
        char pfad[600];
        snprintf(pfad, sizeof(pfad), "%s/hxcfe_pc160.mfi", UFT_CORPUS_DIR);
        FILE *f = fopen(pfad, "rb");
        if (!f) {
            printf("  [SKIP] hxcfe_pc160.mfi fehlt im Korpus\n");
            uebersprungen++;
        } else {
            fclose(f);
            uft_disk_t d;
            memset(&d, 0, sizeof(d));
            uft_error_t rc = uft_format_plugin_mfi.open(&d, pfad, true);
            char h3[220];
            snprintf(h3, sizeof(h3), "open=%d, %u Zyl, %u Koepfe "
                     "(die Datei sagt 42 / 1)", (int)rc,
                     d.geometry.cylinders, d.geometry.heads);
            pruefe("mfi     — die Geometrie kommt aus dem Kopf der Datei, "
                   "nicht aus einer festen 2", rc == UFT_OK
                   && d.geometry.cylinders == 42
                   && d.geometry.heads == 1, h3);

            if (rc == UFT_OK) {
                uft_track_t t;
                memset(&t, 0, sizeof(t));
                uft_error_t r = uft_format_plugin_mfi.read_track(&d, 0, 0,
                                                                &t);
                snprintf(h3, sizeof(h3), "rc=%d, %zu Sektoren, %zu Rohbyte",
                         (int)r, (size_t)t.sector_count,
                         (size_t)t.raw_size);
                pruefe("mfi     — und die gepackten Spurdaten werden "
                       "ABGESAGT, nicht als Sektor ausgegeben",
                       r != UFT_OK && t.sector_count == 0
                       && t.raw_size == 0, h3);
                free(t.sectors);
                free(t.raw_data);
                uft_format_plugin_mfi.close(&d);
            }
        }
    }

    /* ── kfx: ein KryoFlux-Strom, und der Pruefer sagt auch NEIN ────
     *
     * MF-1024. Ein KryoFlux-Strom ist EINE Spur je Datei, also kann
     * die Sektortafel oben ihn nicht pruefen. Die Frage ist stattdessen:
     * erkennt UFT die STRUKTUR des Stroms, und weist es Fremdes ab?
     *
     * `uft_kfc_stream_is_valid()` (MF-919) laeuft die OOB-Kette ab und
     * prueft die in den Bloecken EINGEBETTETE Stromposition gegen die
     * eigene Zaehlung der Nicht-OOB-Bytes. Das ist eine Aussage ueber
     * die Datei, nicht ueber unseren Code — und es ist der Grund, warum
     * `kfx` hier eine Stufe tragen kann, obwohl es keine Sektoren
     * liefert.
     *
     * Gemessen an den beiden Stroemen: je **10 OOB-Bloecke** und
     * **3 Indexmarken**. Und der Pruefer sagt nachweislich auch nein —
     * 65536 Byte Zufall (OOB 1, Index 0), ein Strom mit EINEM
     * gekippten 0x0D (OOB 7 statt 10) und eine IMD-Datei (OOB 24,
     * Index 0) fallen alle durch. Das ist wichtig, weil `kfx`s Sonde
     * einmal genau daran gescheitert ist: MF-919 hat gemessen, dass
     * sie 0x0D-Bytes ZAEHLTE und in 512 Zufallsbytes nie „nein" sagen
     * konnte. */
    {
        static const char *stroeme[2] = { "hxcfe_kfx_t00.0.raw",
                                          "hxcfe_kfx_t40.0.raw" };
        int gut = 0, vorhanden = 0;
        char detail[200] = "";
        for (int k = 0; k < 2; k++) {
            char pfad[600];
            snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR,
                     stroeme[k]);
            FILE *f = fopen(pfad, "rb");
            if (!f) continue;
            vorhanden++;
            fseek(f, 0, SEEK_END);
            long len = ftell(f);
            fseek(f, 0, SEEK_SET);
            uint8_t *b = (uint8_t *)malloc((size_t)len);
            size_t gelesen = b ? fread(b, 1, (size_t)len, f) : 0;
            fclose(f);
            if (!b || gelesen != (size_t)len) { free(b); continue; }

            uint32_t oob = 0, idx = 0;
            bool gueltig = uft_kfc_stream_is_valid(b, (size_t)len,
                                                   &oob, &idx);
            int conf = -1;
            bool p = uft_format_plugin_kfx.probe(b, (size_t)len,
                                                 (size_t)len, &conf);
            uft_disk_t d;
            memset(&d, 0, sizeof(d));
            uft_error_t rc = uft_format_plugin_kfx.open(&d, pfad, true);
            size_t roh = 0;
            if (rc == UFT_OK) {
                uft_track_t t;
                memset(&t, 0, sizeof(t));
                if (uft_format_plugin_kfx.read_track(&d, 0, 0, &t) == UFT_OK)
                    roh = t.raw_size;
                free(t.sectors);
                free(t.raw_data);
                uft_format_plugin_kfx.close(&d);
            }
            if (gueltig && oob == 10 && idx == 3 && p && conf >= 50
                && conf < 80 && rc == UFT_OK && roh == (size_t)len)
                gut++;
            else if (detail[0] == '\0')
                snprintf(detail, sizeof(detail),
                         "%s: gueltig=%d OOB=%u Index=%u Sonde=%d(%d) "
                         "open=%d roh=%zu von %ld", stroeme[k], gueltig,
                         oob, idx, p, conf, (int)rc, roh, len);
            free(b);
        }
        if (vorhanden == 0) {
            printf("  [SKIP] die KryoFlux-Stroeme fehlen im Korpus\n");
            uebersprungen++;
        } else {
            char h4[240];
            snprintf(h4, sizeof(h4), "%d von %d Stroemen vollstaendig%s%s",
                     gut, vorhanden, detail[0] ? "; " : "", detail);
            pruefe("kfx     — zwei KryoFlux-Stroeme aus hxcfe: OOB-Kette "
                   "geprueft, 10 Bloecke, 3 Indexmarken, Rohstrom "
                   "vollstaendig", gut == 2 && vorhanden == 2, h4);
        }

        /* Und der Pruefer sagt nein, wenn er nein sagen muss. */
        {
            static uint8_t zufall[65536];
            uint32_t z = 12345u;
            for (size_t i = 0; i < sizeof(zufall); i++) {
                z = z * 1103515245u + 12345u;
                zufall[i] = (uint8_t)(z >> 16);
            }
            uint32_t oob = 0, idx = 0;
            bool gueltig = uft_kfc_stream_is_valid(zufall, sizeof(zufall),
                                                   &oob, &idx);
            char h5[160];
            snprintf(h5, sizeof(h5), "gueltig=%d, OOB=%u, Index=%u",
                     gueltig, oob, idx);
            pruefe("kfx     — und 64 KB Pseudozufall werden ABGEWIESEN "
                   "(die Luecke aus MF-919)", !gueltig, h5);
        }
    }

    printf("\n%d gruen, %d rot, %d uebersprungen\n",
           gruen, rot, uebersprungen);
    return rot ? 1 : 0;
}
