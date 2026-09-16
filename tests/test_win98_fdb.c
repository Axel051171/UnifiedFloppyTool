/**
 * @file test_win98_fdb.c
 * @brief MSWIN4.1-Bootrecord: die Doktrin gegen die Leiter der
 *        Zulieferung, und der Fingerabdruck gegen die Lokalisierung
 *        (MF-1181).
 *
 * @section QUELLE
 *
 * Daniel B. Sedory, „MSWIN4.1 (Windows 98) Floppy Disk Boot Record",
 * https://daniel.sedory.com/asm/mbr/WIN98FDB.htm — nur GELESEN
 * (Kanal *Spec*). Uebernommen sind Bytelagen und BPB-Werte; Text und
 * Bootcode nicht. Der Pruefsektor unten ist SYNTHETISCH: er traegt die
 * dokumentierten Felder und einen selbst gewaehlten Fuellcode, keinen
 * Byte fremden Bootcodes.
 *
 * @section WAS_GEPRUEFT_WIRD
 *
 * 1. Die Zulieferung vergab ihre Konfidenz selbst — eine Leiter aus
 *    OEM +30, Sprung +10, Signatur +10, IO.SYS +10, MSDOS.SYS +10,
 *    Codehash +30, Schwelle 30. Damit erreichte `Sprung + Signatur +
 *    IO.SYS + MSDOS.SYS` bereits 40 und die Sonde stimmte ZU — an
 *    einem Sektor ohne jede MSWIN4.1-Kennung. Das ist der Rotbeweis
 *    unten; nach der Doktrin (MF-1153) ist die Obergrenze ohne Kennung
 *    45 und die Sonde sagt ab.
 *
 * 2. Die Zulieferung bildete ihren Fingerabdruck ueber 0x03E..0x1FD.
 *    Die Quelle sagt aber: „Most of the code is between offsets 3Eh
 *    through 17Eh, but there's a subroutine at offsets 1F1h through
 *    1FBh", und dazwischen liegen „three error messages" — die sind
 *    LOKALISIERT. Ein Abdruck ueber die Texte kann bei einer deutschen
 *    Startdiskette nie treffen. Der Test kippt ein Byte im Textbereich
 *    und zeigt, dass der Codeabdruck unveraendert bleibt; und kippt
 *    eines im Codebereich und zeigt, dass er sich aendert.
 *
 * 3. Kein zweiter BPB: die Werte im Bericht kommen aus
 *    `fat_analysis_result_t`, und eine der Zahlen ist gegen die QUELLE
 *    gehalten statt gegen sich selbst — die Seite rechnet 2847 nutzbare
 *    Sektoren vor, und der Analysator des Baums muss dieselbe Zahl
 *    liefern. Zwei Haende, eine Zahl.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "uft/formats/fat/uft_win98_fdb.h"
#include "uft/uft_format_plugin.h"

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-44s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)
#define CHECK(c, msg) do { if (!(c)) { \
                        printf("FAIL @ %d: %s\n", __LINE__, (msg)); \
                        _fail++; return; } } while (0)

#define SEK 512u

/* Die BPB-Werte der Quelle (Hexdump Z. 0000-0030 und die BPB-Tafel):
 *   0x0B 00 02  512 Byte/Sektor      0x16 09 00  9 Sektoren/FAT
 *   0x0D 01     1 Sektor/Cluster     0x18 12 00  18 Sektoren/Spur
 *   0x0E 01 00  1 reserviert         0x1A 02 00  2 Seiten
 *   0x10 02     2 FATs               0x26 29     Extended-BPB
 *   0x11 E0 00  224 Wurzeleintraege  0x2B "BOOTDISK   "
 *   0x13 40 0B  2880 Sektoren        0x36 "FAT12   "
 *   0x15 F0     Medienbyte
 * 2880 x 512 = 1 474 560 Byte, und die Seite nennt genau diese Zahl. */
