/**
 * @file test_disk_open_fuzz.c
 * @brief Missgebildete Abbilder durch den ECHTEN Oeffnungspfad (MF-512)
 *
 * ── Was dieser Test anders macht als test_format_probe_fuzz.c ────────────
 *
 * Der bestehende Fuzz-Test (MF-392) ist gut, hat aber zwei gemessene
 * Grenzen:
 *
 *   1. Er nennt **28** Plugins in einer Hand-Liste (`extern const
 *      uft_format_plugin_t uft_format_plugin_d64;` und so weiter). Die
 *      Registry fuehrt **137**. Rund vier Fuenftel der Plugins sah der
 *      Fuzzer nie — und eine Hand-Liste driftet zwangslaeufig: wer ein
 *      Plugin hinzufuegt, denkt nicht an diese Datei.
 *
 *   2. Er ruft ausschliesslich `probe`. Ein `probe` schaut auf einen
 *      Kopf und gibt bool zurueck. Die Fehler, die einen Benutzer treffen,
 *      stecken in `open`: dort wird geparst, gerechnet und **alloziert**.
 *
 * Dieser Test schliesst beide Luecken, und zwar ueber den Pfad, den ein
 * Benutzer wirklich nimmt:
 *
 *      uft_register_all_formats()      wie in main()
 *      -> Datei schreiben
 *      -> uft_disk_open(pfad, true)    Erkennung + Plugin-Auswahl + open
 *      -> uft_disk_get_geometry()
 *      -> uft_track_read()             mehrere Koordinaten, auch unsinnige
 *      -> uft_disk_close()
 *
 * Das ist ausdruecklich KEIN nachgebauter Pfad. Zweimal in diesem Baum
 * sind veroeffentlichte Zahlen falsch gewesen, weil ein Messaufbau den
 * Produktionspfad nachbaute statt ihn zu benutzen (MF-494, MF-497). Hier
 * wird dieselbe Funktion gerufen, die die Oberflaeche ruft.
 *
 * ── Was als Fehler gilt ──────────────────────────────────────────────────
 *
 * Ein Absturz, eine Endlosschleife, oder ein Speicherfehler unter den
 * ASan/UBSan-Laeufen der CI. **Nicht** als Fehler gilt: `uft_disk_open`
 * gibt NULL zurueck. Muell abzulehnen ist richtiges Verhalten, keine
 * Schwaeche — der Test zaehlt beide Ausgaenge und meldet die Verteilung,
 * damit sichtbar bleibt, wie viel ueberhaupt bis in ein `open` kommt.
 *
 * ── Reproduzierbarkeit ───────────────────────────────────────────────────
 *
 * Der Zufall ist ein eigener, gesetzter xorshift — nicht `rand()`. Der
 * gleiche Lauf erzeugt die gleichen Eingaben auf jeder Plattform, sonst
 * waere ein Fund nicht nachstellbar. Ein Fund druckt seinen Seed.
 *
 * ── WAS DIE VERBREITERUNG GEBRACHT HAT (MF-539 -> 543 -> 553) ───────
 *
 * MF-539 ergaenzte 25 Sonden-Kennungen und 7 Groessen-Tore, abgelesen aus
 * den probe-Funktionen. Damit erreichte der Fuzzer Plugins, die NIE eine
 * Eingabe gesehen hatten — und terminierte nicht mehr: 55 GB Working Set,
 * dann 0xC0000409 (STATUS_STACK_BUFFER_OVERRUN).
 *
 * Zwei echte Speicherfehler im Produktionspfad, beide Bestand und beide
 * nur deshalb nie aufgefallen, weil keine Eingabe je ihre Kennung trug:
 *
 *   QRST      Geometrie ungeprueft aus einem 22-Byte-Kopf.
 *             uft_disk_alloc() nimmt heads als uint8_t, die Fuellschleife
 *             indiziert mit dem ungekuerzten uint16 — ein Feld mit 88
 *             Plaetzen, beschrieben bis Index 599.
 *   NanoWasp  die Groessenpruefung war berechnet und NIE BENUTZT:
 *             1,1 TB Anspruch auf eine 80-Byte-Datei.
 *
 * Beide behoben in MF-543. Seither:
 *
 *      Sonde hat je zugestimmt : 128 von 137   (vorher 103)
 *      Abstuerze               : 0
 *      Vertragsverletzungen    : 0
 *
 * Nie erreicht bleiben 9 (MSA, DIM_ATARI, DC42, D77, D88, FDI_PC98, CFI,
 * Logical, POSIX). Sie verlangen einen Kopf, dessen INHALT und
 * DATEILAENGE zueinander passen — das kann ein Erzeuger aus "Kennung plus
 * beliebiger Rest" nicht liefern. POSIX kann ueberhaupt nie gewinnen, weil
 * seine Sonde unbedingt false liefert (MF-546).
 *
 * Die Lehre steht ueber dem Befund: der Fuzzer meldete vorher brav "kein
 * Absturz" und prueste dabei 103 von 137 Plugins. Eine gruene Zahl ueber
 * einer unvollstaendigen Menge ist keine Aussage ueber das Ganze.
 */

#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

/* ── Gesetzter Zufall, plattformgleich ─────────────────────────────────── */

static uint64_t rng_state = 0x9E3779B97F4A7C15ull;

static void rng_seed(uint64_t s)
{
    rng_state = s ? s : 0x9E3779B97F4A7C15ull;
}

static uint32_t rng_next(void)
{
    uint64_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    rng_state = x;
    return (uint32_t)(x >> 32);
}

/* ── Eingabe-Erzeugung ─────────────────────────────────────────────────── */

