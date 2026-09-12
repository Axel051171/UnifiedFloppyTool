/**
 * @file test_atx_gegen_a8rawconv.c
 * @brief Die erste ECHTE ATX im Korpus, gegen eine zweite Hand (MF-1065)
 *
 * `atx` stand auf **T2** — und im Korpus lag seit dem 2026-09-04 ein
 * Abbild mit `origin: "real"`, das **kein Test geoeffnet hat**. Der
 * Manifest-Eintrag sagt es woertlich: *„Erstes reales ATX und erstes
 * reales MFI im Korpus — beide Tier-Vermerke ,kein reales Abbild'
 * faellig"*, und im Feld `test` stand: **„noch keiner"**.
 *
 * Das ist die billigste Hebung, die es gibt: die Beschaffung war getan,
 * die Messung fehlte.
 *
 * ── Das Objekt ──────────────────────────────────────────────────────────
 *
 * `F-15 Strike Eagle` (MicroProse, Atari 8-bit), von archive.org
 * (@floppyarchaeology), 100 344 Byte, sha256 `54438032…`. Eine
 * **kopiergeschuetzte** Diskette — und genau deshalb taugt sie: ein
 * unbeschriebenes Abbild wuerde von ATX nichts verlangen, was ein ATR
 * nicht auch koennte.
 *
 * Die Datei ist gitignoriert (`/tests/corpus/`) und bleibt es
 * (Rechtelage am Objekt ungeklaert, Eigentuemer-Entscheidung
 * 2026-09-04: „beschaffen und messen, Weitergabe nicht"). Ohne sie
 * ueberspringt sich dieser Test benannt.
 *
 * ── Die zweite Hand ─────────────────────────────────────────────────────
 *
 * **a8rawconv v0.95** (Avery Lee, GPL-2+), im Klon
 * `tools/uft-scout/work/a8rawconv/` gebaut. **Ausgefuehrt, nicht
 * portiert** — Kanal *Oracle* nach MF-695, keine Zeile uebernommen.
 * Sein Schalter `-l` gibt die Spur-/Sektorkarte aus, also eine fremde
 * ZERLEGUNG und nicht bloss ein Lesen (Massstab MF-1037).
 *
 *     a8rawconv -if atx -of atr -l <datei>.atx <datei>.atr
 *
 * ── Was beide Haende sagen, nebeneinander ───────────────────────────────
 *
 *                              UFT              a8rawconv
 *     Spuren                    40               40
 *     Sektoren gesamt          721               39 x 18 + 19
 *     Spur mit 19 Sektoren     Spur 2            „Track 2, sector 5:
 *                                                 1 phantom sector found"
 *     zweites Merkmal          Spur 39 ID 3      „Track 39, sector 3:
 *                              CRC_ERROR          Stable CRC error"
 *
 * Und die REIHENFOLGE stimmt Position fuer Position:
 *
 *     Spur  0  beide:  1 3 5 7 9 11 13 15 17 2 4 6 8 10 12 14 16 18
 *     Spur  2  beide:  1 3 5 7 9 11 13 15 17 2 4 5 8 10 12 14 16 18 6
 *                                                  ^^ Phantom       ^^
 *
 * Das ist die 9:1-Verschraenkung des Atari-810-Laufwerks; auf Spur 2
 * steht der Phantomsektor 5 an der Stelle, an der Sektor 6 stuende, und
 * 6 rutscht ans Ende. **Beide Haende sehen dieselbe Verdraengung.**
 *
 * ── Warum die Statuswerte hier zaehlen ──────────────────────────────────
 *
 * UFT meldet fuer Spur 39 / Sektor 3 `UFT_SECTOR_CRC_ERROR` (Daten-CRC)
 * und laesst `id.crc_ok` dabei **wahr** — die ID-Feld-CRC ist ja in
 * Ordnung, nur die Daten-CRC nicht. a8rawconv sagt genau dasselbe
 * („Stable CRC error"), und es ist dieselbe Unterscheidung, die MF-1022
 * bei `sap` und MF-1017 bei `jv3` gekostet hat. Der Test nagelt beide
 * Felder einzeln fest, damit aus „irgendwie markiert" nicht „richtig
 * markiert" wird.
 *
 * Der Phantomsektor traegt `UFT_SECTOR_DUPLICATE` — und zwar **beide**
 * Ausfertigungen, nicht nur die zweite. Das ist richtig: welche von
 * beiden das Laufwerk liefert, entscheidet die Drehlage, nicht die
 * Datei.
 *
 * ── Was NICHT geprueft ist ──────────────────────────────────────────────
 *
 * * **Weak Bits.** UFT meldet 0 Sektoren mit `weak_mask`, und
 *   a8rawconv nennt keine. Ob die Diskette welche traegt und beide
 *   Haende sie uebersehen, sagt dieser Test nicht — er haelt nur fest,
 *   dass beide dasselbe sehen.
 * * **Der Inhalt der Sektoren.** Ein Byte-Vergleich gegen a8rawconvs
 *   ATR-Ausgabe ist moeglich und waere die naechste Verschaerfung; er
 *   braucht aber die 92 176 Byte grosse ATR im Korpus, und die ist
 *   heute nicht dort.
 * * **Der Schreibpfad.** Hier wird nur gelesen.
 *
 * ── Eine Falle, in die der erste Entwurf gelaufen ist ───────────────────
 *
 * Die Zusagen zur ID-Feld-CRC lasen zuerst `sec->id.crc_ok`. Das ist
 * das falsche Feld: `uft_sector_set_id_crc()` schreibt
 * `sec->id_crc_ok`, und `uft_format_add_sector_with_id()` setzt das
 * verschachtelte `id.crc_ok` **unbedingt auf wahr**. Die Zusage war
 * damit gruen, egal was der Leser tat — die Klasse aus MF-1014,
 * MF-1026 und MF-1028, nur diesmal in meinem eigenen Test.
 *
 * Gefangen hat es die Mutationsmatrix (M4: „Daten-CRC-Fehler auch als
 * ID-CRC melden" rutschte durch). Gemessen ueber `git ls-files`:
 * `id_crc_ok` hat **15** Lesestellen in `src/`, das verschachtelte
 * `id.crc_ok` **keine einzige** — seine eine Fundstelle
 * (`uft_g64.c:641`) ist selbst ein Schreiben. Ein Produktionsfehler ist
 * das nicht; eine Falle fuer den naechsten Testautor schon.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"   /* uft_track_release — implizite Deklaration
                              * ist auf macOS-Clang ein FEHLER */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Der Pfad kommt aus dem Bausystem: `UFT_CORPUS_RESTRICTED_DIR` ist die
 * Konvention dieses Baums fuer den rechtlich gefuehrten Teil des Korpus
 * (gitignoriert). Der Rueckfall auf einen relativen Pfad ist da, damit
 * ein Uebersetzen ohne CMake nicht still etwas anderes liest — er
 * findet die Datei dann in aller Regel nicht, und der Test
 * ueberspringt sich. */