static void baue_mswin41(uint8_t *s, bool mit_oem)
{
    memset(s, 0, SEK);
    s[0] = 0xEBu; s[1] = 0x3Cu; s[2] = 0x90u;          /* Sprung + NOP */
    if (mit_oem) memcpy(s + 0x03, "MSWIN4.1", 8u);
    else         memcpy(s + 0x03, "MSDOS5.0", 8u);      /* andere Hand */

    s[0x0B] = 0x00u; s[0x0C] = 0x02u;                  /* 512 */
    s[0x0D] = 0x01u;                                   /* 1 */
    s[0x0E] = 0x01u; s[0x0F] = 0x00u;                  /* 1 */
    s[0x10] = 0x02u;                                   /* 2 FATs */
    s[0x11] = 0xE0u; s[0x12] = 0x00u;                  /* 224 */
    s[0x13] = 0x40u; s[0x14] = 0x0Bu;                  /* 2880 */
    s[0x15] = 0xF0u;                                   /* Medienbyte */
    s[0x16] = 0x09u; s[0x17] = 0x00u;                  /* 9 */
    s[0x18] = 0x12u; s[0x19] = 0x00u;                  /* 18 */
    s[0x1A] = 0x02u; s[0x1B] = 0x00u;                  /* 2 */
    s[0x24] = 0x00u; s[0x25] = 0x00u;
    s[0x26] = 0x29u;                                   /* Extended-BPB */
    s[0x27] = 0x21u; s[0x28] = 0x6Cu; s[0x29] = 0x15u; s[0x2A] = 0x27u;
    memcpy(s + 0x2B, "BOOTDISK   ", 11u);
    memcpy(s + 0x36, "FAT12   ", 8u);

    /* Fuellcode — SELBST gewaehlt, kein fremder Bootcode. Was dort
     * steht, ist fuer jede Zusage unten gleichgueltig, solange es nicht
     * null ist (sonst wuerden die beiden Abdruecke trivial gleich). */
    for (unsigned i = UFT_WIN98_FDB_CODE_BEGIN; i < UFT_WIN98_FDB_CODE_END; ++i)
        s[i] = (uint8_t)(0x90u + (i & 0x0Fu));
    for (unsigned i = UFT_WIN98_FDB_SUBROUTINE_BEGIN;
         i < UFT_WIN98_FDB_SUBROUTINE_END; ++i)
        s[i] = (uint8_t)(0xE0u + (i & 0x0Fu));

    /* Der Textbereich — DEUTSCH, damit der Test selbst vorfuehrt, dass
     * die Meldungen lokalisiert sind. */
    memcpy(s + UFT_WIN98_FDB_MSG_BEGIN, "\r\nKeine Systemdiskette\xFF", 23u);

    memcpy(s + UFT_WIN98_FDB_IO_SYS_OFFSET,    "IO      SYS", 11u);
    memcpy(s + UFT_WIN98_FDB_MSDOS_SYS_OFFSET, "MSDOS   SYS", 11u);

    s[0x1FE] = 0x55u; s[0x1FF] = 0xAAu;                /* Signatur */
}

/* Die Leiter der Zulieferung, woertlich nachgebaut — NUR fuer den
 * Rotbeweis. Sie steht hier und nicht im Produktivcode, weil genau das
 * der Befund ist: sie darf nicht in den Baum. */
static unsigned leiter_der_zulieferung(const uint8_t *d)
{
    unsigned score = 0u;
    if (memcmp(d + 3u, "MSWIN4.1", 8u) == 0)             score += 30u;
    if (d[0] == 0xEBu && d[1] == 0x3Cu && d[2] == 0x90u) score += 10u;
    if (d[510] == 0x55u && d[511] == 0xAAu)              score += 10u;
    if (memcmp(d + 0x1D8u, "IO      SYS", 11u) == 0)     score += 10u;
    if (memcmp(d + 0x1E3u, "MSDOS   SYS", 11u) == 0)     score += 10u;
    return score;   /* der Codehash-Anteil entfaellt, siehe Testkopf */
}

/* ══════════════════════════════════════════════════════════════════ */

TEST(die_kennung_traegt_und_die_doktrin_bildet_die_zahl)
{
    uint8_t s[SEK];
    baue_mswin41(s, true);

    int k = -1;
    ASSERT(uft_win98_fdb_probe(s, SEK, &k));

    /* Am reinen Sektor liegen vor: KENNUNG (50), STRUKTUR (15),
     * GEOMETRIE (10) — SELBSTKONSISTENZ nicht, weil 512 Byte nicht die
     * 1 474 560 des BPB sind. 50 + 15 + 10 = 75. */
    printf("\n      Sektor allein: Konfidenz %d\n", k);
    ASSERT(k == uft_probe_konfidenz(UFT_BELEG_KENNUNG | UFT_BELEG_STRUKTUR
                                    | UFT_BELEG_GEOMETRIE));
    ASSERT(k == 75);

    /* Mit der ganzen Diskettengroesse kommt die Selbstkonsistenz dazu:
     * 75 + 25 = 100. Die Groesse darf also NICHT verworfen werden
     * (MF-1029). */
    int k2 = -1;
    ASSERT(uft_win98_fdb_probe(s, 1474560u, &k2));
    printf("      mit 1 474 560 Byte: Konfidenz %d\n", k2);
    ASSERT(k2 == 100);
    CHECK(k2 > k, "die Groesse aendert die Konfidenz nicht — dann wird sie "
                  "verworfen, und das ist die Falle aus MF-1029");
}