/* Gross genug fuer die groesste Korpusdatei (gw_amigados.hfe, 2049024 B).
 *
 * War 256 KB. Damit schnitt `fread(blob, 1, MAX_BLOB, ...)` acht der
 * zwoelf Korpusdateien still ab — und ihre Sonden lehnten die Truemmer
 * folgerichtig ab. Der erste Lauf meldete deshalb "D71 D80 D81 D82 ... nie
 * erreicht", obwohl fuer genau diese Formate echte Dateien im Repo liegen.
 *
 * Eine stille Kuerzung liest sich in der Auswertung wie Abdeckung, die es
 * nicht gibt. Wird eine Datei doch groesser als dieser Puffer, sagt der
 * Lauf das jetzt ausdruecklich. */
#define MAX_BLOB (4 * 1024 * 1024)

static uint8_t blob[MAX_BLOB];

/** Reiner Zufall — trifft selten eine Signatur, deckt dafuer die
 *  Ablehnungspfade ab. */
static size_t make_random(size_t size)
{
    for (size_t i = 0; i < size; i++)
        blob[i] = (uint8_t)rng_next();
    return size;
}

/** Ein echter Plugin-Kopf, danach Muell. Das ist die gefaehrliche Sorte:
 *  die Erkennung greift, und der Parser laeuft auf unsinnigen Feldern
 *  weiter — genau dort entstehen Ueberlaeufe. */
static size_t make_signature_then_garbage(const char *sig, size_t siglen,
                                          size_t size)
{
    if (siglen > size) siglen = size;
    memcpy(blob, sig, siglen);
    for (size_t i = siglen; i < size; i++)
        blob[i] = (uint8_t)rng_next();
    return size;
}

/** Nullen: laesst Laengen- und Zaehlerfelder auf 0 laufen. Klassische
 *  Quelle von Division durch null und Schleifen ohne Abbruch. */
static size_t make_zeros(size_t size)
{
    memset(blob, 0, size);
    return size;
}

/** Nur 0xFF: laesst jedes Feld auf sein Maximum laufen. Klassische
 *  Quelle von Ueberlauf in `count * size`-Rechnungen. */
static size_t make_ones(size_t size)
{
    memset(blob, 0xFF, size);
    return size;
}

/* Koepfe, die in diesem Baum als Format-Signaturen dokumentiert sind.
 * Sie stehen hier NICHT, um ein Format zu pruefen, sondern um die
 * Erkennung absichtlich greifen zu lassen — der interessante Teil kommt
 * danach. */
static const struct { const char *sig; size_t len; const char *label; } SIGS[] = {
    { "SCP",           3, "SCP"      },
    { "HXCPICFE",      8, "HFE"      },
    { "HXCHFEV3",      8, "HFEv3"    },
    { "IMD ",          4, "IMD"      },
    { "TD",            2, "TD0"      },
    { "GCR-1541",      8, "G64"      },
    { "WOZ1",          4, "WOZ1"     },
    { "WOZ2",          4, "WOZ2"     },
    { "A2R2",          4, "A2R"      },
    { "MOOF",          4, "MOOF"     },
    { "MV - CPCEMU",  11, "DSK"      },
    { "EXTENDED",      8, "EDSK"     },
    { "\x96\x02",   2, "ATR"      },   /* ATR-Magie 0x0296, little-endian */
    { "Reduced MFM",  11, "DMK"      },
    { "CAPS",          4, "IPF"      },
    { "KryoFlux",      8, "KryoFlux" },
    /* MF-521: aus den Sonden gelesen mit scripts/audit_probe_magic.py.
     * Vorher fehlten sie, und die zugehoerigen Plugins waren fuer den
     * Fuzzer unerreichbar — Zufallsbytes treffen eine Kennung nie. */
    { "86BX",              4, "86F"      },
    { "UAE-1ADF",          8, "ExtADF"   },
    { "M2I\0",             4, "M2I"      },
    { "FDI",               3, "FDI"      },
    { "GCR-1571",          8, "G71"      },
    { "LDB\1",             4, "LDB"      },
    /* MF-539: hier stand `{ "MAME FLOPPY IMAGE", 17, "MFI" }`. Diese Kennung
     * steht in src/formats/mame/uft_mame_mfi.c — einer Datei, die zwar in
     * UnifiedFloppyTool.pro:2552 gelistet ist, aber KEINE
     * `uft_format_plugin_t`-Struktur enthaelt und darum in keiner Registry
     * steht (gemessen: `grep -n "uft_format_plugin_t" src/formats/mame/
     * uft_mame_mfi.c` liefert nichts). Das registrierte MFI-Plugin ist
     * src/formats/mfi/uft_mfi.c:277, und dessen Sonde vergleicht
     * MFI_MAGIC = "MAMEFLOP", 8 Byte (uft_mfi.c:37/78). Der Eintrag zeigte
     * also auf tote Nachbarschaft: er sah aus wie Abdeckung fuer MFI und
     * war keine. */
    { "MAMEFLOP",          8, "MFI"      },
    { "SAD!",              4, "SAD"      },
    { "SINCLAIR",          8, "SCL"      },

    /* MF-539: 18 weitere Kennungen, je an der genannten Stelle nachgelesen,
     * nicht aus einer Formatliste abgeschrieben. Vorher stimmte fuer diese
     * 18 Plugins keine Sonde je zu — sie waren im Lauf als "NIE erreicht"
     * ausgewiesen, also ungeprueft. */
    { "AT8X",              4, "ATX"      },  /* uft_atx.c:61  0x58385441 LE  */
    { "APRO",              4, "PRO"      },  /* uft_pro_plugin.c:13 0x4F525041 LE */
    { "RSY\0",             4, "STX"      },  /* uft_stx_plugin.c:12-15 'R','S','Y',0 */
    { "2IMG",              4, "2IMG"     },  /* uft_2img.c:28 0x474D4932 LE */
    { "DMS!",              4, "DMS"      },  /* uft_dms_plugin.c:22 */
    { "CQ\x14",            3, "CQM"      },  /* uft_cqm.c:132 'C','Q',0x14 */
    { "UDI!",              4, "UDI"      },  /* uft_udi_plugin.c:29 */
    { "dk",                2, "VDK"      },  /* uft_vdk_plugin.c:12 0x6B64 LE16 */
    { "T98FDDIMAGE.R0",   14, "NFD"      },  /* uft_nfd_plugin.c:90-93 */
    { "PRI ",              4, "PRI"      },  /* uft_pri.c:29 */
    { "\x1F\xA6\xDE\xBA\xCC\x13\x7D\x74", 8, "CAS" },  /* uft_cas.c:32 */
    { "ACT Apricot disk image\x1A\x04", 24, "ApriDisk" },
                                             /* uft_apridisk.h:29 ("\032\004") */
    { "RCPM",              4, "RCPMFS"   },  /* uft_rcpmfs.h:32 */
    { "nanowasp floppy image\r\n\x1A", 24, "NanoWasp" },
                                             /* uft_nanowasp.h:24 ("\r\n\032") */
    { "QRST",              4, "QRST"     },  /* uft_qrst.h:24 */
    { "SAP\0",             4, "SAP"      },  /* uft_sap_plugin.c:69-73; Byte 3
                                              * muss SAP_VERSION_FM (0x00)
                                              * oder _MFM (0x01) sein */
    { "FDS\x1A",           4, "FDS"      },  /* uft_fds_plugin.c:30 */
};
#define N_SIGS ((int)(sizeof(SIGS) / sizeof(SIGS[0])))