#ifndef UFT_CORPUS_RESTRICTED_DIR
#define UFT_CORPUS_RESTRICTED_DIR "tests/corpus"
#endif

#define ATX_ECHT UFT_CORPUS_RESTRICTED_DIR \
                 "/kor_b/F-15 Strike Eagle - F-15 Strike Eagle.atx"

extern const uft_format_plugin_t uft_format_plugin_atx;

/* Am Objekt gemessen, bevor eine Zeile Testcode stand. */
#define ECHT_GROESSE     100344L
#define ECHT_SPUREN          40
#define ECHT_SEKTOREN       721   /* 39 x 18 + 19 */
#define ECHT_SGR            128
#define PHANTOM_SPUR          2
#define PHANTOM_ID            5
#define CRC_SPUR             39
#define CRC_ID                3

/* a8rawconvs Karte fuer Spur 0 — die 9:1-Verschraenkung des 810. */
static const uint8_t KARTE_S0[18] = {
    1, 3, 5, 7, 9, 11, 13, 15, 17, 2, 4, 6, 8, 10, 12, 14, 16, 18
};
/* Und fuer Spur 2, mit dem Phantom an Position 11 und der 6 am Ende. */
static const uint8_t KARTE_S2[19] = {
    1, 3, 5, 7, 9, 11, 13, 15, 17, 2, 4, 5, 8, 10, 12, 14, 16, 18, 6
};

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung, const char *detail)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s - %s\n", was, detail ? detail : ""); }
}