TEST(rot_probe_ohne_kennung_stimmte_die_zulieferung_zu)
{
    uint8_t s[SEK];
    baue_mswin41(s, false);   /* OEM = „MSDOS5.0" */

    /* VORZUSTAND: die Leiter der Zulieferung erreicht ohne die
     * MSWIN4.1-Kennung 40 — Sprung 10 + Signatur 10 + IO.SYS 10 +
     * MSDOS.SYS 10 —, und ihre Schwelle war `>= 30`. Sie haette also
     * ZUGESTIMMT. */
    const unsigned alt = leiter_der_zulieferung(s);
    printf("\n      Leiter der Zulieferung ohne Kennung: %u (Schwelle 30)\n",
           alt);
    ASSERT(alt == 40u);
    CHECK(alt >= 30u, "ROT-PROBE verfehlt: die alte Leiter haette schon "
                      "abgesagt — dann ist dieser Befund kein Befund");

    /* NACHZUSTAND: die Sonde sagt ab, und die Doktrin klemmt die Zahl. */
    int k = -1;
    ASSERT(!uft_win98_fdb_probe(s, SEK, &k));
    printf("      Doktrin ohne Kennung: %d (Klemme 45)\n", k);
    ASSERT(k <= 45);
    ASSERT(k == uft_probe_konfidenz(UFT_BELEG_STRUKTUR | UFT_BELEG_GEOMETRIE));

    /* ANTI-TAUTOLOGIE: mit Kennung stimmt dieselbe Sonde am sonst
     * gleichen Sektor zu. Ohne diese Gegenprobe pruefte die Zusage
     * oben nur, dass die Sonde immer „nein" sagt. */
    uint8_t mit[SEK];
    baue_mswin41(mit, true);
    int k_mit = -1;
    CHECK(uft_win98_fdb_probe(mit, SEK, &k_mit),
          "ROT-PROBE verfehlt: die Sonde sagt AUCH mit Kennung ab — dann "
          "misst sie nicht die Kennung, sondern sagt immer nein");
    ASSERT(k_mit > k);

    /* Und `parse` benennt die Absage statt sie zu verschweigen. */
    uft_win98_fdb_report_t r;
    ASSERT(uft_win98_fdb_parse(s, SEK, &r) == UFT_WIN98_FDB_UNRECOGNIZED);
    ASSERT(!r.has_oem_mswin41);
    /* Der Bericht ist trotzdem vollstaendig — eine Absage ist ein
     * Ergebnis, kein leerer Rueckgabewert. */
    ASSERT(r.fat_ok);
    ASSERT(r.has_io_sys_marker && r.has_msdos_sys_marker);
    ASSERT(r.code_fingerprint_fnv1a64 != 0u);
}