/* ── Absturz-Melder ────────────────────────────────────────────────────── */

/* Was gerade laeuft. Ein Fuzzer, der abstuerzt und nicht sagt woran, kostet
 * pro Fund einen kompletten Instrumentierungs-Durchgang. Der Handler unten
 * schreibt diese Angabe auf stderr, bevor der Prozess faellt — damit steht
 * in JEDEM Absturz, auch in der CI, welche Eingabe und welches Plugin. */
/* MF-553: der Pfad traegt die Prozesskennung.
 *
 * Vorher stand hier ein fester relativer Name. Zwei Instanzen im selben
 * Arbeitsverzeichnis ueberschreiben einander damit die EINGABEdatei —
 * nicht die Ausgabe. Ein Lauf oeffnet dann eine Datei, die ein anderer
 * Lauf gerade hineingeschrieben hat, und der Bericht am Ende gehoert zu
 * keiner der beiden Eingaben.
 *
 * Aufgefallen ist es bei der Suche nach dem QRST-Absturz (MF-543): ein
 * paralleler Helfer-Prozess lief im selben Verzeichnis und kontaminierte
 * den Lauf. Der Fund war trotzdem echt — er liess sich in einem eigenen
 * Verzeichnis unabhaengig wiederholen — aber die erste Spur fuehrte
 * zwanzig Minuten in die falsche Richtung.
 *
 * `ctest -j8` startet Tests parallel im selben Arbeitsverzeichnis. Solange
 * nur dieser eine Test die Datei benutzt, geht es gut; der naechste Test,
 * der denselben Namen waehlt, bricht beide. Ein Fuzzer, dessen Eingabe
 * jemand anderes schreiben kann, misst nicht, was er zu messen glaubt. */
static char tmp_path_buf[64];
static const char *tmp_path = NULL;

static const char *fuzz_tmp_path(void)
{
    if (!tmp_path) {
        snprintf(tmp_path_buf, sizeof(tmp_path_buf),
                 "uft_fuzz_tmp_%ld.img", (long)getpid());
        tmp_path = tmp_path_buf;
    }
    return tmp_path;
}
static char g_where[256] = "(noch nichts)";
static const char *g_stage_plugin = "-";
static const char *g_stage = "-";

static void crash_handler(int sig)
{
    /* Nur async-signal-sicheres Schreiben: kein printf, kein malloc. */
    const char *msg = "\n*** ABSTURZ (Signal ";
    fputs(msg, stderr);
    fputc('0' + (sig / 10) % 10, stderr);
    fputc('0' + sig % 10, stderr);
    fputs(") bei: ", stderr);
    fputs(g_where, stderr);
    fputs("  |  Plugin: ", stderr);
    fputs(g_stage_plugin, stderr);
    fputs("  Stufe: ", stderr);
    fputs(g_stage, stderr);
    fputs("\n    Eingabe liegt in ", stderr);
    fputs(fuzz_tmp_path(), stderr);
    fputs("\n", stderr);
    fflush(stderr);
    _exit(139);
}

/* ── Der Produktionspfad ───────────────────────────────────────────────── */

static long n_open_ok, n_open_null, n_track_ok, n_track_null;

/** Legt den Blob als Datei ab. Muss VOR den Sonden laufen: sobald eine
 *  Sonde zustimmt, wird ihr `open` auf genau dieser Datei gerufen. */
static void write_tmp(size_t size)
{
    FILE *f = fopen(fuzz_tmp_path(), "wb");
    if (!f) {
        printf("  KANN TEMPDATEI NICHT SCHREIBEN: %s\n", fuzz_tmp_path());
        exit(2);
    }
    if (size && fwrite(blob, 1, size, f) != size) {
        fclose(f);
        printf("  TEMPDATEI UNVOLLSTAENDIG GESCHRIEBEN\n");
        exit(2);
    }
    fclose(f);
}

/** Schickt die abgelegte Datei durch uft_disk_open() und alles, was ein
 *  Betrachter danach tut — also durch die Rangfolge, mit einem Gewinner. */
