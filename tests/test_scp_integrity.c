/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_scp_integrity.c
 * @brief Waechter fuer uft_scp_integrity: Pruefsumme, Tabellenbefund, Seiten.
 *
 * Der Test baut seine SCP-Dateien selbst. Er braucht keinen Korpus, keine
 * Hardware und keine Fremdwerkzeuge — damit laeuft er in jeder CI, auch der
 * ohne Netz.
 *
 * Geprueft werden fuenf Faelle:
 *
 *   1. Saubere zweiseitige Datei         → Pruefsumme OK, keine Auffaelligkeit
 *   2. Nur Seite 1 aufgenommen (0x0A=2)  → Seiten korrekt, kein Widerspruch
 *   3. Nur Seite 1, Kopf sagt "beide"    → aus der Tabelle erkannt + Widerspruch
 *   4. Datei durch SCPmodSideB.py        → Pruefsumme falsch UND Tabelle aliasiert
 *   5. Nachtraeglich veraenderte Nutzlast→ Pruefsumme falsch, Tabelle sauber
 *
 * Fall 4 ist der eigentliche Anlass: er beweist, dass BEIDE Detektoren
 * unabhaengig voneinander anschlagen. Faellt einer aus, faellt der Test auf.
 */

#include "uft/formats/uft_scp_integrity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;

#define CHECK(cond, ...)                                                      \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("  FEHLGESCHLAGEN (%s:%d): ", __func__, __LINE__);         \
            printf(__VA_ARGS__);                                              \
            printf("\n");                                                     \
            g_fail++;                                                         \
        }                                                                     \
    } while (0)

/* ────────────────────────── Aufbau von Testdateien ─────────────────────── */

#define TEST_CYLS     4u
#define TDH_SIZE     (4u + 12u + 16u)  /* "TRK"+Nr, eine Umdrehung, Flussdaten */

typedef struct {
    uint8_t *data;
    size_t   size;
} blob_t;

static void wr_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void seal_checksum(blob_t *b) {
    wr_le32(b->data + UFT_SCP_OFF_CHECKSUM,
            uft_scp_integrity_compute_checksum(b->data, b->size));
}

/**
 * Baut eine SCP-Datei mit TEST_CYLS Zylindern.
 * @param heads_byte  Wert fuer 0x0A
 * @param write_side0 Seite-0-Spuren anlegen
 * @param write_side1 Seite-1-Spuren anlegen
 */
static blob_t build_scp(uint8_t heads_byte, bool write_side0, bool write_side1) {
    unsigned tracks = (write_side0 ? TEST_CYLS : 0u)
                    + (write_side1 ? TEST_CYLS : 0u);
    size_t size = UFT_SCP_MIN_FILE_SIZE + (size_t)tracks * TDH_SIZE;

    blob_t b = { calloc(1, size), size };
    if (!b.data) { perror("calloc"); exit(1); }

    memcpy(b.data, "SCP", 3);
    b.data[UFT_SCP_OFF_VERSION]   = 0x19;   /* v1.9 */
    b.data[UFT_SCP_OFF_DISKTYPE]  = 0x00;
    b.data[UFT_SCP_OFF_REVS]      = 1;
    b.data[UFT_SCP_OFF_START_TRK] = 0;
    b.data[UFT_SCP_OFF_END_TRK]   = (uint8_t)(TEST_CYLS * 2u - 1u);
    b.data[UFT_SCP_OFF_FLAGS]     = 0x01;   /* Index gespeichert */
    b.data[UFT_SCP_OFF_CELLWIDTH] = 0;
    b.data[UFT_SCP_OFF_HEADS]     = heads_byte;
    b.data[UFT_SCP_OFF_RESOLUTION]= 0;

    size_t pos = UFT_SCP_MIN_FILE_SIZE;
    for (unsigned cyl = 0; cyl < TEST_CYLS; ++cyl) {
        for (unsigned head = 0; head < 2u; ++head) {
            if (head == 0u && !write_side0) continue;
            if (head == 1u && !write_side1) continue;

            unsigned idx = uft_scp_track_index(cyl, head);
            wr_le32(b.data + UFT_SCP_TDHT_OFFSET + idx * 4u, (uint32_t)pos);

            memcpy(b.data + pos, "TRK", 3);
            b.data[pos + 3] = (uint8_t)idx;          /* Spurnummer im TDH */
            wr_le32(b.data + pos + 4,  200000u);     /* Indexzeit          */
            wr_le32(b.data + pos + 8,  8u);          /* Zellen             */
            wr_le32(b.data + pos + 12, (uint32_t)(pos + 16u - pos)); /* Datenoffset */
            for (unsigned i = 0; i < 16u; ++i)
                b.data[pos + 16u + i] = (uint8_t)(0x40u + idx + i);
            pos += TDH_SIZE;
        }
    }

    seal_checksum(&b);
    return b;
}

