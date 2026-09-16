/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_roland_osvol.c
 * @brief Waechter fuer uft_roland_ident und uft_os_volume.
 *
 * Laeuft ohne Hardware und ohne Korpus. Der Identifikationsteil baut seine
 * Sektoren selbst aus der veroeffentlichten Tabelle; der Geometrieteil prueft
 * die LBA/CHS-Abbildung gegen die vom Orakel fest verdrahtete Variante und
 * gegen zwei Geometrien, die das Orakel NICHT kann.
 */

#include "uft/formats/uft_roland_ident.h"
#include "uft/hal/uft_os_volume.h"
/* MF-1176: die Sondendoktrin (uft_probe_konfidenz, UFT_BELEG_*) und die
 * HAL-Merkmalstafel, deren erster Aufrufer dieser Test ist. */
#include "uft/uft_format_plugin.h"
#include "uft/hal/uft_hal.h"

#include <stdio.h>
#include <string.h>

static int g_fail = 0;

#define CHECK(cond, ...)                                                      \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("  FEHLGESCHLAGEN (%s:%d): ", __func__, __LINE__);         \
            printf(__VA_ARGS__); printf("\n"); g_fail++;                      \
        }                                                                     \
    } while (0)

/* ─────────────────────── Identifikation ───────────────────────────────── */

static void make_sector0(uint8_t s[512], const uft_roland_id_t *id) {
    memset(s, 0xE5, 512);                     /* typisches Fuellbyte */
    memcpy(s + UFT_ROLAND_KEY_OFF_LO, id->key_lo, 4);
    memcpy(s + UFT_ROLAND_KEY_OFF_HI, id->key_hi, 4);
}

static void t_all_seven_identify(void) {
    printf("Test 1: alle sieben Tabelleneintraege erkennen sich selbst\n");
    CHECK(uft_roland_id_count() == 7u, "erwartet 7 Eintraege, gefunden %zu",
          uft_roland_id_count());

    for (size_t i = 0; i < uft_roland_id_count(); ++i) {
        const uft_roland_id_t *want = uft_roland_id_at(i);
        uint8_t s[512];
        make_sector0(s, want);
        const uft_roland_id_t *got = uft_roland_identify(s, sizeof(s));
        CHECK(got == want, "Eintrag %zu (%s/%s) nicht wiedererkannt",
              i, want->model, want->content);
    }
}

static void t_model_content_split(void) {
    printf("Test 2: Typcode unterscheidet Inhaltsklassen innerhalb eines Modells\n");
    /* S330 Sound (0x08) gegen S330 Utility (0x09) — die ersten sieben Bytes
     * des Schluessels sind identisch, nur das achte trennt sie. Wer den
     * Schluessel als C-Zeichenkette ablegt, verliert genau dieses Byte. */
    const uft_roland_id_t *sound = uft_roland_id_at(0);
    const uft_roland_id_t *util  = uft_roland_id_at(1);
    CHECK(strcmp(sound->model, "S330") == 0 && strcmp(util->model, "S330") == 0,
          "beide Eintraege muessen S330 sein");
    CHECK(memcmp(sound->key_lo, util->key_lo, 4) == 0,
          "die ersten vier Schluesselbytes muessen gleich sein");
    CHECK(memcmp(sound->key_hi, util->key_hi, 4) != 0,
          "die zweite Schluesselhaelfte muss sich unterscheiden");
    CHECK(sound->key_hi[3] == 0x08 && util->key_hi[3] == 0x09,
          "Typcodes 0x08/0x09 erwartet, gefunden 0x%02X/0x%02X",
          sound->key_hi[3], util->key_hi[3]);

    uint8_t s[512];
    make_sector0(s, util);
    CHECK(uft_roland_identify(s, sizeof(s)) == util,
          "Utility-Diskette wurde nicht als Utility erkannt");
}

static void t_rejects_foreign(void) {
    printf("Test 3: Fremddisketten werden abgewiesen\n");
    uint8_t s[512];

    /* Ein FAT12-Bootsektor: JMP, OEM-Name, BPB. Groesse passt, Inhalt nicht. */
    memset(s, 0, sizeof(s));
    s[0] = 0xEB; s[1] = 0x3C; s[2] = 0x90;
    memcpy(s + 3, "MSDOS5.0", 8);
    CHECK(uft_roland_identify(s, sizeof(s)) == NULL,
          "FAT12-Bootsektor darf nicht als Roland gelten");

    int conf = -1;
    CHECK(!uft_roland_probe(s, sizeof(s), UFT_ROLAND_IMAGE_SIZE, &conf),
          "Probe muss bei passender Groesse und fremdem Inhalt false liefern");

    /* Nur Nullen. */
    memset(s, 0, sizeof(s));
    CHECK(uft_roland_identify(s, sizeof(s)) == NULL,
          "Nullsektor darf nicht treffen");

    /* Zu kurzer Puffer. */
    const uft_roland_id_t *id = uft_roland_id_at(0);
    make_sector0(s, id);
    CHECK(uft_roland_identify(s, UFT_ROLAND_IDENT_LEN - 1u) == NULL,
          "zu kurzer Puffer muss NULL ergeben");
}