static void run_open_path(size_t size)
{
    (void)size;
    uft_disk_t *disk = uft_disk_open(fuzz_tmp_path(), true);
    if (!disk) {
        n_open_null++;
        return;                 /* Muell abzulehnen ist richtig. */
    }
    n_open_ok++;

    uft_geometry_t geo;
    memset(&geo, 0, sizeof(geo));
    (void)uft_disk_get_geometry(disk, &geo);

    /* Koordinaten absichtlich auch ausserhalb jeder gemeldeten Geometrie:
     * ein Plugin darf nicht darauf bauen, dass der Aufrufer sich an die
     * Geometrie haelt, die es selbst gemeldet hat.
     *
     * Gelesen wird ueber das Plugin, NICHT ueber uft_track_read(): dieser
     * Prototyp steht zwar in uft_core.h, hat aber im ganzen Baum keine
     * Definition und keinen Aufrufer (gemessen, MF-512). Ihn hier zu
     * benutzen wuerde den Test an ein Versprechen haengen, das niemand
     * einloest — siehe KNOWN_ISSUES ARCH-27. */
    static const int CYL[] = { 0, 1, 39, 79, 82, -1, 1000 };
    static const int HEAD[] = { 0, 1, -1, 7 };

    /* disk->plugin, NICHT uft_get_format_plugin(disk->format). MF-445 hat
     * genau das behoben: 82 Plugins teilen sich UFT_FORMAT_DSK, ein
     * Nachschlagen ueber die Format-ID liefert also ein FREMDES Plugin, das
     * dann auf plugin_data losgeht, die ein anderes alloziert hat. Die
     * erste Fassung dieses Tests machte diesen Fehler und stuerzte
     * prompt ab — der Absturz war meiner, nicht der des Werkzeugs. */
    const uft_format_plugin_t *plug = disk->plugin;
    for (size_t c = 0; c < sizeof(CYL) / sizeof(CYL[0]); c++) {
        for (size_t h = 0; h < sizeof(HEAD) / sizeof(HEAD[0]); h++) {
            if (!plug || !plug->read_track) { n_track_null++; continue; }
            uft_track_t trk;
            memset(&trk, 0, sizeof(trk));
            if (plug->read_track(disk, CYL[c], HEAD[h], &trk) == UFT_OK)
                n_track_ok++;
            else
                n_track_null++;
            uft_track_cleanup(&trk);   /* MF-525 */
        }
    }

    uft_disk_close(disk);
}

/* ── Mutatoren auf echten Dateien ──────────────────────────────────────── */

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "."
#endif

/* Mit benannten Werkzeugen erzeugt (VICE, atrcopy, xdftool, Greaseweazle),
 * im Repo unter tests/corpus_free/. Fehlt eine, wird sie uebersprungen und
 * genannt — ein stilles Ueberspringen waere eine Abdeckung, die es nicht
 * gibt. */
static const char *const CORPUS[] = {
    "vice_c1541_35trk.d64", "vice_c1541_35trk.g64", "vice_c1541_70trk.d71",
    "vice_c1541_80trk.d81", "vice_c1541_2040.d67",  "vice_c1541_8050.d80",
    "vice_c1541_8250.d82",  "vice_c1541_1571.g71",  "atrcopy_dos2sd.atr",
    "atrcopy_dos2sd.xfd",   "xdftool_dd_ofs.adf",   "gw_amigados.hfe",
    NULL
};

/** Kopf unveraendert, ein Feld dahinter auf 0xFF — laesst Zaehler und
 *  Laengen auf ihr Maximum laufen, ohne die Erkennung zu verlieren. */
static size_t mut_header_max(const uint8_t *base, size_t len)
{
    memcpy(blob, base, len);
    for (size_t i = 8; i < 64 && i < len; i++) blob[i] = 0xFF;
    return len;
}

/** Dasselbe mit Nullen: Division durch null, Schleifen ohne Abbruch. */
static size_t mut_header_zero(const uint8_t *base, size_t len)
{
    memcpy(blob, base, len);
    for (size_t i = 8; i < 64 && i < len; i++) blob[i] = 0x00;
    return len;
}

/** Abgeschnitten. Das ist der haeufigste reale Schadensfall — und genau
 *  der, an dem das doppelte free in MF-513 haengt. */
static size_t mut_truncate_half(const uint8_t *base, size_t len)
{
    size_t n = len / 2;
    memcpy(blob, base, n);
    return n;
}

static size_t mut_truncate_header(const uint8_t *base, size_t len)
{
    size_t n = len < 300 ? len : 300;
    memcpy(blob, base, n);
    return n;
}

/** Einzelne gekippte Bits, gleichmaessig ueber die Datei verteilt. */
static size_t mut_bitflips(const uint8_t *base, size_t len)
{
    memcpy(blob, base, len);
    for (int k = 0; k < 64; k++) {
        size_t at = (size_t)(rng_next() % (len ? len : 1));
        blob[at] ^= (uint8_t)(1u << (rng_next() & 7));
    }
    return len;
}

/** Der Kopf bleibt, der Rumpf wird Rauschen: die Erkennung greift, der
 *  Parser laeuft auf Unsinn weiter. */
static size_t mut_body_noise(const uint8_t *base, size_t len)
{
    memcpy(blob, base, len);
    for (size_t i = 64; i < len; i++) blob[i] = (uint8_t)rng_next();
    return len;
}

/** Unveraendert. Gehoert dazu: eine gueltige Datei muss den ganzen Weg
 *  ueberstehen, sonst misst der Rest nichts. */
static size_t mut_identity(const uint8_t *base, size_t len)
{
    memcpy(blob, base, len);
    return len;
}

