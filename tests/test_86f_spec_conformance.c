/**
 * @file test_86f_spec_conformance.c
 * @brief 86F gegen die Spezifikation von 86Box selbst (MF-707)
 *
 * ── Die Quelle ──────────────────────────────────────────────────────────
 *
 * `docs/dev/formats/86f.rst` im **eigenen Dokumentations-Repositorium von
 * 86Box** (github.com/86Box/docs, abgerufen 2026-08-30). Das ist keine
 * Sekundaerquelle und keine Rueckentwicklung: es ist die Beschreibung des
 * Formats durch dessen Urheber. Kopf-Aufbau woertlich:
 *
 *     00000000: Magic 4 bytes ("86BF")
 *     00000004: Minor version (0C)
 *     00000005: Major version (02)
 *     00000006: Disk flags (16-bit)
 *     00000008: Offsets of tracks
 *
 * Der feste Kopf vor der Spur-Offset-Tabelle ist damit **8 Byte** lang,
 * und die Tabelle besteht aus **32-Bit-Offsets**. 86F speichert
 * FM-/MFM-**Transitionen** — es ist ein Oberflaechenformat, kein
 * CHS-Sektorformat.
 *
 * ── Die zweite, unabhaengige Quelle (MF-708) ────────────────────────────
 *
 * Die Zwei-Quellen-Regel verlangt eine zweite Quelle, die von der ersten
 * nichts weiss. `Digitoxin1/DiskImageTool` (GPL-3.0, VB.NET, Windows)
 * bringt einen **eigenstaendigen** 86F-Leser mit — 871 Zeilen unter
 * `ImageFormats/86F/`, geschrieben ohne Kenntnis dieses Baums und ohne
 * Bezug auf den Text oben. Gemessen im Klon:
 *
 *     86FImage.vb:8    Private Const FILE_SIGNATURE = "86BF"
 *     86FImage.vb:348  _MinorVersion  = Buffer(4)
 *     86FImage.vb:349  _MajorVersion  = Buffer(5)
 *     86FImage.vb:350  _DiskFlags     = BitConverter.ToUInt16(Buffer, 6)
 *     86FImage.vb:359  Dim Pos = 8            ' Beginn der Offset-Tabelle
 *     86FImage.vb:363  Offset = BitConverter.ToUInt32(Buffer, Pos)
 *
 * Sie bestaetigt die Spezifikation Feld fuer Feld: Magic, beide
 * Versionsbytes, die 16-Bit-Disk-Flags bei 6, den Tabellenbeginn bei 8
 * und die 32-Bit-Offsets. Damit steht der Befund unten nicht mehr auf
 * einer Quelle, sondern auf zweien, die einander nicht kennen.
 *
 * **Kanal:** GPL-3.0 heisst Zone GELB — lesen und beschreiben ja,
 * portieren nein. Uebernommen wurde nichts; belegt wurden **Tatsachen**
 * (welches Byte welche Bedeutung traegt), nicht Ausdruck. Eine
 * Neufassung des Lesers folgt der 86Box-Spezifikation, nicht dieser
 * Datei.
 *
 * ── Was `uft_86f_plugin.c` stattdessen annimmt ──────────────────────────
 *
 * | Stelle | Baum | Spezifikation |
 * |---|---|---|
 * | Magic | `"86BX"` (`:20`) | `"86BF"` |
 * | Kopfgroesse | 32 (`:22`) | 8 |
 * | Byte 6 | `disk_type` (`:66`) | untere Haelfte der Disk flags |
 * | Byte 7 | `sides` (`:68`) | obere Haelfte der Disk flags |
 * | Byte 8 | `tracks` (`:67`) | **erstes Byte des ersten Spur-Offsets** |
 * | Spurtabelle | ab 32, Eintraege 12 Byte: offset(4) + length(4) + flags(1) + sectors(1) + rpm(2) (`:105-107`) | ab 8, Eintraege sind 32-Bit-Offsets |
 * | Geometrie | CHS aus einem "disk type byte" (`:32-43`) | das Format kennt keinen solchen Typ |
 *
 * Keine dieser Annahmen laesst sich aus der Spezifikation herleiten. Die
 * 12-Byte-Eintragsstruktur mit benannten Unterfeldern ist die Signatur
 * aus FMT-2/3/10/11/12: plausibel aussehend und erfunden.
 *
 * ── Die praktische Folge, und sie ist die entscheidende ─────────────────
 *
 * Weil die Probe auf `"86BX"` besteht, **weist sie jede echte 86F-Datei
 * ab**. Das Plugin ist im Betrieb wirkungslos — und meldet dabei in
 * `uft_format_plugin_86f.features` „Read: SUPPORTED", „Write: SUPPORTED",
 * „Flux: SUPPORTED" und traegt `UFT_FORMAT_CAP_READ | ..._WRITE |
 * ..._FLUX | ..._VERIFY`.
 *
 * Das ist „Bestand, nicht Faehigkeit" (P0-2) in seiner unangenehmsten
 * Form: nicht bloss unerreichbar, sondern **angekuendigt**.
 *
 * ── Und der zweite Leser, den MF-622 uebersehen hat (MF-708) ────────────
 *
 * Beim Nachpruefen des Tabellenbeginns fiel auf, dass dieser Baum **zwei**
 * 86F-Leser baut:
 *
 * | | Spezifikation + DiskImageTool | `86box/uft_86f_plugin.c` | `pc/uft_86f.c` |
 * |---|---|---|---|
 * | im Build | — | ja (`.pro:906`) | ja (`.pro:3249`) |
 * | **registriert** | — | **ja** (Registry) | **nein** |
 * | Aufrufer | — | ueber Plugin-Zeiger | **keiner**, gemessen |
 * | Magic | `86BF` | `86BX` ✗ | `86BF` ✓ |
 * | Byte 4/5 | Minor / Major | — ✗ | Version LE16 ✓ |
 * | Byte 6 | Disk flags LE16 | `disk_type` u8 ✗ | Flags LE16 ✓ |
 * | Byte 8 | **Spur-Offset-Tabelle** | `tracks` u8 ✗ | `disk_type` u8 ✗ |
 * | Byte 9-13 | (Tabelle) | — | encoding/rpm/tracks/sides/bitcell ✗ |
 *
 * Der Leser mit dem **richtigen** Erkennungsmerkmal ist der, der keine
 * Tuer hat. Er kommt acht Byte weit korrekt und erfindet dann einen
 * verlaengerten Kopf aus sechs Feldern, wo die Spezifikation die
 * Offset-Tabelle beginnen laesst — dieselbe Fabrikationsklasse, nur
 * einen Schritt spaeter.
 *
 * Der Kopf von `uft_86f_plugin.c` sagte bis MF-708, es trage die
 * 86F-Unterstuetzung „allein". MF-622 hatte **einen** unerreichbaren
 * 86F-Leser geloescht und daraus geschlossen, es sei der letzte
 * gewesen; ein zweiter, 477 Zeilen, stand die ganze Zeit im Build.
 * Das ist zum elften Mal in diesem Baum die **Aufzaehlung bekannter
 * Faelle** statt einer Messung (MF-567/578/598/633/651/652/668/671/
 * 678/703).
 *
 * ── Warum hier KEIN Fix steht ───────────────────────────────────────────
 *
 * Das Magic zu berichtigen waere eine Zeile — und **schlimmer als der
 * jetzige Zustand**. Heute lehnt das Plugin echte Dateien ab; mit
 * richtigem Magic naehme es sie an und liese sie mit einem Kopf-Offset
 * von 32 statt 8 und einer erfundenen Spurtabelle. `.write_track` ist
 * verdrahtet. Aus „wirkungslos" wuerde „nimmt an und zerlegt falsch,
 * mit Schreibpfad".
 *
 * Dieselbe Kopplung wie bei `hardsector` (MF-706): erst die Struktur,
 * dann das Erkennungsmerkmal. Was hier zu tun ist, ist kein Tagesrand,
 * sondern eine Neufassung gegen die Spezifikation — mit Rotbeweis
 * zuerst, und das ist diese Datei.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/formats/pc/uft_86f.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

extern const uft_format_plugin_t uft_format_plugin_86f;

static int fehler = 0;

#define PRUEFE(bed, ...) do {                                            \
    if (!(bed)) { printf("  FAIL "); printf(__VA_ARGS__);                \
                  printf("\n"); fehler++; }                              \
} while (0)

/* Ein Kopf nach der Spezifikation: Magic, Minor 0x0C, Major 0x02,
 * Disk flags, dann 32-Bit-Spur-Offsets. */