static void t_size_never_decides(void) {
    printf("Test 4: die Groesse allein entscheidet nie\n");
    const uft_roland_id_t *id = uft_roland_id_at(3);   /* S550 */
    uint8_t s[512];
    make_sector0(s, id);

    int conf = 0;
    CHECK(uft_roland_probe(s, sizeof(s), UFT_ROLAND_IMAGE_SIZE, &conf),
          "richtiger Inhalt und richtige Groesse muessen treffen");

    /* MF-1176: hier stand `conf >= 90 && conf <= 99`. Die Zulieferung
     * vergab 95 von Hand; seit MF-1153 ist `uft_probe_konfidenz()` die
     * EINZIGE erlaubte Quelle einer Sondenkonfidenz. Belegt ist genau eine
     * Kennung (zwei 32-Bit-Worte an fester Position) — Summe 50, Band
     * „Struktur gelesen". Das Merkmalsband 80..100 verlangt eine Kennung
     * UND zwei weitere Belege; die gibt es erst mit einem Verzeichnisleser
     * (P3-428). Die Herleitung steht genau EINMAL, in der Funktion; hier
     * wird nur die Zahl festgenagelt, die dabei herauskommt. */
    CHECK(conf == uft_probe_konfidenz(UFT_BELEG_KENNUNG),
          "die Zuversicht muss aus der Doktrin kommen, war %d, erwartet %d",
          conf, uft_probe_konfidenz(UFT_BELEG_KENNUNG));
    CHECK(conf == 50, "die Leiter ergibt fuer eine Kennung allein 50, war %d",
          conf);
    /* Und die Gegenprobe zur Klemme: OHNE Kennung waeren 25+15+10 = 50,
     * die Doktrin klemmt auf 45. Steht hier, damit die 50 oben nicht
     * ZUFAELLIG richtig aussieht (Tor 64). */
    CHECK(uft_probe_konfidenz(UFT_BELEG_SELBSTKONSISTENZ |
                              UFT_BELEG_STRUKTUR |
                              UFT_BELEG_GEOMETRIE) == 45,
          "ohne Kennung muss die Obergrenze 45 greifen");

    /* Gleicher Inhalt, falsche Groesse (HD-Abbild). */
    CHECK(!uft_roland_probe(s, sizeof(s), 1474560u, &conf),
          "falsche Groesse muss abweisen");

    /* Und die Gegenprobe zum eigentlichen Problem: 737.280 Byte gehoeren im
     * Baum schon neun anderen Formaten. Ohne Inhaltstreffer darf hier nie
     * etwas zurueckkommen. */
    memset(s, 0x00, sizeof(s));
    CHECK(!uft_roland_probe(s, sizeof(s), UFT_ROLAND_IMAGE_SIZE, &conf),
          "737280 Byte allein darf nicht genuegen");
}

/* ─────────────────────── Geometrie / HAL ──────────────────────────────── */