/** Wendet die Umformung von SCPmodSideB.py an: Seite-B-Offset in beide Plaetze. */
static void apply_scpmod_sideb(blob_t *b) {
    uint8_t *t = b->data + UFT_SCP_TDHT_OFFSET;
    for (unsigned cyl = 0; cyl < UFT_SCP_MAX_TRACKS / 2u; ++cyl) {
        memcpy(t + (cyl * 2u + 0u) * 4u, t + (cyl * 2u + 1u) * 4u, 4u);
    }
    /* Die Pruefsumme wird BEWUSST nicht erneuert — genau das tut das
     * Originalskript auch nicht. */
}

/* ──────────────────────────────── Faelle ────────────────────────────────── */

static void case1_clean_double_sided(void) {
    printf("Fall 1: saubere zweiseitige Datei\n");
    blob_t b = build_scp(UFT_SCP_HEADS_BOTH, true, true);

    uft_scp_integrity_t r;
    CHECK(uft_scp_check_integrity(b.data, b.size, UFT_SCP_SIDES_AUTO,
                                  UFT_SCP_INTERP_NONE, &r), "Pruefung schlug fehl");
    CHECK(r.checksum == UFT_SCP_CKSUM_OK, "Pruefsumme sollte stimmen");
    CHECK(!r.tampered, "keine Manipulation erwartet");
    CHECK(r.tdht.aliased_pairs == 0u, "keine aliasierten Paare erwartet");
    CHECK(r.tdht.populated == TEST_CYLS * 2u, "erwartet %u belegte Eintraege, gefunden %u",
          TEST_CYLS * 2u, r.tdht.populated);
    CHECK(r.sides.sides == UFT_SCP_SIDES_BOTH, "beide Seiten erwartet");
    CHECK(r.sides.head_count == 2u, "zwei Koepfe erwartet");
    CHECK(!r.sides.header_contradicts, "kein Widerspruch erwartet");
    CHECK(r.sides.cylinders == TEST_CYLS, "erwartet %u Zylinder, gefunden %u",
          TEST_CYLS, r.sides.cylinders);
    free(b.data);
}

static void case2_sideb_declared(void) {
    printf("Fall 2: nur Seite 1, Kopfbyte sagt es korrekt\n");
    blob_t b = build_scp(UFT_SCP_HEADS_SIDE1, false, true);

    uft_scp_integrity_t r;
    uft_scp_check_integrity(b.data, b.size, UFT_SCP_SIDES_AUTO,
                            UFT_SCP_INTERP_NONE, &r);
    CHECK(r.checksum == UFT_SCP_CKSUM_OK, "Pruefsumme sollte stimmen");
    CHECK(!r.tampered, "keine Manipulation erwartet");
    CHECK(r.sides.sides == UFT_SCP_SIDES_SIDE1, "Seite 1 erwartet");
    CHECK(r.sides.head_count == 1u, "ein Kopf erwartet");
    CHECK(r.sides.first_head == 1u, "erster Kopf sollte 1 sein");
    CHECK(r.sides.from_header, "Deutung sollte aus dem Kopfbyte stammen");
    CHECK(!r.sides.header_contradicts, "kein Widerspruch erwartet");
    CHECK(r.tdht.side0_empty, "Seite 0 sollte leer sein");
    free(b.data);
}

static void case3_sideb_undeclared(void) {
    printf("Fall 3: nur Seite 1, Kopfbyte behauptet \"beide\"\n");
    blob_t b = build_scp(UFT_SCP_HEADS_BOTH, false, true);

    uft_scp_integrity_t r;
    uft_scp_check_integrity(b.data, b.size, UFT_SCP_SIDES_AUTO,
                            UFT_SCP_INTERP_NONE, &r);
    CHECK(r.checksum == UFT_SCP_CKSUM_OK, "Pruefsumme sollte stimmen");
    CHECK(r.sides.sides == UFT_SCP_SIDES_SIDE1,
          "Seite 1 aus der Tabelle erwartet");
    CHECK(r.sides.from_tdht, "Deutung sollte aus der Tabelle stammen");
    CHECK(r.sides.header_contradicts,
          "Widerspruch Kopf/Tabelle muss gemeldet werden");
    /* Kein tampered: die Datei ist ehrlich, nur ihr Kopf ist ungenau. */
    CHECK(!r.tampered, "ungenauer Kopf ist keine Manipulation");
    free(b.data);
}

