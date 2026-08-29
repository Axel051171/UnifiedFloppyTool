/**
 * @file test_dim_atari_magic.c
 * @brief `.dim` ohne `BB` ist kein `.dim` (MF-687)
 *
 * ── Die Vorgeschichte, weil sie die Regel zeigt ──────────────────────────
 *
 * Der Fund kam aus einem Scout-Gutachten: Jacknife prüft an Offset 0 ein
 * Magic `0x4242`, unsere Probe prüft **keines**. Beim ersten Anlauf
 * (MF-684) wurde der Parser trotzdem **nicht** geändert, und das war die
 * richtige Entscheidung: **unser eigener Header** beschrieb Byte 0 als
 * „Flags (unused, often 0)" und Byte 1 als „Reserved". Eine verifizierte
 * Fremdquelle gegen die eigene Dokumentation — genau die Lage, aus der
 * fünf fabrizierte Parser kamen (FMT-2/3/10/11/12). Die zweite Quelle
 * war damals nicht beschaffbar.
 *
 * Ein Varianten-Zyklus hat sie beschafft. Jetzt sind es **drei
 * unabhängige Implementierungen**, alle drei von mir im Klon gelesen:
 *
 *   Hatari   `src/floppies/dim.c:75-76`
 *            `if (pDimFile[0x00] != 0x42 || pDimFile[0x01] != 0x42 ||
 *                 pDimFile[0x03] != 0    || pDimFile[0x0A] != 0)`
 *            → "This is not a valid DIM image!", Datei abgelehnt.
 *            Und `:36` als Spec: `0x0000 Word ID Header (0x4242('BB'))`.
 *
 *   HxC      `dim_loader/dim_loader.c:74` und `:110`
 *            `if (header->id_header == 0x4242)` / `!= 0x4242` → ablehnen.
 *
 *   Jacknife `dllmain.c:590`
 *            `*(unsigned short *)buffer == 0x4242  // "BB"`
 *
 * Damit ist die Frage entschieden, die MF-684 offenlassen musste: `BB`
 * gehört zum **Format**, nicht zu einer Fastcopy-Variante. Unser
 * Header-Kommentar war von keiner Quelle gedeckt.
 *
 * ── Was hier geprüft wird ────────────────────────────────────────────────
 *
 * Zwei Abbilder, die sich in **genau zwei Bytes** unterscheiden. Alles
 * andere — Größe, Geometrie, Nutzdaten — ist identisch. Damit kann kein
 * anderer Grund als das Magic den Unterschied erklären.
 *
 * Beide werden synthetisch gebaut, und das ist hier zulässig: geprüft
 * wird nicht, ob wir ein echtes DIM richtig **lesen**, sondern ob unsere
 * Probe eines **ablehnt**, das keines ist. Für die Ablehnung genügt die
 * Kopf-Struktur, und die steht in drei Quellen.
 *
 * ── Warum das mehr ist als Formalismus ───────────────────────────────────
 *
 * Eine Probe ohne Magic entscheidet allein nach Dateigröße und
 * Geometrie-Plausibilität. Jede Datei passender Länge — ein rohes
 * ST-Abbild, ein abgeschnittenes MSA, ein Zufallspuffer — wird dann als
 * DIM angenommen und ihre ersten 32 Byte als Geometrie gelesen. Das ist
 * keine Fehlerkennung am Rand: es ist eine erfundene Geometrie auf
 * fremden Daten, und der Benutzer sieht ein Ergebnis statt einer
 * Absage.
 */

#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "."
#endif

extern const uft_format_plugin_t uft_format_plugin_dim_atari;

static int fehler;

#define PRUEFE(bed, ...)                                                   \
    do { if (!(bed)) { printf("  FAIL "); printf(__VA_ARGS__);             \
                       printf("\n"); fehler++; } } while (0)

/* Ein Atari-DIM-Kopf nach der Hatari-Beschreibung (`dim.c:34-45`).
 *
 * 80 Spuren, 2 Seiten, 9 Sektoren, DD — die haeufigste ST-Geometrie.
 * Groesse: 32 Byte Kopf + 80*2*9*512 Nutzdaten. */
