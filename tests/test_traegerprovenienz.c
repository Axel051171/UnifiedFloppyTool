/**
 * @file test_traegerprovenienz.c
 * @brief P3-454 — Herkunft des DATENTRAEGERS aus dem Bootstrap-Code.
 *
 * WARUM DIESER TEST VOR DEM CODE STEHT
 * ------------------------------------
 * Gemessen (MF-1186): `uft_provenance` (6 `.c`) und
 * `uft_fundus_provenance` (3 `.c`) fuehren die Herkunft von KORPUSDATEIEN
 * — Werkzeug, Fassung, Weg, Lizenz. Fuer den DATENTRAEGER fuehrt der Baum
 * keine. Der Pruefwert liegt dabei schon vor: `oem_name` wird an neun
 * Stellen gelesen. Der SCHLUESSEL fehlt: `bootstrap` hat 0 `.c`-Treffer,
 * `crc32_boot` 0.
 *
 * DIE QUELLE
 * ----------
 * `Modules/BootstrapDB.vb` aus DiskImageTool (GPL-3.0, im Baum unter
 * `tools/uft-scout/work/DiskImageTool/`, 213 Zeilen, MF-1187 gelesen):
 *
 *   Private _OEMNameDictionary As Dictionary(Of UInteger, BootstrapLookup)
 *   Dim Checksum = CRC32.ComputeChecksum(BootstrapCode)
 *   If _OEMNameDictionary.ContainsKey(Checksum) Then …
 *
 * CRC32 ueber den Bootstrap-Code ist der SCHLUESSEL, der OEM-Name der
 * PRUEFWERT. Die Win9x-Kennung „IHC" an den Positionen 5..7 steht in
 * `BootSector.vb:306`.
 *
 * WAS HIER NICHT GEPRUEFT WIRD, UND WARUM
 * ---------------------------------------
 * Der BESTAND. Die Originaldatenbank liegt vor — `Resources/bootstrap.xml`
 * im gitignorierten Fremdklon, gemessen 379 `<bootstrap>`-Schluessel und
 * 490 `<oemname>`-Namen, 223 davon `verified="true"` —, ist aber ohne
 * Eigentuemer-Entscheidung nicht uebernehmbar (S3: GPL-3.0 nach MF-698
 * plus EU-Datenbankherstellerrecht, P3-454; der Grund steht im Header
 * von `uft_bootstrap.h`).
 *
 * Der Mechanismus ist ohne sie vollstaendig pruefbar, und der leere
 * Bestand muss sagen, was er ist: **„dieser Bestand kennt ihn nicht",
 * NICHT „unbekanntes Werkzeug"** — dieselbe Regel wie MF-980 („das
 * Format sagt 0xE5" und „hier wurde 0xE5 gelesen" sind zwei Aussagen)
 * und die Spaltenregel D6.
 *
 * DER PRODUKTIVAUFRUFER IST MITGEPRUEFT (D2)
 * ------------------------------------------
 * Ein Erkenner ohne Aufrufer waere der vierte ungerufene dieses Baums
 * (P3-204/MF-767). Die beiden letzten Zusagen rufen deshalb
 * `fat_analyze_boot_sector()` — den EINEN Ort im Baum, an dem der
 * Bootsektor ohnehin auf dem Tisch liegt — und werden rot, wenn der
 * Aufruf dort verschwindet.
 */

#include "uft/forensic/uft_bootstrap.h"
#include "uft/formats/uft_air_crc32.h"
/* Der Produktivaufrufer (D2) — siehe letzten Abschnitt oben. */
#include "uft/formats/fat/uft_fat_bootsector.h"

#include <stdio.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-46s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                   _fail++; return; } } while (0)

static uint8_t g_sek[512];

/* Ein PC-Bootsektor. Die Feldlagen stehen AUSGESCHRIEBEN und nicht ueber
 * `fat_create_boot_sector()` — sonst befragt die Pruefdatei dieselbe
 * Quelle wie der Pruefling (MF-1000).
 *
 * `code_bytes` ist die Zahl der Nicht-Null-Bytes im Bootstrap-Bereich.
 * Die Polsterung dahinter ist immer 0, weil genau das geprueft wird: sie
 * darf die CRC NICHT aendern. */