TEST(der_abdruck_nimmt_die_lokalisierten_texte_nicht_mit)
{
    uint8_t a[SEK], b[SEK];
    baue_mswin41(a, true);
    memcpy(b, a, SEK);

    uft_win98_fdb_report_t ra, rb;
    ASSERT(uft_win98_fdb_parse(a, SEK, &ra) == UFT_WIN98_FDB_OK);

    /* Ein Byte MITTEN in den Fehlermeldungen kippen — so, wie eine
     * andere Sprachfassung es tut. */
    b[UFT_WIN98_FDB_MSG_BEGIN + 4u] ^= 0xFFu;
    ASSERT(uft_win98_fdb_parse(b, SEK, &rb) == UFT_WIN98_FDB_OK);

    printf("\n      Codeabdruck  a=%016llx  b=%016llx\n",
           (unsigned long long)ra.code_fingerprint_fnv1a64,
           (unsigned long long)rb.code_fingerprint_fnv1a64);
    ASSERT(ra.code_fingerprint_fnv1a64 == rb.code_fingerprint_fnv1a64);
    ASSERT(ra.tail_fingerprint_fnv1a64 == rb.tail_fingerprint_fnv1a64);

    /* ROT-PROBE zur Gegenrichtung: die Spanne der Zulieferung
     * (0x03E..0x1FD) HAETTE sich geaendert, weil sie den Textbereich
     * mitnimmt. Nachgerechnet an denselben zwei Puffern. */
    uint64_t alt_a = UINT64_C(0xcbf29ce484222325);
    uint64_t alt_b = alt_a;
    for (unsigned i = 0x03Eu; i < 0x1FEu; ++i) {
        alt_a ^= a[i]; alt_a *= UINT64_C(0x100000001b3);
        alt_b ^= b[i]; alt_b *= UINT64_C(0x100000001b3);
    }
    printf("      Spanne der Zulieferung a=%016llx b=%016llx\n",
           (unsigned long long)alt_a, (unsigned long long)alt_b);
    CHECK(alt_a != alt_b,
          "ROT-PROBE verfehlt: auch die alte Spanne bleibt bei einer "
          "anderen Sprachfassung gleich — dann war die Korrektur "
          "unnoetig und dieser Test gehoert entfernt");

    /* Und der neue Abdruck reagiert SEHR WOHL auf eine Aenderung im
     * Code — sonst pruefte die Zusage oben nur, dass er konstant ist. */
    uint8_t c[SEK];
    memcpy(c, a, SEK);
    c[UFT_WIN98_FDB_CODE_BEGIN + 7u] ^= 0xFFu;
    uft_win98_fdb_report_t rc;
    ASSERT(uft_win98_fdb_parse(c, SEK, &rc) == UFT_WIN98_FDB_OK);
    CHECK(rc.code_fingerprint_fnv1a64 != ra.code_fingerprint_fnv1a64,
          "ROT-PROBE verfehlt: der Codeabdruck aendert sich auch bei einer "
          "Codeaenderung nicht — dann ist er wertlos");

    /* Dasselbe fuer das Unterprogramm, getrennt gefuehrt. */
    uint8_t d[SEK];
    memcpy(d, a, SEK);
    d[UFT_WIN98_FDB_SUBROUTINE_BEGIN + 2u] ^= 0xFFu;
    uft_win98_fdb_report_t rd;
    ASSERT(uft_win98_fdb_parse(d, SEK, &rd) == UFT_WIN98_FDB_OK);
    ASSERT(rd.tail_fingerprint_fnv1a64 != ra.tail_fingerprint_fnv1a64);
    ASSERT(rd.code_fingerprint_fnv1a64 == ra.code_fingerprint_fnv1a64);
}

TEST(der_bpb_kommt_aus_dem_baum_und_trifft_die_quelle)
{
    uint8_t s[SEK];
    baue_mswin41(s, true);
    uft_win98_fdb_report_t r;
    ASSERT(uft_win98_fdb_parse(s, SEK, &r) == UFT_WIN98_FDB_OK);
    ASSERT(r.fat_ok);

    /* Die BPB-Werte, wie die Quelle sie ausschreibt. */
    ASSERT(r.fat.bytes_per_sector    == 512u);
    ASSERT(r.fat.sectors_per_cluster == 1u);
    ASSERT(r.fat.reserved_sectors    == 1u);
    ASSERT(r.fat.fat_count           == 2u);
    ASSERT(r.fat.root_entry_count    == 224u);
    ASSERT(r.fat.total_sectors       == 2880u);
    ASSERT(r.fat.media_type          == 0xF0u);
    ASSERT(r.fat.sectors_per_fat     == 9u);
    ASSERT(r.fat.sectors_per_track   == 18u);
    ASSERT(r.fat.head_count          == 2u);
    ASSERT(r.fat.has_extended_bpb);
    ASSERT(r.fat.volume_serial       == 0x27156C21u);
    ASSERT(strcmp(r.fat.oem_name, "MSWIN4.1") == 0);

    /* Und jetzt die Zahl, die NICHT im BPB steht, sondern gerechnet
     * wird — und die die Quelle unabhaengig nennt: „which leaves us
     * with 2,847 sectors (2,880 - 33)". Der Analysator des Baums und
     * die Seite sind zwei Haende. */
    printf("\n      Wurzelverzeichnis %u Sektoren, Daten %u, Cluster %u\n",
           (unsigned)r.fat.root_dir_sectors, (unsigned)r.fat.data_sectors,
           (unsigned)r.fat.cluster_count);
    ASSERT(r.fat.root_dir_sectors == 14u);      /* 224 x 32 / 512 */
    ASSERT(r.fat.data_sectors     == 2847u);    /* 2880 - 1 - 18 - 14 */
    ASSERT(r.fat.cluster_count    == 2847u);    /* 1 Sektor je Cluster */
    ASSERT(r.fat.total_bytes      == UINT64_C(1474560));

    /* Gegenprobe zur Summe: 1 + 18 + 14 + 2847 = 2880. Eine Summe, die
     * aufgeht, sagt nichts ueber ihre Verteilung (MF-1026) — deshalb
     * steht sie ZULETZT und nicht statt der Einzelwerte. */
    ASSERT(1u + 2u * 9u + r.fat.root_dir_sectors + r.fat.data_sectors
           == r.fat.total_sectors);
}