static void t_geometry_roland(void) {
    printf("Test 5: LBA/CHS gegen die Rechnung des Orakels\n");
    /* Orakel VA 0x402e44, fest verdrahtet:
     *   cyl = lba / 18; rem = lba % 18;
     *   head = rem >= 9 ? 1 : 0;  sector = rem >= 9 ? rem - 9 : rem;
     * Unsere Fassung muss dasselbe liefern, aber parametrisiert. */
    const uft_osvol_geometry_t g = { 80, 2, 9, 512, false };
    uft_osvol_chs_t c;

    for (uint32_t lba = 0; lba < 1440u; ++lba) {
        CHECK(uft_osvol_lba_to_chs(&g, lba, 0u, &c), "LBA %u abgewiesen", lba);
        uint32_t ref_cyl  = lba / 18u;
        uint32_t rem      = lba % 18u;
        uint32_t ref_head = rem >= 9u ? 1u : 0u;
        uint32_t ref_sec  = rem >= 9u ? rem - 9u : rem;
        if (c.cyl != ref_cyl || c.head != ref_head || c.sector != ref_sec) {
            CHECK(false, "LBA %u: erwartet C%u H%u S%u, erhalten C%u H%u S%u",
                  lba, ref_cyl, ref_head, ref_sec, c.cyl, c.head, c.sector);
            break;
        }
    }

    /* 1-basierte Sektornummern, wie INT 13h und die meisten FDC sie wollen. */
    CHECK(uft_osvol_lba_to_chs(&g, 0u, 1u, &c) && c.sector == 1u,
          "LBA 0 muss mit sector_base=1 auf Sektor 1 abbilden");
    CHECK(uft_osvol_lba_to_chs(&g, 9u, 1u, &c) && c.head == 1u && c.sector == 1u,
          "LBA 9 muss auf Kopf 1, Sektor 1 abbilden");

    /* Ueberlauf. Das Orakel prueft das nicht. */
    CHECK(!uft_osvol_lba_to_chs(&g, uft_osvol_total_sectors(&g), 0u, &c),
          "Sektor hinter dem Medium muss abgewiesen werden");

    CHECK(uft_osvol_total_sectors(&g) == 1440u, "1440 Sektoren erwartet");
    CHECK(uft_osvol_image_size(&g) == UFT_ROLAND_IMAGE_SIZE,
          "737280 Byte erwartet, gerechnet %llu",
          (unsigned long long)uft_osvol_image_size(&g));
}

static void t_geometry_others(void) {
    printf("Test 6: Geometrien, die das Orakel nicht kann\n");
    /* Akai S900 DD: 5 Sektoren a 1024, 80x2 -> 819.200 (UFT: akai_s900, T2) */
    const uft_osvol_geometry_t akai = { 80, 2, 5, 1024, false };
    CHECK(uft_osvol_image_size(&akai) == 819200u,
          "Akai DD: 819200 erwartet, gerechnet %llu",
          (unsigned long long)uft_osvol_image_size(&akai));

    uft_osvol_chs_t c;
    CHECK(uft_osvol_lba_to_chs(&akai, 5u, 0u, &c) && c.cyl == 0u &&
          c.head == 1u && c.sector == 0u,
          "Akai LBA 5 muss C0 H1 S0 sein");

    /* Korg DSS-1: 80x2x5x1024 — dieselbe Geometrie, anderes Dateisystem.
     * Belegt, dass Geometrie allein auch hier nicht identifiziert. */
    const uft_osvol_geometry_t korg = { 80, 2, 5, 1024, false };
    CHECK(uft_osvol_image_size(&korg) == uft_osvol_image_size(&akai),
          "Akai und Korg haben dieselbe Groesse — Inhalt muss entscheiden");

    /* 48-TPI-Medium, 40 Zylinder, Doppelschritt. */
    const uft_osvol_geometry_t dd40 = { 40, 2, 9, 512, true };
    CHECK(uft_osvol_image_size(&dd40) == 368640u, "360K erwartet");
    CHECK(dd40.double_step, "Doppelschritt muss gesetzt bleiben");
}

static void t_status_summary(void) {
    printf("Test 7: Statusfeld und Zusammenfassung\n");
    uft_osvol_sec_status_t st[10];
    memset(st, 0, sizeof(st));
    for (int i = 0; i < 10; ++i) { st[i].state = UFT_OSVOL_SEC_OK; st[i].attempts = 1; }
    st[3].state = UFT_OSVOL_SEC_OK_RETRY; st[3].attempts = 4;
    st[7].state = UFT_OSVOL_SEC_BAD;      st[7].attempts = 8; st[7].os_error = 5;

    char buf[512];
    size_t n = uft_osvol_status_summary(st, 10u, buf, sizeof(buf));
    CHECK(n > 0u, "Zusammenfassung leer");
    CHECK(strstr(buf, "8 gut") != NULL, "Zahl der guten Sektoren fehlt:\n%s", buf);
    CHECK(strstr(buf, "1 defekt") != NULL, "Zahl der defekten fehlt");
    CHECK(strstr(buf, "Erster defekter Sektor: 7") != NULL,
          "erster defekter Sektor nicht benannt");
    printf("    --- Beispielausgabe ---\n");
    fputs(buf, stdout);
    printf("    -----------------------\n");
}