static void sektor_bauen(unsigned code_bytes, const char *oem,
                         int mit_jmp, int mit_kennung)
{
    memset(g_sek, 0, sizeof(g_sek));
    if (mit_jmp) {                       /* EB 3C 90 -> Code ab 2 + 0x3C */
        g_sek[0] = 0xEBu; g_sek[1] = 0x3Cu; g_sek[2] = 0x90u;
    }
    memcpy(g_sek + 0x03, oem, 8);        /* OEM-Name, 8 Byte, 0x03..0x0A */
    /* BPB-Rumpf, damit der Sektor plausibel ist — fuer diesen Test nicht
     * ausgewertet, aber ein Bootsektor ohne BPB waere unrealistisch. */
    g_sek[0x0B] = 0x00u; g_sek[0x0C] = 0x02u;      /* 512 Byte/Sektor   */
    g_sek[0x15] = 0xF0u;                            /* Medienbyte       */
    /* Der Code beginnt bei 2 + 0x3C = 0x3E und traegt `code_bytes`
     * unterscheidbare Nicht-Null-Bytes; der Rest bleibt 0. */
    for (unsigned i = 0; i < code_bytes; i++)
        g_sek[0x3Eu + i] = (uint8_t)(0x40u + (i & 0x3Fu));
    if (mit_kennung) { g_sek[510] = 0x55u; g_sek[511] = 0xAAu; }
}

/* ── ROTBEWEIS: der Baum bildet heute keinen Bootstrap-Schluessel ────── */

TEST(ein_bootsektor_bekommt_einen_schluessel)
{
    uft_bs_traeger_id_t id;
    sektor_bauen(64u, "MSDOS5.0", 1, 1);

    ASSERT(uft_bs_identifiziere(g_sek, sizeof(g_sek), &id));
    /* Der Code beginnt bei 2 + 0x3C = 0x3E und ist 64 Byte lang. */
    ASSERT(id.code_offset == 0x3Eu);
    ASSERT(id.code_len    == 64u);
    ASSERT(id.crc32       != 0u);
    ASSERT(strcmp(id.oem, "MSDOS5.0") == 0);
    /* Der Bestand ist LEER, und das ist der dritte Zustand: gemessen,
     * aber nicht zugeordnet. NICHT „unbekannt". */
    ASSERT(id.lage     == UFT_BS_TRAEGER_GEMESSEN);
    ASSERT(id.werkzeug == NULL);
}

TEST(der_schluessel_ist_das_crc32_des_baums)
{
    /* Anti-Tautologie und die wichtigste Zusage dieses Tests: der Baum
     * hat das Polynom 0xEDB88320 in **22** Dateien und **vier**
     * CRC-Header (gemessen MF-1189). Ein achtzehntes CRC-32 waere
     * MF-1177 in Reinform. Hier wird nachgerechnet, dass der Schluessel
     * GENAU das ist, was `air_crc32_buffer()` liefert — sonst trifft er
     * die Datenbank von DiskImageTool nie. */
    uft_bs_traeger_id_t id;
    uint32_t erwartet;
    sektor_bauen(64u, "MSDOS5.0", 1, 1);
    ASSERT(uft_bs_identifiziere(g_sek, sizeof(g_sek), &id));

    erwartet = air_crc32_buffer(g_sek, id.code_offset, id.code_len);
    ASSERT(id.crc32 == erwartet);
    ASSERT(erwartet != 0u);
}

TEST(die_polsterung_aendert_den_schluessel_nicht)
{
    /* Die Entwurfsentscheidung mit ihrem eigenen Beweis: nachlaufende
     * Nullen gehoeren nicht zum Code, sonst haengt der Schluessel daran,
     * wie viel Fuellung der Formatierer geschrieben hat. Zwei Sektoren,
     * gleicher Code, unterschiedlich viel Polsterung -> GLEICHE CRC. */
    uft_bs_traeger_id_t a, b;

    sektor_bauen(32u, "MSDOS5.0", 1, 1);
    ASSERT(uft_bs_identifiziere(g_sek, sizeof(g_sek), &a));

    /* Dieselben 32 Byte, aber die Kennung 0x55AA fehlt — damit reicht der
     * Bereich bis 512 statt 510, also 2 Nullbyte mehr Polsterung. */
    sektor_bauen(32u, "MSDOS5.0", 1, 0);
    ASSERT(uft_bs_identifiziere(g_sek, sizeof(g_sek), &b));

    ASSERT(a.code_len == 32u);
    ASSERT(b.code_len == 32u);
    ASSERT(a.crc32 == b.crc32);
}