#define DIM_HDR   32
#define TRACKS    80
#define SIDES     2
#define SPT       9
#define SEKTOR    512
#define NUTZ      ((size_t)TRACKS * SIDES * SPT * SEKTOR)
#define GESAMT    (DIM_HDR + NUTZ)

static uint8_t *baue_dim(bool mit_magic)
{
    uint8_t *p = (uint8_t *)calloc(1, GESAMT);
    if (!p) return NULL;

    if (mit_magic) {
        p[0x00] = 0x42;          /* 'B' — ID Header, Hatari dim.c:36 */
        p[0x01] = 0x42;          /* 'B' */
    }
    p[0x02] = 1;                 /* Konfiguration automatisch erkannt */
    p[0x03] = 0;                 /* alle Sektoren enthalten */
    p[0x06] = SIDES - 1;         /* Hatari: "add 1 to get sides" */
    p[0x08] = SPT;
    p[0x0A] = 0;                 /* Startspur */
    p[0x0C] = TRACKS - 1;        /* Endspur */
    p[0x0D] = 0;                 /* DD */

    /* Etwas Inhalt, damit kein Nullpuffer geprueft wird. */
    for (size_t i = DIM_HDR; i < GESAMT; i += 512) p[i] = (uint8_t)(i / 512);
    return p;
}

static bool probe(const uint8_t *daten, int *konfidenz)
{
    *konfidenz = 0;
    if (!uft_format_plugin_dim_atari.probe) return false;
    return uft_format_plugin_dim_atari.probe(daten, GESAMT, GESAMT, konfidenz);
}

/* ── Der Schreibpfad (MF-688) ────────────────────────────────────────
 *
 * Die Probe zu haerten reicht nicht. `open()` prueft seinerseits kein
 * Magic — es liest den Kopf, prueft Geometrie-Plausibilitaet und oeffnet
 * die Datei bei Bedarf im SCHREIBMODUS ("r+b"). Wer das Plugin
 * unmittelbar waehlt (Registry umgangen, ausdrueckliche Formatwahl, ein
 * Fuzzer), bekommt fuer jede Fremddatei passender Laenge ein offenes
 * Schreibziel — und `write_track()` schreibt dann Sektoren hinein.
 *
 * Das ist kein Variantenthema. Das laeuft gegen "Kein Bit verloren.
 * Keine stille Veraenderung": ein Benutzer mit einer Nicht-DIM-Datei
 * richtiger Laenge im falschen Dialog ueberschreibt sie kommentarlos.
 *
 * Der Test schreibt darum ABSICHTLICH echt: er legt eine Fremddatei mit
 * erkennbarem Inhalt an, laesst den Schreibversuch laufen und sieht
 * danach nach, ob auch nur ein Byte anders ist. Eine Ablehnung, die
 * vorher schon geschrieben hat, ist keine Ablehnung. */