static void case4_scpmod_sideb(void) {
    printf("Fall 4: durch SCPmodSideB.py veraendert\n");
    blob_t b = build_scp(UFT_SCP_HEADS_BOTH, false, true);
    apply_scpmod_sideb(&b);

    uft_scp_integrity_t r;
    uft_scp_check_integrity(b.data, b.size, UFT_SCP_SIDES_AUTO,
                            UFT_SCP_INTERP_NONE, &r);

    /* Detektor 1: die Pruefsumme wurde nicht erneuert. */
    CHECK(r.checksum == UFT_SCP_CKSUM_MISMATCH,
          "Pruefsumme muss als falsch erkannt werden");
    CHECK(r.checksum_stored != r.checksum_computed,
          "gespeicherter und berechneter Wert muessen abweichen");

    /* Detektor 2: die Tabelle ist systematisch aliasiert. */
    CHECK(r.tdht.looks_aliased, "aliasierte Tabelle muss erkannt werden");
    CHECK(r.tdht.aliased_pairs == TEST_CYLS,
          "erwartet %u aliasierte Zylinder, gefunden %u",
          TEST_CYLS, r.tdht.aliased_pairs);

    CHECK(r.tampered, "Datei muss als veraendert gelten");

    /* Und: die Datei sieht nun zweiseitig aus, obwohl sie es nicht ist —
     * genau die falsche Aussage, gegen die die Pruefung schuetzt. */
    CHECK(!r.tdht.side0_empty && !r.tdht.side1_empty,
          "nach der Umformung wirken beide Seiten belegt");
    free(b.data);
}

static void case5_payload_tampered(void) {
    printf("Fall 5: Nutzlast nachtraeglich veraendert\n");
    blob_t b = build_scp(UFT_SCP_HEADS_BOTH, true, true);
    b.data[b.size - 1u] ^= 0xFFu;   /* ein Byte im letzten Flussstrom */

    uft_scp_integrity_t r;
    uft_scp_check_integrity(b.data, b.size, UFT_SCP_SIDES_AUTO,
                            UFT_SCP_INTERP_NONE, &r);
    CHECK(r.checksum == UFT_SCP_CKSUM_MISMATCH,
          "veraenderte Nutzlast muss die Pruefsumme brechen");
    CHECK(!r.tdht.looks_aliased, "Tabelle ist unveraendert");
    CHECK(r.tampered, "Datei muss als veraendert gelten");
    free(b.data);
}

static void case6_not_stored(void) {
    printf("Fall 6: Pruefsummenfeld ist 0 (nicht gebildet)\n");
    blob_t b = build_scp(UFT_SCP_HEADS_BOTH, true, true);
    wr_le32(b.data + UFT_SCP_OFF_CHECKSUM, 0u);

    uft_scp_integrity_t r;
    uft_scp_check_integrity(b.data, b.size, UFT_SCP_SIDES_AUTO,
                            UFT_SCP_INTERP_NONE, &r);
    CHECK(r.checksum == UFT_SCP_CKSUM_NOT_STORED,
          "leeres Feld darf kein Fehlalarm sein");
    CHECK(!r.tampered, "leeres Feld ist keine Manipulation");
    free(b.data);
}

static void case7_forced_interpretation(void) {
    printf("Fall 7: erzwungene Deutung schlaegt alles\n");
    blob_t b = build_scp(UFT_SCP_HEADS_SIDE1, false, true);

    uft_scp_integrity_t r;
    uft_scp_check_integrity(b.data, b.size, UFT_SCP_SIDES_AUTO,
                            UFT_SCP_INTERP_DS80, &r);
    CHECK(r.sides.head_count == 2u, "ds80 erzwingt zwei Koepfe");
    CHECK(r.sides.cylinders == 80u, "ds80 erzwingt 80 Zylinder");
    CHECK(!r.sides.double_step, "ds80 ohne Doppelschritt");
    CHECK(r.sides.from_override, "Herkunft muss als Vorgabe gelten");

    uft_scp_check_integrity(b.data, b.size, UFT_SCP_SIDES_AUTO,
                            UFT_SCP_INTERP_SS40, &r);
    CHECK(r.sides.head_count == 1u, "ss40 erzwingt einen Kopf");
    CHECK(r.sides.cylinders == 40u, "ss40 erzwingt 40 Zylinder");
    CHECK(r.sides.double_step, "ss40 mit Doppelschritt");
    free(b.data);
}

static void show_summary_example(void) {
    printf("\nBeispielausgabe fuer eine durch SCPmodSideB.py veraenderte Datei:\n");
    printf("------------------------------------------------------------\n");
    blob_t b = build_scp(UFT_SCP_HEADS_BOTH, false, true);
    apply_scpmod_sideb(&b);

    uft_scp_integrity_t r;
    uft_scp_check_integrity(b.data, b.size, UFT_SCP_SIDES_AUTO,
                            UFT_SCP_INTERP_NONE, &r);
    char buf[1024];
    uft_scp_integrity_summary(&r, buf, sizeof(buf));
    fputs(buf, stdout);
    printf("------------------------------------------------------------\n");
    free(b.data);
}

int main(void) {
    printf("=== test_scp_integrity ===\n\n");
    case1_clean_double_sided();
    case2_sideb_declared();
    case3_sideb_undeclared();
    case4_scpmod_sideb();
    case5_payload_tampered();
    case6_not_stored();
    case7_forced_interpretation();
    show_summary_example();

    printf("\n%s (%d Fehler)\n", g_fail == 0 ? "BESTANDEN" : "FEHLGESCHLAGEN",
           g_fail);
    return g_fail == 0 ? 0 : 1;
}