/* ─────────────────────── HAL-Merkmalstafel ──────────────────────────────
 *
 * MF-1176. Diese Zusage ist der Grund, warum `UFT_CAPS_OS_VOLUME`
 * existiert, und sie wird ROT, wenn die Registrierung in
 * `g_controller_caps[]` wieder verschwindet (D2). Sie ist zugleich der
 * ERSTE Aufrufer von `uft_hal_get_controller_caps()` im ganzen Baum —
 * gemessen hatte die Funktion vorher 0 ausserhalb ihrer eigenen Datei,
 * ebenso ihre drei Nachbarn. Dass eine Merkmalstafel niemand liest, ist die
 * Lage P3-429; hier faengt der Weg heraus an.
 * ───────────────────────────────────────────────────────────────────────── */

static void t_hal_profil_ist_registriert(void) {
    printf("Test 8: die HAL-Merkmalstafel kennt den OS-Volume-Weg\n");

    const uft_controller_caps_t *c =
        uft_hal_get_controller_caps(UFT_CTRL_OS_VOLUME);
    CHECK(c != NULL, "UFT_CTRL_OS_VOLUME muss in g_controller_caps[] stehen");
    if (!c) return;

    CHECK(c->type == UFT_CTRL_OS_VOLUME, "Typfeld muss auf sich selbst zeigen");
    CHECK(c->name != NULL && c->name[0] != '\0', "Name muss gesetzt sein");

    /* Die beiden Zahlen aus dem Auftrag, woertlich. */
    CHECK(c->can_read_flux == false, "can_read_flux MUSS false sein");
    CHECK(c->can_read_sector == true, "can_read_sector MUSS true sein");
    /* und die drei, die daraus folgen */
    CHECK(c->can_read_bitstream == false, "kein Zellstrom auf diesem Weg");
    CHECK(c->can_write_flux == false, "kein Flussschreiben");
    CHECK(c->hardware_index == false, "kein Indexsignal");
    CHECK(c->copy_protection_support == false, "kein Kopierschutz");
    CHECK(c->weak_bit_detection == false, "keine Weak-Bit-Erkennung");

    /* Die Selbstbeschraenkung muss AUSSPRECHBAR sein, nicht nur wahr:
     * drei Begriffe, die die Oberflaeche zitieren kann. Ohne diese Zusage
     * waere `can_read_flux = false` eine Zahl ohne Erklaerung. */
    bool weak = false, phantom = false, schutz = false;
    unsigned n = 0u;
    for (unsigned i = 0; i < UFT_CAPS_MAX_LIMITATIONS && c->limitations[i]; i++) {
        n++;
        if (strstr(c->limitations[i], "Weak Bits"))      weak = true;
        if (strstr(c->limitations[i], "Phantomsektor"))  phantom = true;
        if (strstr(c->limitations[i], "Kopierschutz"))   schutz = true;
    }
    CHECK(n > 0u, "die Einschraenkungsliste darf nicht leer sein");
    CHECK(weak, "Weak Bits muessen als unerreichbar benannt sein");
    CHECK(phantom, "Phantomsektoren muessen als unerreichbar benannt sein");
    CHECK(schutz, "Kopierschutz muss als unerreichbar benannt sein");

    /* Gegenprobe: ein Flusscontroller derselben Tafel sagt das Gegenteil.
     * Ohne sie koennte die Tafel fuer JEDEN Eintrag false melden und die
     * Zusage oben waere wertlos (Tor 64). */
    const uft_controller_caps_t *gw =
        uft_hal_get_controller_caps(UFT_CTRL_GREASEWEAZLE);
    CHECK(gw != NULL, "Greaseweazle muss in derselben Tafel stehen");
    if (gw) CHECK(gw->can_read_flux == true,
                  "Greaseweazle MUSS Fluss lesen koennen - sonst sagt die "
                  "Tafel bei jedem Eintrag dasselbe");

    /* Und ein Typ, den es nicht gibt, muss NULL ergeben statt zu raten. */
    CHECK(uft_hal_get_controller_caps(UFT_CTRL_COUNT) == NULL,
          "ein unbekannter Typ muss NULL liefern");
}