static bool schreibversuch_veraendert_die_datei(const char *pfad)
{
    uint8_t *fremd = baue_dim(false);       /* passende Laenge, kein Magic */
    if (!fremd) return false;

    /* Wiedererkennbar machen — aber NUR in den Nutzdaten.
     *
     * Der erste Entwurf setzte die ersten 64 Byte auf 0x5A und bestand
     * prompt: `open()` scheiterte dann an der zerstoerten GEOMETRIE, nicht
     * am fehlenden Magic. Der Test war gruen und hat nichts gezeigt —
     * dieselbe Falle, gegen die dieser Test antritt, eine Ebene hoeher.
     *
     * Jetzt bleibt der Kopf vollstaendig gueltig; einzig die zwei
     * Magic-Bytes fehlen. Damit kann `open()` nur noch aus einem Grund
     * ablehnen. */
    memset(fremd + DIM_HDR, 0x5A, 512);

    FILE *f = fopen(pfad, "wb");
    if (!f) { free(fremd); return false; }
    fwrite(fremd, 1, GESAMT, f);
    fclose(f);

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = false;

    uft_error_t rc = uft_format_plugin_dim_atari.open(&disk, pfad, false);
    printf("  open(schreibend) auf Fremddatei: rc=%d%s\n", (int)rc,
           rc == UFT_OK ? "  <- angenommen" : "  <- abgelehnt");

    if (rc == UFT_OK) {
        uint8_t inhalt[512];
        memset(inhalt, 0xCC, sizeof(inhalt));
        uft_sector_t sek;
        memset(&sek, 0, sizeof(sek));
        sek.data = inhalt;
        sek.data_len = sizeof(inhalt);

        uft_track_t trk;
        memset(&trk, 0, sizeof(trk));
        trk.sectors = &sek;
        trk.sector_count = 1;

        if (uft_format_plugin_dim_atari.write_track)
            uft_format_plugin_dim_atari.write_track(&disk, 0, 0, &trk);
        if (uft_format_plugin_dim_atari.close)
            uft_format_plugin_dim_atari.close(&disk);
    }

    /* Nachsehen, nicht vermuten. */
    bool veraendert = false;
    f = fopen(pfad, "rb");
    if (f) {
        uint8_t *jetzt = (uint8_t *)malloc(GESAMT);
        if (jetzt && fread(jetzt, 1, GESAMT, f) == GESAMT)
            veraendert = (memcmp(jetzt, fremd, GESAMT) != 0);
        free(jetzt);
        fclose(f);
    }
    free(fremd);
    return veraendert;
}

/* ── Die fremde Hand (MF-690) ────────────────────────────────────────
 *
 * Alles oben ist synthetisch: der Test baut die Koepfe selbst. Das
 * genuegt fuer die Frage "lehnt die Probe ab, was keines ist" — aber es
 * beweist nicht, dass wir ein DIM lesen, das jemand ANDERES geschrieben
 * hat. Genau diese Luecke nennt VERIFICATION_TIERS.md T3.
 *
 * Seit MF-690 liegen zwei Abbilder im freien Korpus, erzeugt von HxC:
 *
 *     hxcfe.exe -finput:tests/differential/corpus/sources/atarist_dd.st
 *               -conv:ATARIST_DIM
 *
 * Die Quelle ist unser eigener Differential-Korpus, das ZIELFORMAT aber
 * von fremder Hand. Dazu ein Gegenstueck, bei dem nur die zwei
 * Magic-Bytes genullt sind — sonst byteidentisch.
 *
 * Das Paar ist die staerkere Fassung des Zwei-Byte-Beweises von oben:
 * dort baue ich beide Seiten selbst, hier kommt die gueltige Seite von
 * einem fremden Werkzeug. Wenn unsere Probe sie annimmt und das
 * Gegenstueck ablehnt, liegt das nicht an meinem Kopf-Nachbau. */