TEST(kurz_leer_und_null_werden_abgesagt)
{
    uft_win98_fdb_report_t r;
    uint8_t klein[SEK - 1u];
    memset(klein, 0, sizeof(klein));

    ASSERT(uft_win98_fdb_parse(NULL, SEK, &r) == UFT_WIN98_FDB_INVALID_ARG);
    ASSERT(uft_win98_fdb_parse(klein, sizeof(klein), &r)
           == UFT_WIN98_FDB_TOO_SHORT);
    ASSERT(!uft_win98_fdb_probe(klein, sizeof(klein), NULL));
    ASSERT(uft_win98_fdb_parse_file(NULL, &r) == UFT_WIN98_FDB_INVALID_ARG);
    ASSERT(uft_win98_fdb_parse_file("gibt-es-nicht.img", &r)
           == UFT_WIN98_FDB_IO_ERROR);

    /* Ein Nullpuffer darf nichts beanspruchen — die Eichung aus
     * MF-729: auf lauter Nullen meldet niemand >= 50. */
    uint8_t nullen[SEK];
    memset(nullen, 0, sizeof(nullen));
    int k = -1;
    ASSERT(!uft_win98_fdb_probe(nullen, SEK, &k));
    printf("\n      Nullpuffer: Konfidenz %d\n", k);
    ASSERT(k < 50);

    /* Und `report` ist auch im Fehlerfall genullt, nicht halb gefuellt. */
    memset(&r, 0xAA, sizeof(r));
    ASSERT(uft_win98_fdb_parse(NULL, SEK, &r) == UFT_WIN98_FDB_INVALID_ARG);
    ASSERT(r.source_size == 0u);
    ASSERT(r.warning_count == 0u);
    ASSERT(r.confidence == 0);
}

TEST(die_datei_haelt_die_selbstkonsistenz)
{
    /* Eine 1 474 560 Byte grosse Datei mit dem Bootsektor vorn: dann
     * liegt die Selbstkonsistenz vor und die Konfidenz erreicht 100.
     * Der Test legt sie an und raeumt sie weg. */
    const char *pfad = "uft-test-win98-fdb.img";
    uint8_t s[SEK];
    baue_mswin41(s, true);

    FILE *f = fopen(pfad, "wb");
    ASSERT(f != NULL);
    bool ok = (fwrite(s, 1u, SEK, f) == SEK);
    static uint8_t leer[4096];
    memset(leer, 0, sizeof(leer));
    size_t rest = 1474560u - SEK;
    while (rest > 0u && ok) {
        const size_t n = rest < sizeof(leer) ? rest : sizeof(leer);
        if (fwrite(leer, 1u, n, f) != n) { ok = false; break; }
        rest -= n;
    }
    fclose(f);
    ASSERT(ok);
    ASSERT(rest == 0u);

    uft_win98_fdb_report_t r;
    const uft_win98_fdb_status_t st = uft_win98_fdb_parse_file(pfad, &r);
    remove(pfad);

    ASSERT(st == UFT_WIN98_FDB_OK);
    printf("\n      aus der Datei: %zu Byte, Konfidenz %d, %zu Warnungen\n",
           r.source_size, r.confidence, r.warning_count);
    ASSERT(r.source_size == 1474560u);
    ASSERT(r.image_size_matches_bpb);
    ASSERT(r.confidence == 100);
    ASSERT(!r.warnings_truncated);
    ASSERT(strcmp(uft_win98_fdb_status_name(st), "OK") == 0);
}

int main(void)
{
    printf("=== MSWIN4.1-Bootrecord (MF-1181) ===\n");
    RUN(die_kennung_traegt_und_die_doktrin_bildet_die_zahl);
    RUN(rot_probe_ohne_kennung_stimmte_die_zulieferung_zu);
    RUN(der_abdruck_nimmt_die_lokalisierten_texte_nicht_mit);
    RUN(der_bpb_kommt_aus_dem_baum_und_trifft_die_quelle);
    RUN(kurz_leer_und_null_werden_abgesagt);
    RUN(die_datei_haelt_die_selbstkonsistenz);
    printf("\n=== %d bestanden, %d gefallen ===\n", _pass, _fail);
    return _fail ? 1 : 0;
}