static const struct {
    size_t (*fn)(const uint8_t *, size_t);
    const char *label;
} MUTATORS[] = {
    { mut_identity,         "roh"     },
    { mut_header_max,       "kopf-FF" },
    { mut_header_zero,      "kopf-00" },
    { mut_truncate_half,    "halb"    },
    { mut_truncate_header,  "300B"    },
    { mut_bitflips,         "bits"    },
    { mut_body_noise,       "rumpf"   },
};
#define N_MUTATORS ((int)(sizeof(MUTATORS) / sizeof(MUTATORS[0])))

/* ── Abdeckung je Plugin ───────────────────────────────────────────────── */

#define MAX_PLUGINS 512

/* Fuer jedes registrierte Plugin: hat seine Sonde je zugestimmt, und ist
 * sein `open` je gelaufen?
 *
 * Ohne diese beiden Zahlen ist "0 Abstuerze" keine Aussage, sondern ein
 * Satz ohne Nenner. Der erste Lauf dieses Tests oeffnete 81 Dateien — aber
 * ueber die Rangfolge gewinnt immer nur EIN Plugin je Eingabe, und welche
 * 136 dabei nie an die Reihe kamen, stand nirgends. */
static unsigned char cov_probe_hit[MAX_PLUGINS];
static unsigned char cov_open_called[MAX_PLUGINS];
static unsigned char cov_open_ok[MAX_PLUGINS];

/** Ruft `open` EINES Plugins auf derselben Datei, die seine eigene Sonde
 *  gerade angenommen hat.
 *
 *  Das ist keine Umgehung der Rangfolge, sondern die Pruefung des
 *  Vertrags: wenn `probe` ja sagt, muss `open` mit dieser Datei
 *  zurechtkommen. Ueber `uft_disk_open()` gewinnt je Eingabe nur ein
 *  Plugin — die anderen 136 saehen ihr `open` sonst nie, egal wie lange
 *  der Fuzzer laeuft.
 *
 *  Der Handle wird genau so aufgebaut wie in uft_disk_open()
 *  (src/core/uft_core_stubs.c) und mit uft_disk_close() abgebaut, damit
 *  hier kein zweiter, abweichender Lebenszyklus entsteht. */
static void run_one_plugin_open(const uft_format_plugin_t *p, size_t idx,
                                const char *path)
{
    if (!p || !p->open) return;
    cov_open_called[idx] = 1;
    g_stage_plugin = p->name ? p->name : "?";
    g_stage = "open";

    uft_disk_t *disk = calloc(1, sizeof(uft_disk_t));
    if (!disk) return;
    strncpy(disk->path_buf, path, sizeof(disk->path_buf) - 1);
    disk->path_buf[sizeof(disk->path_buf) - 1] = '\0';
    disk->path = disk->path_buf;
    disk->format = p->format;
    disk->plugin = p;
    disk->read_only = true;

    if (p->open(disk, path, true) != UFT_OK) { free(disk); return; }
    disk->is_open = true;
    cov_open_ok[idx] = 1;

    uft_geometry_t geo;
    memset(&geo, 0, sizeof(geo));
    (void)uft_disk_get_geometry(disk, &geo);

    g_stage = "read_track";
    if (p->read_track) {
        static const int CYL[]  = { 0, 1, 39, 79, 1000, -1 };
        static const int HEAD[] = { 0, 1, -1 };
        for (size_t c = 0; c < sizeof(CYL) / sizeof(CYL[0]); c++)
            for (size_t h = 0; h < sizeof(HEAD) / sizeof(HEAD[0]); h++) {
                uft_track_t trk;
                memset(&trk, 0, sizeof(trk));
                /* MF-525: uft_track_add_sector() alloziert das
                 * sectors-Feld UND je Sektor die Daten. Wer eine
                 * Spur liest und nicht aufraeumt, leckt sie. Diese
                 * Schleife liest tausende — ohne cleanup war der
                 * groesste Posten des LeakSanitizer-Berichts mein
                 * eigener Test, und er verdeckte damit den echten
                 * Rueckstand. */
                if (p->read_track(disk, CYL[c], HEAD[h], &trk) == UFT_OK)
                    n_track_ok++;
                else
                    n_track_null++;
                uft_track_cleanup(&trk);
            }
    }
    g_stage = "close";
    uft_disk_close(disk);
}

/* ── Registry-getriebene probe-Abdeckung ───────────────────────────────── */

static long n_probe_calls, n_probe_hits, n_probe_bad_conf, n_probe_mutated;