TEST(ohne_sprungbefehl_gibt_es_keinen_schluessel)
{
    /* Anti-Tautologie: ohne diese Zusage waere „bildet immer eine CRC"
     * genauso gruen. Ein Sektor ohne EB/E9 am Anfang ist kein
     * PC-Bootsektor — die Lage ist UNBEKANNT und die CRC bleibt 0. */
    uft_bs_traeger_id_t id;
    sektor_bauen(64u, "MSDOS5.0", 0, 1);   /* kein JMP */

    ASSERT(!uft_bs_identifiziere(g_sek, sizeof(g_sek), &id));
    ASSERT(id.lage     == UFT_BS_TRAEGER_UNBEKANNT);
    ASSERT(id.crc32    == 0u);
    ASSERT(id.code_len == 0u);
    ASSERT(id.werkzeug == NULL);

    /* Und ein zu kurzer Puffer ebenso. */
    ASSERT(!uft_bs_identifiziere(g_sek, 511u, &id));
    ASSERT(id.lage == UFT_BS_TRAEGER_UNBEKANNT);
}

TEST(ein_leerer_bootstrap_ist_kein_schluessel)
{
    /* Ein Sektor mit JMP, aber ohne ein einziges Nicht-Null-Byte im
     * Codebereich. Nach dem Abschneiden der Nullen bleibt NICHTS — und
     * eine CRC ueber nichts waere ein Schluessel ohne Gegenstand. */
    uft_bs_traeger_id_t id;
    sektor_bauen(0u, "MSDOS5.0", 1, 1);

    ASSERT(!uft_bs_identifiziere(g_sek, sizeof(g_sek), &id));
    ASSERT(id.lage  == UFT_BS_TRAEGER_UNBEKANNT);
    ASSERT(id.crc32 == 0u);
}

TEST(die_win9x_kennung_wird_in_beide_richtungen_gelesen)
{
    /* Quelle: `BootSector.vb:306` — „IHC" an den Positionen 5..7. Beide
     * Richtungen, sonst prueft die Zusage nur, dass die Funktion immer
     * dasselbe sagt. */
    uft_bs_traeger_id_t id;

    sektor_bauen(64u, "MSWIN4.1", 1, 1);   /* Positionen 5..7: N 4 .     */
    ASSERT(uft_bs_identifiziere(g_sek, sizeof(g_sek), &id));
    ASSERT(!id.oem_ist_win9x);

    sektor_bauen(64u, "MSWININC", 1, 1);   /* 5..7: I N C -> nicht IHC   */
    ASSERT(uft_bs_identifiziere(g_sek, sizeof(g_sek), &id));
    ASSERT(!id.oem_ist_win9x);

    sektor_bauen(64u, "12345IHC", 1, 1);   /* 5..7: I H C                */
    ASSERT(uft_bs_identifiziere(g_sek, sizeof(g_sek), &id));
    ASSERT(id.oem_ist_win9x);
}

TEST(der_dritte_zustand_ist_von_unbekannt_unterscheidbar)
{
    /* Die eigentliche Aussage des Postens: „dieser Bestand kennt ihn
     * nicht" ist NICHT „unbekanntes Werkzeug". */
    uft_bs_traeger_id_t gemessen, unbekannt;

    sektor_bauen(64u, "MSDOS5.0", 1, 1);
    ASSERT(uft_bs_identifiziere(g_sek, sizeof(g_sek), &gemessen));
    sektor_bauen(64u, "MSDOS5.0", 0, 1);
    ASSERT(!uft_bs_identifiziere(g_sek, sizeof(g_sek), &unbekannt));

    ASSERT(gemessen.lage  == UFT_BS_TRAEGER_GEMESSEN);
    ASSERT(unbekannt.lage == UFT_BS_TRAEGER_UNBEKANNT);
    ASSERT(gemessen.lage != unbekannt.lage);

    /* UFT_BS_TRAEGER_ZUGEORDNET ist heute NICHT erreichbar, weil der
     * Bestand leer ist — und das gehoert gesagt statt verschwiegen
     * (P3-454: die 379 Schluessel liegen vor, sind aber ohne
     * Entscheidung nicht uebernehmbar). Die Zusage hier ist deshalb,
     * dass der Bestand LEER ist, nicht dass er trifft. */
    ASSERT(uft_bs_bestand_groesse() == 0u);
}

/* ── D2: der Produktivaufrufer ────────────────────────────────────────── */

