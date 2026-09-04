/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_d64_fehlerbytes.c
 * @brief D64-Fehlerbytes als Schutzbefund (MF-876).
 *
 * Begruendung, Messungen und die Referenz stehen im Kopf von
 * `include/uft/protection/uft_d64_fehlerbytes.h`.
 */
#include "uft/protection/uft_d64_fehlerbytes.h"

#include <string.h>

/* ── Die Tabelle ─────────────────────────────────────────────────────
 *
 * Quelle: `docs/format_specs/commodore/D64.TXT`, Abschnitt „*** Error
 * codes" (Peter Schepers, Rev. 1.11; dort nach Immers/Neufeld, „Inside
 * Commodore DOS"). Es wurde ausschliesslich diese Dokumentation
 * gelesen, kein fremder Quelltext — die Tabelle ist eigenstaendig
 * eingetragen.
 *
 * Die vier Spalten der Referenz heissen dort, von links nach rechts:
 * das Byte im D64, die Zahl die das 1541-Laufwerk meldet, der Typ
 * (Seek/Read/Write) und die Beschreibung.
 *
 * Die Zuordnung zur Taxonomie ist eine EINORDNUNG, keine Uebersetzung
 * — sie steht je Zeile begruendet. Wo kein Code passt, steht
 * UFT_SCHUTZ_UNBEKANNT; das ist ehrlicher als ein ungefaehr passender.
 */
typedef struct {
    uint8_t           byte;        /* Byte im D64             */
    uint8_t           fehler_1541; /* was die 1541 meldet    */
    uft_schutz_code_t code;
    const char       *text;
} zeile_t;

static const zeile_t TABELLE[] = {
    /* Kein Fehler — kein Befund. */
    { 0x01, 0, UFT_SCHUTZ_UNBEKANNT, "kein Fehler" },

    /* „Header descriptor byte not found (HEX $08, GCR $52)" bzw.
     * „Header block not found". Der Sektor hat kein lesbares ID-Feld. */
    { 0x02, 20, UFT_SCHUTZ_SNI, "Header nicht gefunden" },

    /* „No SYNC sequence found." Die Referenz warnt bei genau diesem
     * Code am deutlichsten vor Uebertragung: „it depends on where the
     * physical head is on the disk when a read attempt is made." */
    { 0x03, 21, UFT_SCHUTZ_ISS, "keine SYNC-Folge" },

    /* „Data descriptor byte not found (HEX $07, GCR $55)" — der Sektor
     * hat einen Header, aber keinen Datenblock. */
    { 0x04, 22, UFT_SCHUTZ_SND, "Datenkennung nicht gefunden" },

    /* „Checksum error in data block" — der klassische DCE. */
    { 0x05, 23, UFT_SCHUTZ_DCE, "Pruefsummenfehler im Datenblock" },

    /* Die drei Schreibfehler sagen etwas ueber den SCHREIBVORGANG,
     * nicht ueber das Medium. Kein Schutzcode. */
    { 0x06, 24, UFT_SCHUTZ_UNBEKANNT, "Write verify (beim Formatieren)" },
    { 0x07, 25, UFT_SCHUTZ_UNBEKANNT, "Write verify" },
    { 0x08, 26, UFT_SCHUTZ_UNBEKANNT, "Schreibschutz aktiv" },

    /* „Checksum error in header block" — das ID-Feld ist da, aber
     * unstimmig. */
    { 0x09, 27, UFT_SCHUTZ_IIF, "Pruefsummenfehler im Header" },

    /* „In actual fact, this error never occurs" — die Referenz selbst.
     * Also kein Schutzcode. */
    { 0x0A, 28, UFT_SCHUTZ_UNBEKANNT, "Schreibfehler" },

    /* „Disk sector ID mismatch" — die IDs des gelesenen Headers weichen
     * von denen ab, die das Laufwerk erwartet. Das ist unter den zehn
     * der einzige Code, der ohne Weiteres ABSICHT bedeuten kann. */
    { 0x0B, 29, UFT_SCHUTZ_IIF, "Sektor-ID stimmt nicht" },
};