static void run_all_probes_from_registry(size_t n_plugins, size_t size)
{
    static uint8_t shadow[MAX_BLOB];
    memcpy(shadow, blob, size);

    /* `size` selbst gehoert dazu. Die erste Fassung nannte nur feste Werte
     * und verfehlte damit genau den Fall, den uft_disk_open() erzeugt: dort
     * ist file_size die ECHTE Dateigroesse. Ein Fuzzer, der andere
     * Randbedingungen prueft als der Produktionspfad erzeugt, prueft den
     * Produktionspfad nicht. */
    const size_t FILE_SIZES[] = { 0, 1, 512, 174848, 1474560, size };

    for (size_t i = 0; i < n_plugins; i++) {
        const uft_format_plugin_t *p = uft_get_format_by_index(i);
        if (!p || !p->probe) continue;
        for (size_t k = 0; k < sizeof(FILE_SIZES) / sizeof(FILE_SIZES[0]); k++) {
            int confidence = -12345;
                    bool hit = p->probe(blob, size, FILE_SIZES[k], &confidence);
            n_probe_calls++;
            if (hit) {
                n_probe_hits++;
                if (i < MAX_PLUGINS) cov_probe_hit[i] = 1;
                /* uft_format_plugin.h dokumentiert Konfidenz als 0-100. */
                if (confidence < 0 || confidence > 100) {
                    printf("  KONFIDENZ AUSSERHALB 0-100: %s meldet %d\n",
                           p->name ? p->name : "(namenlos)", confidence);
                    n_probe_bad_conf++;
                }
                /* Die Sonde hat zugestimmt — also ist `open` an der Reihe.
                 * Nur bei der ECHTEN Dateigroesse: bei einer erfundenen
                 * haette die Sonde in der Wirklichkeit nie zugestimmt. */
                if (FILE_SIZES[k] == size && i < MAX_PLUGINS)
                    run_one_plugin_open(p, i, fuzz_tmp_path());
            }
        }
    }

    /* probe nimmt einen const-Zeiger und darf nichts veraendern
     * (DESIGN_PRINCIPLES: keine stille Veraenderung). Geprueft statt
     * geglaubt — aber EINMAL je Eingabe, nicht je Sonde.
     *
     * Die erste Fassung verglich nach jedem einzelnen probe-Aufruf. Bei
     * 137 Plugins x 6 Dateigroessen sind das 822 Vergleiche ueber bis zu
     * 3 MB — rund 2,5 GB je Eingabe. Damit war der Groessen-Durchlauf
     * (MF-518) rechnerisch unbezahlbar, und ein Fuzzer, der zu langsam
     * ist, um zu laufen, prueft nichts.
     *
     * Der billige Weg zuerst: ein Vergleich ueber alles. Nur wenn er
     * anschlaegt, kostet die Zuordnung einen zweiten, langsamen Durchlauf
     * — und das lohnt sich, weil dieser Fall bisher nie eingetreten ist. */
    if (size && memcmp(shadow, blob, size) != 0) {
        memcpy(blob, shadow, size);
        for (size_t i = 0; i < n_plugins; i++) {
            const uft_format_plugin_t *p = uft_get_format_by_index(i);
            if (!p || !p->probe) continue;
            int c = 0;
            (void)p->probe(blob, size, size, &c);
            if (memcmp(shadow, blob, size) != 0) {
                printf("  PUFFER VERAENDERT DURCH: %s\n",
                       p->name ? p->name : "(namenlos)");
                n_probe_mutated++;
                memcpy(blob, shadow, size);
            }
        }
    }
}

/** Eine Eingabe, vollstaendig: ablegen, alle Sonden fragen (und jedem
 *  Zustimmenden sein `open` geben), dann die Rangfolge gehen.
 *
 *  Die Reihenfolge ist wesentlich: die Datei muss VOR den Sonden liegen,
 *  weil eine zustimmende Sonde sofort ihr `open` auf genau dieser Datei
 *  bekommt. */
static void feed(size_t n_plugins, size_t size)
{
    write_tmp(size);
    /* g_where wurde vom Aufrufer gesetzt (Muster/Mutant + Groesse);
     * run_one_plugin_open haengt den Plugin-Namen an. */
    run_all_probes_from_registry(n_plugins, size);
    run_open_path(size);
}

/* ── Treiber ───────────────────────────────────────────────────────────── */

/** Wiedergabe: EINE Datei durch den Oeffnungspfad, mit Stufenanzeige.
 *
 *      ./test_disk_open_fuzz <datei>
 *
 * Ein Fuzzer ohne Wiedergabe zwingt dazu, den ganzen Lauf zu wiederholen,
 * um dieselbe Eingabe noch einmal zu sehen — und beim Eingrenzen ist genau
 * das die teuerste Schleife. Die Dateien unter tests/crashers/ sind damit
 * einzeln nachstellbar. */
static int replay(const char *path)
{
    printf("Wiedergabe: %s\n", path);

    printf("  [1] uft_probe_file_format ... ");
    const uft_format_plugin_t *w = uft_probe_file_format(path);
    printf("%s\n", w && w->name ? w->name : "(kein Treffer)");

    printf("  [2] uft_disk_open ... ");
    uft_disk_t *disk = uft_disk_open(path, true);
    if (!disk) { printf("abgelehnt\n"); return 0; }
    printf("geoeffnet, Plugin=%s\n",
           disk->plugin && disk->plugin->name ? disk->plugin->name : "?");

    uft_geometry_t geo;
    memset(&geo, 0, sizeof(geo));
    printf("  [3] uft_disk_get_geometry ... ");
    (void)uft_disk_get_geometry(disk, &geo);
    printf("%u Zylinder x %u Koepfe\n",
           (unsigned)geo.cylinders, (unsigned)geo.heads);

    static const int CYL[] = { 0, 1, 39, 79, 82, -1, 1000 };
    static const int HEAD[] = { 0, 1, -1, 7 };
    const uft_format_plugin_t *plug = disk->plugin;
    for (size_t c = 0; c < sizeof(CYL) / sizeof(CYL[0]); c++)
        for (size_t h = 0; h < sizeof(HEAD) / sizeof(HEAD[0]); h++) {
            if (!plug || !plug->read_track) continue;
            printf("  [4] read_track %d/%d ... ", CYL[c], HEAD[h]);
            uft_track_t trk;
            memset(&trk, 0, sizeof(trk));
            uft_error_t e = plug->read_track(disk, CYL[c], HEAD[h], &trk);
            printf("%d\n", (int)e);
            uft_track_cleanup(&trk);   /* MF-525 */
        }

    printf("  [5] uft_disk_close ... ");
    uft_disk_close(disk);
    printf("fertig\n");
    return 0;
}

