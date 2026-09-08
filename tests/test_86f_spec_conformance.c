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

/* ── Die echte Datei: Pfad, Erwartung, Prüfung (MF-960) ───────────── */

/* Der Pfad kommt aus dem Bausystem: `UFT_CORPUS_RESTRICTED_DIR` ist die
 * Konvention dieses Baums für den rechtlich geführten Teil des Korpus.
 * Der Rückfall auf einen relativen Pfad ist da, damit ein Übersetzen
 * ohne CMake nicht still etwas anderes liest — er findet die Datei
 * dann in aller Regel nicht, und der Abschnitt überspringt sich. */
#ifndef UFT_CORPUS_RESTRICTED_DIR
#define UFT_CORPUS_RESTRICTED_DIR "tests/corpus"
#endif

#define F86_ECHT UFT_CORPUS_RESTRICTED_DIR \
                 "/fluxfox_sector_test/sector_test_360k.86f"

/* Am Container gemessen, bevor eine Zeile Code geschrieben wurde. */
#define ECHT_ZYLINDER      86      /* 172 belegte Eintraege / 2 Seiten */
#define ECHT_KOEPFE         2
#define ECHT_SEKTOREN       9      /* 54 Sync-Treffer / 3 / 2 Marken */
#define ECHT_SEKTORGROESSE  512

/* Das Oracle: Sektor k des 360K-Abbilds traegt 512-mal (k mod 256).
 * Auf Zylinder 0 Kopf 0 liegen die Sektoren 1..9 = Abbild-Sektoren 0..8,
 * also die Bytes 0x00 bis 0x08. */