static void pruefe_korpus(void)
{
    char gut[1024], boese[1024];
    snprintf(gut, sizeof(gut), "%s/hxcfe_atarist_dd.dim", UFT_CORPUS_DIR);
    snprintf(boese, sizeof(boese), "%s/hxcfe_atarist_dd_nomagic.dim",
             UFT_CORPUS_DIR);

    FILE *f = fopen(gut, "rb");
    if (!f) {
        printf("  FAIL %s fehlt — die Datei liegt im FREIEN Korpus\n", gut);
        fehler++;
        return;
    }
    uint8_t kopf[64];
    size_t gelesen = fread(kopf, 1, sizeof(kopf), f);
    fseek(f, 0, SEEK_END);
    long groesse = ftell(f);
    fclose(f);
    if (gelesen != sizeof(kopf)) { printf("  FAIL Korpus-DIM zu kurz\n");
                                   fehler++; return; }

    int k = 0;
    bool ja = uft_format_plugin_dim_atari.probe(kopf, sizeof(kopf),
                                                (size_t)groesse, &k);
    PRUEFE(ja, "das HxC-erzeugte DIM wird NICHT angenommen (Konfidenz %d). "
               "Ein fremdes Werkzeug hat es geschrieben; lehnen wir es ab, "
               "ist unsere Pruefung zu streng", k);
    if (ja) printf("  ok   HxC-DIM angenommen, Konfidenz %d\n", k);

    /* Und das Gegenstueck, das sich nur in zwei Byte unterscheidet. */
    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    uft_error_t rc = uft_format_plugin_dim_atari.open(&disk, boese, true);
    PRUEFE(rc != UFT_OK,
           "das Gegenstueck OHNE Magic wird geoeffnet (rc=%d) — dann "
           "entscheidet die Dateigroesse und nicht der Kopf", (int)rc);
    if (rc == UFT_OK && uft_format_plugin_dim_atari.close)
        uft_format_plugin_dim_atari.close(&disk);
    if (rc != UFT_OK)
        printf("  ok   Gegenstueck ohne Magic abgelehnt\n");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("DIM: ohne `BB` weder lesen noch schreiben (MF-687/688)\n\n");

    uint8_t *mit  = baue_dim(true);
    uint8_t *ohne = baue_dim(false);
    if (!mit || !ohne) { printf("FEHLER: kein Speicher\n"); return 1; }

    /* Gegenprobe auf den Messaufbau selbst: die beiden duerfen sich in
     * GENAU zwei Bytes unterscheiden. Waere mehr verschieden, koennte
     * ein anderer Grund das Ergebnis erklaeren, und der Test wuerde das
     * Richtige aus dem Falschen schliessen. */
    size_t abweichungen = 0;
    for (size_t i = 0; i < GESAMT; i++)
        if (mit[i] != ohne[i]) abweichungen++;
    PRUEFE(abweichungen == 2,
           "die beiden Abbilder unterscheiden sich in %zu Byte, erlaubt "
           "sind genau 2 — sonst erklaert der Test nicht, was er misst",
           abweichungen);

    int k_mit = 0, k_ohne = 0;
    bool a_mit  = probe(mit,  &k_mit);
    bool a_ohne = probe(ohne, &k_ohne);

    printf("  mit  `BB`: angenommen=%s Konfidenz=%d\n",
           a_mit ? "ja" : "nein", k_mit);
    printf("  ohne `BB`: angenommen=%s Konfidenz=%d\n",
           a_ohne ? "ja" : "nein", k_ohne);

    PRUEFE(a_mit,
           "ein gueltiges DIM muss angenommen werden — sonst waere die "
           "Magic-Pruefung zu streng und wuerde echte Dateien abweisen");

    PRUEFE(!a_ohne,
           "eine Datei OHNE das Magic wird als DIM angenommen. Drei "
           "unabhaengige Umsetzungen lehnen sie ab (Hatari dim.c:75, HxC "
           "dim_loader.c:74/110, Jacknife dllmain.c:590). Ohne diese "
           "Pruefung wird jede Datei passender Laenge als DIM gelesen und "
           "ihre ersten 32 Byte als Geometrie ausgelegt");

    if (a_mit && !a_ohne)
        printf("  ok   das Magic entscheidet, nicht die Dateigroesse\n");

    free(mit); free(ohne);

    /* ── Die fremde Hand (MF-690) ──────────────────────────────────── */
    printf("\n");
    pruefe_korpus();

    /* ── Der Schreibpfad (MF-688) ─────────────────────────────────── */
    printf("\n");
    const char *tmp = "uft_dim_fremd.bin";
    bool veraendert = schreibversuch_veraendert_die_datei(tmp);
    remove(tmp);

    PRUEFE(!veraendert,
           "eine FREMDE Datei passender Laenge wurde beschrieben. `open()` "
           "prueft kein Magic und liefert ein Schreibziel; `write_track()` "
           "schreibt hinein. Das ist stille Veraenderung fremder Daten und "
           "laeuft gegen Prinzip 1");
    if (!veraendert)
        printf("  ok   die Fremddatei ist unveraendert geblieben\n");

    printf("\n%s (%d Abweichungen)\n",
           fehler ? "FEHLGESCHLAGEN" : "OK", fehler);
    return fehler ? 1 : 0;
}