int main(int argc, char **argv)
{
    /* Ungepuffert. Ein Fuzzer, der abstuerzt, muss noch sagen koennen wo —
     * mit Zeilenpufferung in eine Pipe geht genau die letzte Zeile
     * verloren, also die einzige, die zaehlt. */
    setvbuf(stdout, NULL, _IONBF, 0);
    signal(SIGSEGV, crash_handler);
    signal(SIGABRT, crash_handler);
    signal(SIGILL,  crash_handler);
    signal(SIGFPE,  crash_handler);

    uft_error_t reg = uft_register_all_formats();
    if (argc > 1) {
        if (reg != UFT_OK) { printf("Registrierung schlug fehl\n"); return 1; }
        return replay(argv[1]);
    }
    size_t n_plugins = 0;
    while (uft_get_format_by_index(n_plugins) != NULL)
        n_plugins++;

    printf("=== Missgebildete Abbilder durch den echten Oeffnungspfad "
           "(MF-512) ===\n");
    printf("uft_register_all_formats() -> %d, Registry fuehrt %zu Plugins\n",
           (int)reg, n_plugins);

    if (n_plugins == 0) {
        printf("FEHLER: die Registry ist leer. Ohne registrierte Plugins\n"
               "        prueft dieser Test nichts — genau die Lage, die\n"
               "        MF-447 behoben hat.\n");
        return 1;
    }

    static const size_t SIZES[] = { 0, 1, 2, 15, 16, 511, 512, 513,
                                    4096, 65536, 174848 };
    const size_t N_SIZES = sizeof(SIZES) / sizeof(SIZES[0]);

    const uint64_t SEED = 0xC0FFEE123456789ull;
    rng_seed(SEED);

    long n_inputs = 0;

    for (size_t s = 0; s < N_SIZES; s++) {
        size_t size = SIZES[s];
        if (size > MAX_BLOB) continue;

        /* Drei feste Muster ... */
        printf("  Groesse %7zu: nullen", size);
        snprintf(g_where, sizeof(g_where), "nullen/%zu", size);
        n_inputs++; feed(n_plugins, make_zeros(size));
        printf(" einsen");
        snprintf(g_where, sizeof(g_where), "einsen/%zu", size);
        n_inputs++; feed(n_plugins, make_ones(size));
        printf(" zufall");
        snprintf(g_where, sizeof(g_where), "zufall/%zu", size);
        n_inputs++; feed(n_plugins, make_random(size));

        /* ... und jede dokumentierte Signatur mit Muell dahinter. */
        for (int g = 0; g < N_SIGS; g++) {
            printf(" %s", SIGS[g].label);
            snprintf(g_where, sizeof(g_where), "sig %s/%zu", SIGS[g].label, size);
            n_inputs++;
            feed(n_plugins,
                 make_signature_then_garbage(SIGS[g].sig, SIGS[g].len, size));
        }
        printf("\n");
    }

    /* ── Groessen-Tore ────────────────────────────────────────────────
     *
     * Die meisten Sonden dieses Baums entscheiden ueber die DATEIGROESSE:
     * `if (file_size != 819200) return false;`. Zufallsbytes einer
     * beliebigen Laenge kommen an so einem Tor nie vorbei — deshalb sah
     * der erste Lauf 93 der 137 Plugins nie.
     *
     * Diese Liste ist nicht geraten, sondern aus den Sonden GELESEN: alle
     * Vergleiche gegen `file_size`/`size` in den C-Dateien unter
     * src/formats, sofern zwischen 1 KB und 20 MB. 46 Werte aus 30 Sonden;
     * 819200 verlangen allein sechs. Erzeugt mit
     * scripts/audit_probe_sizes.py, das die Liste jederzeit neu misst.
     *
     * MF-539: sieben Werte kamen dazu (jetzt 53). Sie standen in der
     * Geometrie-Tabelle von src/formats/dsk_generic/uft_dsk_generic.c bzw.
     * als `#define` in src/formats/d13/uft_d13.c und wurden deshalb vom
     * frueheren Lauf des Skripts nicht als `file_size`-Vergleich erkannt —
     * dsk_gen_probe_idx() vergleicht gegen `g->expected_size` aus der
     * Tabelle, nicht gegen eine Zahl im Quelltext. Jeder Wert unten ist an
     * der genannten Zeile nachgelesen, nicht abgeleitet. */
    static const size_t GATE_SIZES[] = {
        2277, 6656, 89600, 91648, 92160, 102400,
        116480,                    /* D13     uft_d13.c:16              */
        133120, 143360,
        152320,                    /* DSK_VIC uft_dsk_generic.c:76      */
        163840, 174848, 175531,
        177408,                    /* DSK_CRO uft_dsk_generic.c:69      */
        179200,
        184320, 196608, 197376, 200960, 201745, 204800,
        205312, 206114, 232960, 256256, 266240,
        286720,                    /* DSK_OLI uft_dsk_generic.c:68      */
        315392,
        327680, 368640, 399363, 512512, 533248,
        573440,                    /* DSK_SMC uft_dsk_generic.c:44      */
        626688,
        630784, 634880, 655360,
        696320,                    /* DSK_ORC uft_dsk_generic.c:78      */
        737280, 819200, 822400,
        871424, 901120, 1025024, 1066496, 1228800, 1253376,
        1261568,                   /* DSK_RLD uft_dsk_generic.c:55      */
        1474560, 1638400, 1802240, 2949120,
    };
    printf("\nGroessen-Tore (%d aus den Sonden gelesen):\n",
           (int)(sizeof(GATE_SIZES) / sizeof(GATE_SIZES[0])));
    for (size_t gi = 0; gi < sizeof(GATE_SIZES) / sizeof(GATE_SIZES[0]); gi++) {
        size_t size = GATE_SIZES[gi];
        if (size > MAX_BLOB) {
            printf("  %zu passt nicht in MAX_BLOB — uebersprungen\n", size);
            continue;
        }
        snprintf(g_where, sizeof(g_where), "tor %zu/nullen", size);
        n_inputs++; feed(n_plugins, make_zeros(size));
        snprintf(g_where, sizeof(g_where), "tor %zu/einsen", size);
        n_inputs++; feed(n_plugins, make_ones(size));
        snprintf(g_where, sizeof(g_where), "tor %zu/zufall", size);
        n_inputs++; feed(n_plugins, make_random(size));
        printf(".");
    }
    printf(" fertig\n");

    /* ── Zweiter Teil: echte Dateien, gezielt beschaedigt ──────────────
     *
     * Zufallsbytes kommen selten an einer Sonde vorbei; wo sie es tun,
     * scheitert der Parser meist in der ersten Zeile. Eine ECHTE Datei mit
     * einem verdrehten Feld kommt dagegen tief hinein — dorthin, wo
     * gerechnet und alloziert wird. Genau dort lagen die drei Fehler aus
     * MF-513.
     *
     * Das Korpus unter tests/corpus_free/ ist mit benannten Werkzeugen
     * erzeugt (VICE, atrcopy, xdftool, Greaseweazle) und im Repo. */
    printf("\nKorpus-Mutation:\n");
    long n_corpus = 0;
    int n_truncated = 0;
    for (int ci = 0; CORPUS[ci]; ci++) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", UFT_CORPUS_DIR, CORPUS[ci]);
        FILE *cf = fopen(path, "rb");
        if (!cf) { printf("  %-24s (fehlt)\n", CORPUS[ci]); continue; }
        size_t base_len = fread(blob, 1, MAX_BLOB, cf);
        /* Passte die Datei ueberhaupt hinein? Eine stille Kuerzung wuerde
         * als Abdeckung durchgehen, die es nicht gibt. */
        const bool truncated = (fgetc(cf) != EOF);
        fclose(cf);
        if (base_len == 0) { printf("  %-24s (leer)\n", CORPUS[ci]); continue; }
        if (truncated) {
            printf("  %-24s ABGESCHNITTEN auf %zu B — MAX_BLOB zu klein, "
                   "diese Datei zaehlt NICHT als Abdeckung\n",
                   CORPUS[ci], base_len);
            n_truncated++;
        }

        static uint8_t base[MAX_BLOB];
        memcpy(base, blob, base_len);
        printf("  %-24s %7zu Byte:", CORPUS[ci], base_len);

        for (int m = 0; m < N_MUTATORS; m++) {
            size_t len = MUTATORS[m].fn(base, base_len);
            printf(" %s", MUTATORS[m].label);
            snprintf(g_where, sizeof(g_where), "%s + %s",
                     CORPUS[ci], MUTATORS[m].label);
            n_inputs++; n_corpus++;
            feed(n_plugins, len);
        }
        printf("\n");
    }
    printf("  (%ld Mutanten aus %d Korpusdateien%s)\n", n_corpus,
           (int)(sizeof(CORPUS) / sizeof(CORPUS[0]) - 1),
           n_truncated ? ", DAVON ABGESCHNITTEN — siehe oben" : "");

    remove(fuzz_tmp_path());

    printf("\nEingaben               : %ld\n", n_inputs);
    printf("probe-Aufrufe          : %ld  (%zu Plugins x %ld Eingaben x 6 "
           "Dateigroessen)\n", n_probe_calls, n_plugins, n_inputs);
    printf("  davon Treffer        : %ld\n", n_probe_hits);
    printf("uft_disk_open()        : %ld geoeffnet, %ld abgelehnt\n",
           n_open_ok, n_open_null);
    printf("uft_track_read()       : %ld geliefert, %ld abgelehnt\n",
           n_track_ok, n_track_null);

    /* ── Der Nenner ────────────────────────────────────────────────────
     *
     * "0 Abstuerze" ohne diese Zahlen ist ein Satz ohne Nenner. Wer nie
     * gerufen wurde, ist nicht geprueft — er ist ungeprueft, und das
     * gehoert genauso hingeschrieben wie das Ergebnis. */
    size_t n_hit = 0, n_called = 0, n_ok = 0;
    for (size_t i = 0; i < n_plugins && i < MAX_PLUGINS; i++) {
        n_hit    += cov_probe_hit[i]   ? 1 : 0;
        n_called += cov_open_called[i] ? 1 : 0;
        n_ok     += cov_open_ok[i]     ? 1 : 0;
    }
    printf("\nAbdeckung ueber %zu registrierte Plugins:\n", n_plugins);
    printf("  Sonde hat je zugestimmt   : %3zu  (%zu nie)\n",
           n_hit, n_plugins - n_hit);
    printf("  open() wurde je gerufen   : %3zu  (%zu nie)\n",
           n_called, n_plugins - n_called);
    printf("  open() war je erfolgreich : %3zu\n", n_ok);

    printf("\n  NIE erreicht (Sonde stimmte keiner Eingabe zu) — ungeprueft:\n    ");
    size_t col = 0;
    for (size_t i = 0; i < n_plugins && i < MAX_PLUGINS; i++) {
        if (cov_probe_hit[i]) continue;
        const uft_format_plugin_t *p = uft_get_format_by_index(i);
        printf("%s ", p && p->name ? p->name : "?");
        if (++col % 12 == 0) printf("\n    ");
    }
    printf("\n");

    long fail = n_probe_bad_conf + n_probe_mutated;
    printf("\nVertragsverletzungen   : %ld"
           "  (Konfidenz ausserhalb 0-100: %ld, Puffer veraendert: %ld)\n",
           fail, n_probe_bad_conf, n_probe_mutated);

    if (fail) {
        printf("\nFEHLGESCHLAGEN. Seed zum Nachstellen: 0x%llX\n",
               (unsigned long long)SEED);
        return 1;
    }

    /* Kein Absturz ist hier das eigentliche Ergebnis. Dass er ausbleibt,
     * ist unter ASan/UBSan (CI) eine staerkere Aussage als hier. */
    printf("OK — kein Absturz, keine Vertragsverletzung.\n");
    return 0;
}