#define TABELLE_N (sizeof TABELLE / sizeof TABELLE[0])

static const zeile_t *finde(uint8_t b)
{
    for (size_t i = 0; i < TABELLE_N; i++)
        if (TABELLE[i].byte == b) return &TABELLE[i];
    return NULL;
}

uft_schutz_code_t uft_d64_fehlerbyte_code(uint8_t fehlerbyte)
{
    const zeile_t *z = finde(fehlerbyte);
    return z ? z->code : UFT_SCHUTZ_UNBEKANNT;
}

const char *uft_d64_fehlerbyte_name(uint8_t fehlerbyte)
{
    const zeile_t *z = finde(fehlerbyte);
    /* Kein Rateversuch fuer unbekannte Werte. Ein D64 mit 0x42 in der
     * Fehlerkarte ist ein Befund ueber die DATEI, nicht ueber die
     * Diskette — und das steht so da. */
    return z ? z->text : "kein Wert der Referenztabelle";
}

/* ── Geometrie ───────────────────────────────────────────────────────
 *
 * Die sechs Groessen, die MF-871 aus der Zonentabelle abgeleitet hat.
 * Sie stehen hier NICHT nochmal als Zahl, sondern werden gerechnet:
 * eine siebte Kopie waere genau die Drift, die MF-871 aufgeraeumt hat.
 */
static const struct { int spuren; int bloecke; } ZONEN[] = {
    { 35, 683 }, { 40, 768 }, { 41, 785 }, { 42, 802 },
};
#define ZONEN_N (sizeof ZONEN / sizeof ZONEN[0])

/** @return Blockzahl, oder 0 wenn `size` keine D64-Groesse ist.
 *  `*mit_karte` sagt, ob eine Fehlerkarte anhaengt. */
static int geometrie(size_t size, int *spuren, bool *mit_karte)
{
    for (size_t i = 0; i < ZONEN_N; i++) {
        size_t roh = (size_t)ZONEN[i].bloecke * 256u;
        if (size == roh)                      { *spuren = ZONEN[i].spuren;
                                                *mit_karte = false;
                                                return ZONEN[i].bloecke; }
        if (size == roh + (size_t)ZONEN[i].bloecke) {
            *spuren = ZONEN[i].spuren;
            *mit_karte = true;
            return ZONEN[i].bloecke;
        }
    }
    return 0;
}

/** Spur/Sektor zum n-ten Block (0-basiert). */
static void ort_von_block(int block, int *spur, int *sektor)
{
    static const struct { int von, bis, sek; } Z[] = {
        {  1, 17, 21 }, { 18, 24, 19 }, { 25, 30, 18 }, { 31, 42, 17 },
    };
    int lauf = 0;
    for (size_t i = 0; i < sizeof Z / sizeof Z[0]; i++) {
        for (int t = Z[i].von; t <= Z[i].bis; t++) {
            if (block < lauf + Z[i].sek) {
                *spur = t;
                *sektor = block - lauf;
                return;
            }
            lauf += Z[i].sek;
        }
    }
    *spur = -1;
    *sektor = -1;
}

/* Die sechs zeitbasierten Codes — auf einem Sektorabbild grundsaetzlich
 * nicht feststellbar. Sie werden IMMER als uebersprungen gemeldet. */
static const uft_schutz_code_t NUR_FLUSS[] = {
    UFT_SCHUTZ_LGS, UFT_SCHUTZ_SHS, UFT_SCHUTZ_LGT,
    UFT_SCHUTZ_SHT, UFT_SCHUTZ_SBV, UFT_SCHUTZ_NFA,
};

/* Die datenbasierten Codes, die eine Fehlerkarte tragen KANN. Fehlt
 * sie, sind auch diese nicht pruefbar — und das wird gesagt. */
