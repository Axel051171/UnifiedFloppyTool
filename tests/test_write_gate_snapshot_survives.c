/**
 * @file test_write_gate_snapshot_survives.c
 * @brief Was das Tor der Oberflaeche verspricht, muss es halten (MF-573)
 *
 * -- Worum es geht -------------------------------------------------------
 *
 * Fuenf Slots des Datei-Browsers und `MainWindow::onSave()` aendern
 * Abbilder an Ort und Stelle. Seit MF-573 fragen sie vorher
 * `uft_write_gate_precheck()` mit der Richtlinie
 * @ref UFT_GATE_POLICY_IMAGE_ONLY.
 *
 * Die Zusage dahinter ist nicht "das Tor lief", sondern: **der Zustand vor
 * der Aenderung existiert noch, wenn die Aenderung schiefgeht.** Genau das
 * prueft dieser Test, und zwar so, wie die Oberflaeche es benutzt.
 *
 * `tests/test_write_gate.c` prueft das Tor fuer sich (20 Faelle,
 * einschliesslich "der Schnappschuss ist wirklich da"). Was DORT fehlt,
 * ist der Fall, um den es der Oberflaeche geht: die Quelldatei wird
 * anschliessend zerstoert.
 *
 * -- Warum das nicht selbstverstaendlich ist -----------------------------
 *
 * `MainWindow::onSave()` oeffnet das Ziel mit `QIODevice::WriteOnly`, und
 * das KUERZT beim Oeffnen. Im normalen Speichern-Fall ist Quelle == Ziel,
 * das Original ist also schon weg, bevor ein Byte geschrieben wurde. Ohne
 * Schnappschuss haette eine kurze Schreibung nichts uebrig gelassen —
 * MF-571 hat dort sogar die Teildatei entfernt, was gegen die falsche
 * Erfolgsmeldung richtig war und den letzten Rest kostete.
 */

#include "uft/policy/uft_write_gate.h"
#include "uft/core/uft_snapshot.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond, ...)                                                   \
    do {                                                                   \
        if (cond) { printf("  ok   " __VA_ARGS__); printf("\n"); }         \
        else { printf("  FAIL " __VA_ARGS__); printf("\n"); failures++; }  \
    } while (0)

#define ADF_LEN 901120

/** Eine ADF, die die Formaterkennung des Tors besteht. */
static uint8_t *make_adf(void)
{
    uint8_t *b = (uint8_t *)calloc(1, ADF_LEN);
    if (!b) return NULL;
    /* Bootblock-Signatur "DOS\0" — dieselbe Bauart wie in
     * tests/test_write_gate.c. */
    b[0] = 'D'; b[1] = 'O'; b[2] = 'S'; b[3] = 0;
    for (size_t i = 1024; i < ADF_LEN; i++)
        b[i] = (uint8_t)(i * 7 + (i >> 9));
    return b;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Der Schnappschuss ueberlebt die Zerstoerung (MF-573) ===\n");

    uint8_t *img = make_adf();
    if (!img) { printf("  kein Speicher\n"); return 2; }

    const char *victim = "uft_gate_victim.adf";

    /* Das "Abbild" auf die Platte legen — so, wie es die Oberflaeche
     * vorfindet. */
    {
        FILE *f = fopen(victim, "wb");
        if (!f) { printf("  Opfer nicht schreibbar\n"); free(img); return 2; }
        fwrite(img, 1, ADF_LEN, f);
        fclose(f);
    }

    /* -- Das Tor, genau wie die Oberflaeche es ruft -------------------- */
    uft_write_gate_policy_t pol = UFT_GATE_POLICY_IMAGE_ONLY;
    uft_write_gate_result_t r;
    memset(&r, 0, sizeof(r));

    uft_gate_status_t st = uft_write_gate_precheck(&pol, img, ADF_LEN,
                                                   ".", "uft_gate_snap", &r);
    printf("  Tor: Status %d (%s)\n", (int)st,
           r.decision_reason[0] ? r.decision_reason : "keine Begruendung");

    CHECK(st == UFT_GATE_OK, "das Tor laesst eine gueltige ADF durch");
    if (st != UFT_GATE_OK) {
        free(img);
        remove(victim);
        printf("\nFEHLGESCHLAGEN (%d Abweichungen)\n", ++failures);
        return 1;
    }
    CHECK(r.snapshot.path[0] != '\0', "es nennt einen Schnappschuss-Pfad");

    /* -- Jetzt das, was die Oberflaeche im schlimmsten Fall tut ---------
     *
     * `WriteOnly` kuerzt beim Oeffnen; danach bricht das Schreiben ab und
     * die Teildatei wird entfernt. Nach dieser Zeile gibt es die
     * Originaldatei nicht mehr. */
    remove(victim);
    {
        FILE *f = fopen(victim, "rb");
        CHECK(f == NULL, "das Abbild ist weg — der schlimmste Fall ist "
                         "eingetreten");
        if (f) fclose(f);
    }

    /* -- Die Zusage ---------------------------------------------------- */
    long snap_len = -1;
    {
        FILE *f = fopen(r.snapshot.path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            snap_len = ftell(f);
            fclose(f);
        }
    }
    printf("  Schnappschuss: %s (%ld Byte)\n", r.snapshot.path, snap_len);
    CHECK(snap_len == (long)ADF_LEN,
          "er ist vollstaendig — %ld von %d Byte", snap_len, ADF_LEN);

    /* Und er traegt DIESELBEN Bytes. Eine Sicherung, die etwas anderes
     * enthaelt als das Original, ist keine. */
    if (snap_len == (long)ADF_LEN) {
        uint8_t *back = (uint8_t *)malloc(ADF_LEN);
        FILE *f = fopen(r.snapshot.path, "rb");
        size_t got = (back && f) ? fread(back, 1, ADF_LEN, f) : 0;
        if (f) fclose(f);
        CHECK(got == ADF_LEN && memcmp(back, img, ADF_LEN) == 0,
              "byteweise gleich der Vorlage — nicht nur gleich gross");
        free(back);
    }

    /* Die Verifikation des Tors muss ihn ebenfalls anerkennen. */
    CHECK(uft_snapshot_verify(&r.snapshot) == UFT_SUCCESS,
          "und uft_snapshot_verify() bestaetigt ihn");

    remove(r.snapshot.path);
    remove(victim);
    free(img);

    printf("\n%s (%d Abweichungen)\n",
           failures ? "FEHLGESCHLAGEN" : "OK", failures);
    return failures ? 1 : 0;
}