TEST(der_produktivpfad_bildet_den_schluessel_selbst)
{
    /* **Diese Zusage wird ROT, wenn der Aufruf von
     * `uft_bs_identifiziere()` in `fat_analyze_boot_sector()`
     * verschwindet** — das ist D2, und sie ist der Grund, warum der
     * Erkenner ueberhaupt liegen darf.
     *
     * Verglichen wird gegen den Mechanismus selbst, nicht gegen eine
     * hier nachgerechnete Zahl: damit prueft die Zusage die
     * VERDRAHTUNG, waehrend `der_schluessel_ist_das_crc32_des_baums`
     * die Rechnung prueft. Zwei Fragen, zwei Zusagen (MF-1177). */
    fat_analysis_result_t fa;
    uft_bs_traeger_id_t   id;

    sektor_bauen(64u, "MSDOS5.0", 1, 1);
    ASSERT(uft_bs_identifiziere(g_sek, sizeof(g_sek), &id));
    ASSERT(fat_analyze_boot_sector(g_sek, sizeof(g_sek), &fa) == FAT_OK);

    /* Anti-Tautologie zuerst: ohne diese Zeile waere `0 == 0` gruen,
     * und genau so sieht ein fehlender Aufruf aus. */
    ASSERT(fa.bootstrap_crc32 != 0u);
    ASSERT(fa.bootstrap_crc32  == id.crc32);
    ASSERT(fa.bootstrap_offset == 0x3Eu);
    ASSERT(fa.bootstrap_len    == 64u);
    ASSERT(fa.bootstrap_lage   == (int)UFT_BS_TRAEGER_GEMESSEN);

    /* Der PRUEFWERT war vorher schon da (`oem_name` wird an neun Stellen
     * gelesen, MF-1186). Neu ist der SCHLUESSEL — beide zusammen sind
     * die Aussage, einer allein nicht. */
    ASSERT(strcmp(fa.oem_name, "MSDOS5.0") == 0);
}

TEST(der_produktivpfad_erfindet_keinen_schluessel)
{
    /* Die Gegenrichtung, ohne die „setzt immer eine CRC" genauso gruen
     * waere. Und zugleich die Entwurfszusage des Aufrufs: eine Absage
     * der Traegerherkunft darf die BPB-Auswertung NICHT beruehren — ein
     * Abbild ohne Sprungbefehl ist eine gueltige FAT-Diskette. */
    fat_analysis_result_t mit, ohne;

    sektor_bauen(64u, "MSDOS5.0", 1, 1);
    ASSERT(fat_analyze_boot_sector(g_sek, sizeof(g_sek), &mit) == FAT_OK);

    sektor_bauen(64u, "MSDOS5.0", 0, 1);   /* kein JMP -> kein Schluessel */
    ASSERT(fat_analyze_boot_sector(g_sek, sizeof(g_sek), &ohne) == FAT_OK);

    ASSERT(ohne.bootstrap_lage   == (int)UFT_BS_TRAEGER_UNBEKANNT);
    ASSERT(ohne.bootstrap_crc32  == 0u);
    ASSERT(ohne.bootstrap_len    == 0u);
    ASSERT(ohne.bootstrap_offset == 0u);
    /* Unterscheidbar von der Aussage daneben (D6). */
    ASSERT(mit.bootstrap_lage != ohne.bootstrap_lage);

    /* Der BPB ist in BEIDEN Faellen gelesen: 512 Byte/Sektor stehen bei
     * 0x0B/0x0C, und `sektor_bauen()` schreibt sie immer. */
    ASSERT(mit.bytes_per_sector  == 512u);
    ASSERT(ohne.bytes_per_sector == 512u);
    ASSERT(strcmp(ohne.oem_name, "MSDOS5.0") == 0);
}

int main(void)
{
    printf("=== P3-454: Herkunft des Traegers aus dem Bootstrap-Code ===\n");
    RUN(ein_bootsektor_bekommt_einen_schluessel);
    RUN(der_schluessel_ist_das_crc32_des_baums);
    RUN(die_polsterung_aendert_den_schluessel_nicht);
    printf("--- Gegenproben: wann es KEINEN Schluessel gibt ---\n");
    RUN(ohne_sprungbefehl_gibt_es_keinen_schluessel);
    RUN(ein_leerer_bootstrap_ist_kein_schluessel);
    printf("--- Kennung und der dritte Zustand ---\n");
    RUN(die_win9x_kennung_wird_in_beide_richtungen_gelesen);
    RUN(der_dritte_zustand_ist_von_unbekannt_unterscheidbar);
    printf("--- D2: der Produktivaufrufer ---\n");
    RUN(der_produktivpfad_bildet_den_schluessel_selbst);
    RUN(der_produktivpfad_erfindet_keinen_schluessel);
    printf("\nErgebnis: %d bestanden, %d gefallen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