static void echte_datei(void)
{
    const char *pfad = F86_ECHT;
    FILE *pruef = fopen(pfad, "rb");
    if (!pruef) {
        printf("\n  [UEBERSPRUNGEN] %s fehlt.\n", F86_ECHT);
        printf("  Beschaffung: tests/corpus_manifest/manifest.json\n");
        return;
    }
    fclose(pruef);

    printf("\n  ── 5 · Die ECHTE Datei ──────────────────────────────\n");

    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    disk.read_only = true;

    uft_error_t rc = uft_format_plugin_86f.open(&disk, pfad, true);
    PRUEFE(rc == UFT_OK,
           "das Plugin nimmt die echte 86F-Datei nicht an (rc=%d). Genau "
           "das war der Befund von MF-707/708: es sucht ein Magic, das in "
           "keiner echten Datei steht", (int)rc);
    if (rc != UFT_OK) return;

    printf("     Geometrie: %u Zyl x %u Koepfe x %u Sekt x %u B\n",
           disk.geometry.cylinders, disk.geometry.heads,
           disk.geometry.sectors, disk.geometry.sector_size);

    PRUEFE(disk.geometry.cylinders == ECHT_ZYLINDER,
           "Zylinder: %u statt %d — die Offset-Tabelle fuehrt 172 belegte "
           "Eintraege bei zwei Seiten", disk.geometry.cylinders,
           ECHT_ZYLINDER);
    PRUEFE(disk.geometry.heads == ECHT_KOEPFE,
           "Koepfe: %u statt %d (Disk-Flag Bit 3)",
           disk.geometry.heads, ECHT_KOEPFE);

    /* Die Abbildung 86F-Spur -> Abbild-Sektor, und WARUM sie so lautet.
     *
     * Das lineare .img legt die Sektoren in CHS-Reihenfolge ab:
     *     Abbild-Sektor = (Zylinder * Koepfe + Kopf) * 9 + (R - 1)
     *
     * Die 86F-Datei fuehrt aber 86 Zylinder fuer eine 40-spurige
     * Diskette. Gemessen an den Synchronpositionen tragen 86F-Zylinder 0
     * und 1 DASSELBE Muster — die Aufnahme stammt aus einem 80-spurigen
     * Laufwerk, jede physische Spur steht zweimal da. Also:
     *
     *     Abbild-Zylinder = 86F-Zylinder / 2
     *
     * Das ist eine HYPOTHESE, und sie wird hier geprueft statt gesetzt:
     * schlaegt sie fehl, faellt der Test und sagt, an welcher Spur. */
    static const struct { int cyl, head; } proben[] = {
        { 0, 0 }, { 0, 1 },     /* erste Spur, beide Seiten            */
        { 1, 0 },               /* die Verdopplung: wie Zylinder 0     */
        { 2, 0 }, { 2, 1 },     /* Abbild-Zylinder 1                   */
        { 20, 0 },              /* Abbild-Zylinder 10, mitten drin     */
        { 78, 1 },              /* Abbild-Zylinder 39, die letzte      */
    };
    unsigned spuren_ok = 0, sektoren_ok = 0, sektoren_gesamt = 0;

    for (size_t pi = 0; pi < sizeof proben / sizeof proben[0]; pi++) {
        const int c = proben[pi].cyl, h = proben[pi].head;
        uft_track_t t;
        memset(&t, 0, sizeof t);
        rc = uft_format_plugin_86f.read_track(&disk, c, h, &t);
        PRUEFE(rc == UFT_OK, "read_track(%d,%d) gab %d", c, h, (int)rc);
        if (rc != UFT_OK) continue;

        PRUEFE(t.sector_count == ECHT_SEKTOREN,
               "Zyl %d Kopf %d: %u Sektoren statt %d — im Bitstrom stehen "
               "54 Synchronmarken, also 9 Sektoren",
               c, h, (unsigned)t.sector_count, ECHT_SEKTOREN);

        const unsigned abbild_cyl = (unsigned)c / 2u;
        unsigned ok = 0;

        for (size_t i = 0; i < t.sector_count; i++) {
            const uft_sector_t *s = &t.sectors[i];
            sektoren_gesamt++;
            if (!s->data || s->data_len != ECHT_SEKTORGROESSE) {
                PRUEFE(0, "Zyl %d Kopf %d Sektor %u: %u Byte statt %d",
                       c, h, (unsigned)i, (unsigned)s->data_len,
                       ECHT_SEKTORGROESSE);
                continue;
            }
            const unsigned r = s->id.sector;
            if (r < 1 || r > ECHT_SEKTOREN) {
                PRUEFE(0, "Zyl %d Kopf %d: Sektornummer %u ausserhalb 1..%d",
                       c, h, r, ECHT_SEKTOREN);
                continue;
            }
            const unsigned lfd =
                (abbild_cyl * ECHT_KOEPFE + (unsigned)h) * ECHT_SEKTOREN
                + (r - 1u);
            const uint8_t soll = (uint8_t)(lfd % 256u);

            size_t abweichend = 0;
            for (size_t k = 0; k < s->data_len; k++)
                if (s->data[k] != soll) abweichend++;
            PRUEFE(abweichend == 0,
                   "Zyl %d Kopf %d Sektor R=%u: %u von %u Bytes weichen ab "
                   "(Abbild-Sektor %u, erwartet ueberall 0x%02X, "
                   "gelesen 0x%02X)", c, h, r, (unsigned)abweichend,
                   (unsigned)s->data_len, lfd, soll, s->data[0]);
            if (abweichend == 0) { ok++; sektoren_ok++; }
        }
        if (ok == ECHT_SEKTOREN) spuren_ok++;

        for (size_t i = 0; i < t.sector_count; i++) free(t.sectors[i].data);
        free(t.sectors);
    }

    printf("     %u von %u Spuren vollstaendig; %u von %u Sektoren "
           "byteweise nach der Formel\n",
           spuren_ok, (unsigned)(sizeof proben / sizeof proben[0]),
           sektoren_ok, sektoren_gesamt);

    uft_format_plugin_86f.close(&disk);
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
    /* MF-961: umgedreht. Bis dahin stand hier `PRUEFE(!ja_spec, ...)` —
     * die Zusicherung, dass jede echte 86F-Datei durchfällt. Sie hat
     * ihren eigenen Anlass überlebt: der Leser ist neu gefasst.
     *
     * Die Konfidenz ist hier 45 und nicht 95, und das ist richtig: der
     * erzeugte Kopf trägt einen Spur-Offset, hinter dem in diesem
     * kleinen Puffer nichts steht. Nach der Skala von MF-729 heißt
     * 30..49 „nur die Kennung", 80..100 „Merkmal getroffen". Die hohe
     * Zahl beansprucht das Plugin erst, wenn die Offset-Tabelle wirklich
     * aufgeht — siehe die echte Datei in Abschnitt 5. */
    PRUEFE(ja_spec, "das Plugin weist den spec-konformen Kopf ab");
    PRUEFE(konf >= 30 && konf < 50,
           "Konfidenz %d fuer einen Kopf ohne auswertbare Spurtabelle — "
           "nach MF-729 gehoert der in 30..49", konf);

    /* ── 2 · Und der Kopf, den der Baum erfunden hat ────────────────── */
    kopf_nach_spec(buf, sizeof(buf), "86BX");
    konf = -1;
    bool ja_baum = uft_format_plugin_86f.probe(buf, sizeof(buf),
                                                sizeof(buf), &konf);
    printf("  Kopf mit \"86BX\"          -> Probe: %s (Konfidenz %d)\n",
           ja_baum ? "JA" : "NEIN", konf);
    /* MF-961: umgedreht. Hier stand `PRUEFE(ja_baum && konf == 98, ...)`
     * — der dokumentierte Ist-Stand, in dem das ERFUNDENE Magic bejaht
     * wurde, und zwar mit höherer Konfidenz als jedes andere Plugin für
     * dieselben Bytes. */
    PRUEFE(!ja_baum,
           "\"86BX\" wird immer noch angenommen — dieses Magic steht in "
           "keiner 86F-Datei");

    /* ── 3 · Die Umkehrung ist der eigentliche Befund ───────────────── */
    /* MF-961: hier stand `PRUEFE(ja_baum != ja_spec, ...)`, weil der
     * Befund war, dass die beiden Köpfe GENAU UMGEKEHRT zur
     * Spezifikation behandelt wurden. Jetzt ist die Unterscheidung
     * richtig herum, und das ist die Zusicherung. */
    PRUEFE(ja_spec && !ja_baum,
           "die Unterscheidung stimmt nicht: spec-Kopf %s, erfundener "
           "Kopf %s", ja_spec ? "JA" : "NEIN", ja_baum ? "JA" : "NEIN");

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
    /* MF-961: hier stand `PRUEFE(pc_spec != ja_spec, ...)` — die
     * Zusicherung, dass die beiden Leser sich WIDERSPRECHEN. Genau das
     * war der Befund von MF-708: der Leser mit dem richtigen Magic war
     * der ohne Tür. Die Spaltung ist aufgelöst.
     *
     * Was damit NICHT erledigt ist: `src/formats/pc/uft_86f.c` bleibt
     * unregistriert und ohne Aufrufer. Zwei Leser für ein Format sind
     * einer zu viel — welcher geht, ist eine eigene Entscheidung. */
    PRUEFE(pc_spec == ja_spec,
           "die beiden Leser widersprechen sich wieder: pc/uft_86f.c "
           "sagt %s, das Plugin %s",
           pc_spec ? "JA" : "NEIN", ja_spec ? "JA" : "NEIN");
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

    /* ── 5 · Die ECHTE Datei (MF-960) ───────────────────────────────
     *
     * Alles bisher Geprüfte ist Struktur gegen Spezifikation: erzeugte
     * Köpfe, die zeigen, dass der Leser sie verfehlt. Kein Byte davon
     * stammt von einem Gerät.
     *
     * `tests/corpus/fluxfox_sector_test/sector_test_360k.86f` ist eine
     * echte 5,25″-360K-Diskette (dbalsom/fluxfox, MIT; Herkunft und
     * Prüfsumme im Korpus-Manifest). An ihr ist die Spezifikation
     * nachgemessen, bevor hier eine Zeile stand:
     *
     *     Magic 86BF, Version 2.0C, Disk-Flags 0x1088
     *     Offset-Tabelle 512 Einträge, davon 172 belegt (86 Zyl × 2)
     *     Spurkopf: Flags 0x000A = MFM, 250 kbps
     *     Ende der letzten Spur == Dateigröße, Differenz 0
     *     Bitstrom MSB zuerst: 54 Treffer 0x4489 je Spur
     *              = 18 Marken × 3 = 9 Sektoren
     *
     * Der INHALT folgt einer Formel statt einem Werkzeug: Sektor k
     * trägt 512-mal das Byte (k mod 256). Selbst nachgerechnet an der
     * beiliegenden `.img` — 0 von 720 Sektoren weichen ab. Ein Oracle,
     * das man nachrechnen statt ausführen kann.
     *
     * Ohne Korpus überspringt sich dieser Abschnitt benannt. */
    echte_datei();

    /* MF-961: hier stand „Was die gruene Ampel NICHT heisst: dass 86F
     * gelesen werden kann" — richtig, solange der Leser die
     * Spezifikation verfehlte. Jetzt liest er, und der Satz waere eine
     * Untertreibung in dieselbe Richtung, in die der alte Zustand
     * uebertrieb. Also gezogen. */
    printf("\n  Was die gruene Ampel jetzt heisst: der Container ist "
           "nach der Spezifikation\n"
           "  des Urhebers gelesen, und eine ECHTE 360K-Datei geht "
           "byteweise gegen eine\n"
           "  unabhaengig nachgerechnete Formel auf.\n"
           "\n"
           "  Was sie NICHT heisst:\n"
           "    - dass 86F GESCHRIEBEN werden kann (write_track sagt "
           "ausdruecklich ab)\n"
           "    - dass FM-, M2FM- oder GCR-Spuren gelesen werden — "
           "dafuer fehlt der\n"
           "      Bitstrom-Dekoder; solche Spuren liefern 0 Sektoren "
           "mit benanntem Grund\n"
           "    - dass die Oberflaechenbeschreibung (schwache Bits) "
           "ausgewertet wird\n"
           "    - dass der zweite, unregistrierte Leser in "
           "src/formats/pc/uft_86f.c\n"
           "      damit erledigt waere\n");

    printf("\n%s (%d Abweichungen)\n", fehler ? "ROT" : "GRUEN", fehler);
    return fehler ? 1 : 0;
}