static void kopf_nach_spec(uint8_t *b, size_t n, const char *magic)
{
    memset(b, 0, n);
    memcpy(b, magic, 4);
    b[4] = 0x0C;            /* Minor version, Spec */
    b[5] = 0x02;            /* Major version, Spec */
    b[6] = 0x00; b[7] = 0x00;   /* Disk flags (16 bit) */
    /* ab 0x08: Spur-Offsets, 32 bit LE — hier ein plausibler erster */
    b[8] = 0x00; b[9] = 0x10; b[10] = 0x00; b[11] = 0x00;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("86F gegen die Spezifikation von 86Box (MF-707)\n\n");

    uint8_t buf[512];
    int konf;

    /* ── 1 · Der Kopf, den die Spezifikation vorschreibt ────────────── */
    kopf_nach_spec(buf, sizeof(buf), "86BF");
    konf = -1;
    bool ja_spec = uft_format_plugin_86f.probe(buf, sizeof(buf),
                                                sizeof(buf), &konf);
    printf("  Kopf nach Spec (\"86BF\") -> Probe: %s (Konfidenz %d)\n",
           ja_spec ? "JA" : "NEIN", konf);
    PRUEFE(!ja_spec,
           "das Plugin nimmt jetzt echte 86F-Dateien an — dann ist das "
           "Magic berichtigt worden, und dieser Test gehoert mitgezogen "
           "(zusammen mit Kopfgroesse und Spurtabelle, sonst wird aus "
           "'weist ab' ein 'nimmt an und zerlegt falsch')");
    if (!ja_spec)
        printf("       ^ jede echte 86F-Datei faellt hier durch\n");

    /* ── 2 · Und der Kopf, den der Baum erfunden hat ────────────────── */
    kopf_nach_spec(buf, sizeof(buf), "86BX");
    konf = -1;
    bool ja_baum = uft_format_plugin_86f.probe(buf, sizeof(buf),
                                                sizeof(buf), &konf);
    printf("  Kopf mit \"86BX\"          -> Probe: %s (Konfidenz %d)\n",
           ja_baum ? "JA" : "NEIN", konf);
    PRUEFE(ja_baum && konf == 98,
           "erwartet war der dokumentierte Ist-Stand (JA, Konfidenz 98), "
           "gemessen %s/%d", ja_baum ? "JA" : "NEIN", konf);
    if (ja_baum)
        printf("       ^ mit Konfidenz 98 — hoeher als jedes andere "
               "Plugin fuer diese Bytes\n");

    /* ── 3 · Die Umkehrung ist der eigentliche Befund ───────────────── */
    PRUEFE(ja_baum != ja_spec,
           "beide Koepfe werden gleich behandelt — dann misst dieser Test "
           "nicht, was er zu messen vorgibt");
    if (ja_baum && !ja_spec)
        printf("\n  Genau umgekehrt zur Spezifikation: das erfundene "
               "Magic wird bejaht,\n"
               "  das echte abgewiesen.\n");

    /* ── 3b · Der zweite Leser im selben Baum (MF-708) ──────────────────
     *
     * `src/formats/pc/uft_86f.c` wird gebaut (`.pro:3249`), ist NICHT
     * registriert und hat keinen Aufrufer — aber sein Erkennungsmerkmal
     * ist das richtige. Beide Leser bekommen hier denselben Kopf. */
    int konf_pc_spec = -1, konf_pc_baum = -1;
    kopf_nach_spec(buf, sizeof(buf), "86BF");
    bool pc_spec = uft_pc86f_probe(buf, sizeof(buf), &konf_pc_spec);
    kopf_nach_spec(buf, sizeof(buf), "86BX");
    bool pc_baum = uft_pc86f_probe(buf, sizeof(buf), &konf_pc_baum);

    printf("\n  Derselbe Kopf, der andere Leser (pc/uft_86f.c, "
           "unregistriert):\n");
    printf("     \"86BF\" -> %s (Konfidenz %d)   |  \"86BX\" -> %s\n",
           pc_spec ? "JA" : "NEIN", konf_pc_spec,
           pc_baum ? "JA" : "NEIN");

    PRUEFE(pc_spec && !pc_baum,
           "der zweite Leser verhaelt sich nicht mehr spec-konform "
           "(erwartet: JA auf \"86BF\", NEIN auf \"86BX\") — dann ist "
           "einer der beiden Leser angefasst worden und dieser Test "
           "nachzuziehen");
    PRUEFE(pc_spec != ja_spec,
           "beide Leser antworten jetzt gleich — dann ist die Spaltung "
           "aufgeloest (gut) und dieser Test hat seinen Gegenstand "
           "verloren");
    if (pc_spec && !ja_spec)
        printf("       ^ genau umgekehrt zum registrierten Plugin: der "
               "Leser MIT\n"
               "         richtigem Magic ist der OHNE Tuer.\n");

    /* ── 4 · Was das Plugin dabei ANKUENDIGT ────────────────────────── */
    printf("\n  Angekuendigt in `uft_format_plugin_86f`:\n");
    for (size_t i = 0; i < uft_format_plugin_86f.feature_count; i++) {
        const uft_plugin_feature_t *f = &uft_format_plugin_86f.features[i];
        printf("     %-10s %s\n", f->name,
               f->status == UFT_FEATURE_SUPPORTED ? "SUPPORTED" : "—");
    }
    PRUEFE(uft_format_plugin_86f.write_track != NULL,
           "kein Schreibpfad mehr — dann ist der Befund entschaerft und "
           "dieser Test nachzuziehen");
    printf("     .write_track ist verdrahtet: %s\n",
           uft_format_plugin_86f.write_track ? "ja" : "nein");

    printf("\n  Was die gruene Ampel NICHT heisst: dass 86F gelesen "
           "werden kann.\n"
           "  Sie haelt fest, dass der Leser die Spezifikation "
           "verfehlt — Magic, Kopfgroesse,\n"
           "  Spurtabelle und Geometriemodell. Die Neufassung ist eine "
           "eigene Aufgabe;\n"
           "  ein Ein-Zeilen-Fix am Magic waere schlimmer als der "
           "jetzige Zustand.\n");

    printf("\n%s (%d Abweichungen)\n", fehler ? "ROT" : "GRUEN", fehler);
    return fehler ? 1 : 0;
}