/* ─────────────────────── uft_osvol_open sagt ab ─────────────────────────
 *
 * MF-1176. Diese Zusage braucht kein Laufwerk, keine Datei und keinen
 * Korpus: sie prueft die drei Argumentschranken und den Fall „Pfad gibt es
 * nicht". Dabei loest sie zugleich zwei Zusagen ein, die sonst
 * uneingeloest blieben — `lock_volume` und `no_buffering` standen gemessen
 * als „Feld in einem oeffentlichen Header, das NIRGENDS geschrieben wird"
 * (audit_dead_fields). Hier werden sie gesetzt, und zwar nicht, um eine
 * Zahl zu bewegen, sondern weil der Aufbau eines vollstaendigen
 * Parametersatzes genau der Fall ist, den ein Aufrufer bauen muss.
 *
 * Der eigentliche Prueflingssatz: `uft_osvol_open()` darf NIE etwas
 * zurueckgeben, das wie Erfolg aussieht. Und die Unterscheidung, die das
 * Orakel nicht macht: ein ARGUMENTfehler setzt keinen OS-Fehler, ein
 * ECHTER Fehlschlag am Medium setzt ihn.
 * ───────────────────────────────────────────────────────────────────────── */

static void t_open_sagt_ab(void) {
    printf("Test 9: uft_osvol_open weist ab statt Erfolg zu melden\n");

    const uft_osvol_geometry_t roland = {
        .cylinders = UFT_ROLAND_CYLINDERS, .heads = UFT_ROLAND_HEADS,
        .sectors = UFT_ROLAND_SPT, .sector_size = UFT_ROLAND_SECTOR_SIZE,
        .double_step = false
    };

    int32_t fehler = -1;

    /* (1) kein Parametersatz */
    CHECK(uft_osvol_open(NULL, &fehler) == NULL, "NULL-Parameter -> NULL");
    CHECK(fehler == 0, "ein Argumentfehler setzt KEINEN OS-Fehler, war %ld",
          (long)fehler);

    /* (2) kein Pfad */
    uft_osvol_open_params_t p = {
        .path = NULL, .geo = roland, .retries = 3u,
        .write_enable = false, .lock_volume = true, .no_buffering = true
    };
    fehler = -1;
    CHECK(uft_osvol_open(&p, &fehler) == NULL, "NULL-Pfad -> NULL");
    CHECK(fehler == 0, "auch hier kein OS-Fehler, war %ld", (long)fehler);

    /* (3) Geometrie, die es nicht gibt — je ein Feld auf 0 */
    p.path = "uft_osvol_gibt_es_nicht.bin";
    const uft_osvol_geometry_t leer[3] = {
        { .cylinders = 80u, .heads = 0u, .sectors = 9u,  .sector_size = 512u },
        { .cylinders = 80u, .heads = 2u, .sectors = 0u,  .sector_size = 512u },
        { .cylinders = 80u, .heads = 2u, .sectors = 9u,  .sector_size = 0u   },
    };
    for (unsigned i = 0; i < 3u; i++) {
        p.geo = leer[i];
        fehler = -1;
        CHECK(uft_osvol_open(&p, &fehler) == NULL,
              "Geometrie %u mit einer 0 muss abgewiesen werden", i);
        CHECK(fehler == 0, "Geometrie %u: kein OS-Fehler erwartet, war %ld",
              i, (long)fehler);
    }

    /* (4) vollstaendiger Satz, aber der Pfad gibt es nicht. JETZT muss ein
     *     OS-Fehler durchkommen — das ist der Unterschied zu (1)-(3), und
     *     er ist der Grund, warum `os_error` im Vertrag steht. */
    p.geo = roland;
    p.path = "uft_osvol_gibt_es_nicht.bin";
    fehler = 0;
    CHECK(uft_osvol_open(&p, &fehler) == NULL,
          "ein Pfad, den es nicht gibt, darf nicht aufgehen");
    CHECK(fehler != 0,
          "ein echter Fehlschlag MUSS den OS-Fehler durchreichen, war %ld",
          (long)fehler);

    /* (5) und die Geometrierechnung des Parametersatzes geht auf */
    CHECK(uft_osvol_total_sectors(&roland) == 1440u,
          "80 x 2 x 9 sind 1440, war %lu",
          (unsigned long)uft_osvol_total_sectors(&roland));
    CHECK(uft_osvol_image_size(&roland) == UFT_ROLAND_IMAGE_SIZE,
          "1440 x 512 muessen die Roland-DD-Groesse sein");
}

int main(void) {
    printf("=== test_roland_osvol ===\n\n");
    t_all_seven_identify();
    t_model_content_split();
    t_rejects_foreign();
    t_size_never_decides();
    t_geometry_roland();
    t_geometry_others();
    t_status_summary();
    t_hal_profil_ist_registriert();
    t_open_sagt_ab();
    printf("\n%s (%d Fehler)\n", g_fail ? "FEHLGESCHLAGEN" : "BESTANDEN", g_fail);
    return g_fail ? 1 : 0;
}