static const uft_schutz_code_t AUS_FEHLERKARTE[] = {
    UFT_SCHUTZ_SNI, UFT_SCHUTZ_ISS, UFT_SCHUTZ_SND,
    UFT_SCHUTZ_DCE, UFT_SCHUTZ_IIF,
};

bool uft_schutz_aus_d64(const uint8_t *data, size_t size,
                        uft_schutz_bericht_t *b)
{
    if (!data || !b) return false;

    int spuren = 0;
    bool mit_karte = false;
    int bloecke = geometrie(size, &spuren, &mit_karte);
    if (bloecke == 0) return false;

    const uft_schutz_ort_t GANZE_DISKETTE = { -1, 0, -1, 0, 0 };

    /* 1) Was hier grundsaetzlich nicht geht. */
    for (size_t i = 0; i < sizeof NUR_FLUSS / sizeof NUR_FLUSS[0]; i++)
        uft_schutz_bericht_uebersprungen(b, NUR_FLUSS[i],
                                         UFT_UEBERSPRUNGEN_KEIN_FLUSS,
                                         GANZE_DISKETTE);

    /* 2) Zusatzspuren — das EINZIGE, was ohne Fehlerkarte messbar ist.
     *
     * Die Spurzahl folgt aus der Dateigroesse, und die Groessen folgen
     * aus der Zonentabelle (MF-871). Das ist eine Beobachtung am
     * Abbild selbst, keine Ableitung aus einer Fehlerkategorie —
     * deshalb als einziger Befund GEMESSEN. */
    if (spuren > 35) {
        uft_schutz_befund_t f;
        memset(&f, 0, sizeof f);
        f.code        = UFT_SCHUTZ_EXT;
        f.beleg       = UFT_BELEG_GEMESSEN;
        f.ort         = GANZE_DISKETTE;
        f.messgroesse = "spuren";
        f.messwert    = (double)spuren;
        uft_schutz_bericht_add(b, &f);
    }

    /* 3) Ohne Fehlerkarte ist hier Schluss — und das wird gemeldet,
     * nicht verschwiegen. */
    if (!mit_karte) {
        for (size_t i = 0;
             i < sizeof AUS_FEHLERKARTE / sizeof AUS_FEHLERKARTE[0]; i++)
            uft_schutz_bericht_uebersprungen(
                b, AUS_FEHLERKARTE[i],
                UFT_UEBERSPRUNGEN_KEINE_FEHLERINFO, GANZE_DISKETTE);
        return true;
    }

    /* 4) Die Fehlerkarte auswerten. Sie liegt hinter den Sektordaten,
     * ein Byte je Block, in derselben Reihenfolge. */
    const uint8_t *karte = data + (size_t)bloecke * 256u;
    for (int i = 0; i < bloecke; i++) {
        uint8_t fb = karte[i];
        if (fb == UFT_D64_FB_OK) continue;

        uft_schutz_code_t code = uft_d64_fehlerbyte_code(fb);
        if (code == UFT_SCHUTZ_UNBEKANNT) continue;

        int spur = -1, sektor = -1;
        ort_von_block(i, &spur, &sektor);

        uft_schutz_befund_t f;
        memset(&f, 0, sizeof f);
        f.code  = code;
        /* GEFOLGERT, nicht GEMESSEN — siehe Kopf des Headers: das
         * Fehlerbyte ist die KATEGORIE einer GCR-Anomalie, nicht die
         * Anomalie. Schepers selbst: „all of the low level data that
         * comprises the errors is lost." */
        f.beleg = UFT_BELEG_GEFOLGERT;
        f.ort.zylinder = spur;
        f.ort.kopf     = 0;
        f.ort.sektor   = sektor;
        f.messgroesse  = "d64_fehlerbyte";
        f.messwert     = (double)fb;
        uft_schutz_bericht_add(b, &f);
    }
    return true;
}