int main(void)
{
    const uft_format_plugin_t *p = &uft_format_plugin_atx;
    uft_disk_t disk;
    FILE *f;
    long gr;
    uint8_t kopf[64];
    char det[300];
    int c, konf = -1, ok;
    unsigned gesamt = 0, spuren = 0, leer = 0, neunzehn = 0;
    int neunzehn_spur = -1;
    unsigned phantom = 0, phantom_dup = 0;
    unsigned crc_fehler = 0, id_crc_schlecht = 0, weak = 0;
    int crc_spur = -1, crc_id = -1, crc_idfeld_ok = -1;
    int s0_gleich = -1, s2_gleich = -1;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("ATX: die echte F-15-Diskette gegen a8rawconv - MF-1065\n");
    printf("======================================================\n");

    f = fopen(ATX_ECHT, "rb");
    if (!f) {
        printf("SKIP: %s fehlt (gitignorierter Korpusteil).\n", ATX_ECHT);
        return 77;      /* SKIP_RETURN_CODE, seit MF-598 */
    }
    fseek(f, 0, SEEK_END);
    gr = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fread(kopf, 1, sizeof kopf, f) != sizeof kopf) {
        fclose(f);
        printf("SKIP: %s zu kurz.\n", ATX_ECHT);
        return 77;
    }
    fclose(f);

    snprintf(det, sizeof det, "%ld Byte, Kennung %.4s", gr, (char *)kopf);
    pruefe("das Objekt ist 100 344 Byte gross und traegt \"AT8X\"",
           gr == ECHT_GROESSE && memcmp(kopf, "AT8X", 4) == 0, det);

    ok = p->probe(kopf, sizeof kopf, (size_t)gr, &konf) ? 1 : 0;
    snprintf(det, sizeof det, "probe=%d konf=%d", ok, konf);
    pruefe("die Sonde erkennt es an der Kennung, Band \"Merkmal "
           "getroffen\" (>= 80)", ok && konf >= 80, det);

    memset(&disk, 0, sizeof disk);
    if (p->open(&disk, ATX_ECHT, true) != UFT_OK) {
        pruefe("open liest das echte Abbild", 0, ATX_ECHT);
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }

    snprintf(det, sizeof det, "%d x %d x %d x %d",
             disk.geometry.cylinders, disk.geometry.heads,
             disk.geometry.sectors, disk.geometry.sector_size);
    pruefe("Geometrie 40 x 1 x 19 x 128 - die 19 ist der Phantomsektor, "
           "nicht die Regel",
           disk.geometry.cylinders == ECHT_SPUREN
           && disk.geometry.heads == 1
           && disk.geometry.sectors == 19
           && disk.geometry.sector_size == ECHT_SGR, det);

    for (c = 0; c < ECHT_SPUREN; c++) {
        uft_track_t t;
        size_t s;
        memset(&t, 0, sizeof t);
        if (p->read_track(&disk, c, 0, &t) != UFT_OK) continue;
        spuren++;
        if (t.sector_count == 0) leer++;
        if (t.sector_count == 19) { neunzehn++; neunzehn_spur = c; }

        if (c == 0 && t.sector_count == 18) {
            s0_gleich = 1;
            for (s = 0; s < 18; s++)
                if (t.sectors[s].id.sector != KARTE_S0[s]) s0_gleich = 0;
        }
        if (c == PHANTOM_SPUR && t.sector_count == 19) {
            s2_gleich = 1;
            for (s = 0; s < 19; s++)
                if (t.sectors[s].id.sector != KARTE_S2[s]) s2_gleich = 0;
        }

        for (s = 0; s < t.sector_count; s++) {
            const uft_sector_t *sec = &t.sectors[s];
            gesamt++;
            /* `id_crc_ok` und NICHT `id.crc_ok` — siehe die Notiz am
             * Ende dieses Kopfs. */
            if (!sec->id_crc_ok) id_crc_schlecht++;
            /* Beides: der Zeiger UND das Statusflag. Der erste Entwurf
             * prueft nur den Zeiger, und eine Mutation, die
             * `UFT_SECTOR_WEAK` pauschal setzt, rutschte durch. */
            if (sec->weak_mask || (sec->status & UFT_SECTOR_WEAK)) weak++;
            if (c == PHANTOM_SPUR && sec->id.sector == PHANTOM_ID) {
                phantom++;
                if (sec->status & UFT_SECTOR_DUPLICATE) phantom_dup++;
            }
            if (sec->status & UFT_SECTOR_CRC_ERROR) {
                crc_fehler++;
                crc_spur = c;
                crc_id = (int)sec->id.sector;
                crc_idfeld_ok = sec->id_crc_ok ? 1 : 0;
            }
        }
        uft_track_release(&t);
    }
    p->close(&disk);

    snprintf(det, sizeof det, "%u Spuren gelesen, %u leer, %u Sektoren",
             spuren, leer, gesamt);
    pruefe("40 Spuren, keine leer, 721 Sektoren - 39 x 18 plus der "
           "eine Phantomsektor",
           spuren == ECHT_SPUREN && leer == 0
           && gesamt == ECHT_SEKTOREN, det);

    snprintf(det, sizeof det, "%u Spur(en) mit 19 Sektoren, zuletzt %d",
             neunzehn, neunzehn_spur);
    pruefe("genau EINE Spur hat 19 Sektoren, und es ist Spur 2 - "
           "a8rawconv: \"Track 2, sector 5: 1 phantom sector found\"",
           neunzehn == 1 && neunzehn_spur == PHANTOM_SPUR, det);

    snprintf(det, sizeof det, "s0_gleich=%d", s0_gleich);
    pruefe("Spur 0 liefert die Sektoren in a8rawconvs Reihenfolge - "
           "die 9:1-Verschraenkung des Atari 810", s0_gleich == 1, det);

    snprintf(det, sizeof det, "s2_gleich=%d", s2_gleich);
    pruefe("Spur 2 ebenso, mit dem Phantom an Position 11 und der "
           "verdraengten 6 am Ende", s2_gleich == 1, det);

    snprintf(det, sizeof det, "%u mal ID %d, davon %u als DUPLICATE",
             phantom, PHANTOM_ID, phantom_dup);
    pruefe("Sektor 5 steht auf Spur 2 ZWEIMAL, und BEIDE tragen "
           "UFT_SECTOR_DUPLICATE",
           phantom == 2 && phantom_dup == 2, det);

    snprintf(det, sizeof det, "%u Sektor(en), zuletzt Spur %d ID %d",
             crc_fehler, crc_spur, crc_id);
    pruefe("genau EIN Sektor traegt UFT_SECTOR_CRC_ERROR, und es ist "
           "Spur 39 / ID 3 - a8rawconv: \"Stable CRC error\"",
           crc_fehler == 1 && crc_spur == CRC_SPUR && crc_id == CRC_ID,
           det);

    snprintf(det, sizeof det, "id.crc_ok=%d", crc_idfeld_ok);
    pruefe("bei diesem Sektor ist die ID-Feld-CRC INTAKT - falsch ist "
           "die Daten-CRC, und das sind zwei Aussagen",
           crc_idfeld_ok == 1, det);

    snprintf(det, sizeof det, "%u Sektor(en) mit schlechter ID-CRC",
             id_crc_schlecht);
    pruefe("kein einziger Sektor hat eine schlechte ID-Feld-CRC",
           id_crc_schlecht == 0, det);

    snprintf(det, sizeof det, "%u Sektor(en) mit weak_mask ODER "
             "UFT_SECTOR_WEAK", weak);
    pruefe("kein Sektor traegt Weak Bits - weder als Maske noch als "
           "Statusflag; a8rawconv nennt ebenfalls keine (beide Haende "
           "sehen dasselbe, mehr sagt das nicht)",
           weak == 0, det);

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
