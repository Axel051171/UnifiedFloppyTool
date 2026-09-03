/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_gw_wire_conformance.c
 * @brief Der Produktionstreiber gegen den Firmware-Automaten (MF-848).
 *
 * ── Was hier zum ersten Mal geprueft wird ────────────────────────────
 *
 * `uft_gw_seek()` traegt seit MF-799 eine BEIDSEITIGE TRK0-Pruefung:
 *
 *     bool trk0 = !pegel;
 *     if (!uft_gw_trk0_stimmig((int)cylinder, trk0)) { ... }
 *
 * mit `uft_gw_trk0_stimmig(cyl, trk0) := (cyl == 0) == trk0`.
 *
 * Die zweite Richtung — /TRK0 liegt an, obwohl das Laufwerk woanders
 * stehen soll — faengt den **dekalibrierten Kopf**: das Laufwerk glaubt,
 * es steht auf Spur 40, steht aber auf 0, und liest achtzigmal dieselbe
 * Spur. Bis heute war diese Richtung nur an echter Hardware pruefbar,
 * und dieses Projekt hat keine (MF-310).
 *
 * ── Warum das jetzt geht ─────────────────────────────────────────────
 *
 * Zwei Haelften lagen im Baum, ohne einander zu kennen: der
 * Firmware-Automat (`tests/emulators/greaseweazle/`) und die
 * Byteebenen-Naht des Treibers (`uft_gw_open_stream()`, MF-686).
 * `gw_wire_bridge.c` verbindet sie.
 *
 * ── Zwei Haende, nicht eine ──────────────────────────────────────────
 *
 * Der bisher EINZIGE Nutzer der Naht (`tests/test_gw_nak_resync.c`)
 * speist eine von Hand geschriebene Bytefolge ein — dieselbe Hand, die
 * auch die Erwartung schreibt. Das ist die fuenfte Frage dieses Baums
 * (MF-644/760): ein Orakel, das sein Pruefobjekt teilt, bestaetigt den
 * gemeinsamen Irrtum. Der Automat ist eine andere Hand: eigener
 * Durchgang, eigenes Abweichungsregister.
 *
 * ── Grenze ───────────────────────────────────────────────────────────
 *
 * Geprueft wird gegen ein MODELL, nicht gegen ein Geraet. Die bekannten
 * Abweichungen stehen in `tests/emulators/greaseweazle/DIVERGENCES.md`
 * und bleiben Sache der Bench-Sitzung. Dieser Pruefstand ersetzt keinen
 * Bench-Termin — er verkleinert, was der noch fangen muss.
 */
#include "emulators/greaseweazle/gw_wire_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-46s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                       _fail++; return; } } while (0)

/**
 * Baut Automat + Bruecke + geoeffnetes Geraet.
 * `trk0` legt fest, ob /TRK0 anliegt.
 */
typedef struct {
    gw_fw_t          fw;
    gw_wire_t        draht;
    uft_gw_device_t *dev;
} stand_t;

static bool stand_auf(stand_t *s, bool trk0, uint8_t fw_major, uint8_t fw_minor)
{
    memset(s, 0, sizeof *s);
    gw_fw_reset(&s->fw);
    gw_fw_power_on_defaults(&s->fw);
    gw_fw_set_firmware_version(&s->fw, fw_major, fw_minor);
    gw_fw_set_trk0_present(&s->fw, trk0);

    gw_wire_init(&s->draht, &s->fw);
    return uft_gw_open_stream(&s->draht.ops, &s->dev) == UFT_GW_OK && s->dev;
}

static void stand_ab(stand_t *s)
{
    if (s->dev) uft_gw_close(s->dev);
    s->dev = NULL;
}

/** Motor an und Laufwerk gewaehlt — der Automat verlangt beides vor Seek. */
static bool betriebsbereit(stand_t *s)
{
    if (uft_gw_select_drive(s->dev, 0) != UFT_GW_OK) return false;
    return uft_gw_set_motor(s->dev, true) == UFT_GW_OK;
}

/* ------------------------------------------------------------------ */

TEST(die_bruecke_traegt_einen_echten_rahmen)
{
    /* Gegenprobe 0: ohne diese wuesste keiner der Faelle unten, ob
     * ueberhaupt ein Byte den Weg geht. Der Automat meldet eine
     * eingestellte Firmware-Version; sie muss unveraendert im
     * Treiber-Datensatz ankommen — ueber Rahmenkopf, ACK und 32 Byte
     * Nutzlast hinweg. */
    stand_t s;
    ASSERT(stand_auf(&s, true, 1, 23));

    uft_gw_info_t info;
    memset(&info, 0, sizeof info);
    ASSERT(uft_gw_get_info(s.dev, &info) == UFT_GW_OK);

    ASSERT(info.fw_major == 1);
    ASSERT(info.fw_minor == 23);
    ASSERT(info.is_main_fw != 0);
    ASSERT(s.draht.rahmen >= 1);
    ASSERT(s.draht.unbekannt == 0);   /* die Bruecke kannte jeden Befehl */

    stand_ab(&s);
}

TEST(dekalibrierter_kopf_wird_erkannt)
{
    /* DER FALL, DER BISHER NUR AN HARDWARE GING.
     *
     * /TRK0 liegt an, aber gesucht wurde Zylinder 40. Das Laufwerk
     * glaubt, es steht auf 40, und steht auf 0 — ohne diese Pruefung
     * laese UFT achtzigmal dieselbe Spur und meldete eine vollstaendige
     * Diskette. */
    stand_t s;
    ASSERT(stand_auf(&s, true /* TRK0 liegt an */, 1, 23));
    ASSERT(betriebsbereit(&s));

    int r = uft_gw_seek(s.dev, 40);
    if (r != UFT_GW_ERR_NO_TRK0) {
        printf("\n      seek(40) bei anliegendem /TRK0 gab %d, erwartet %d\n"
               "      -> der dekalibrierte Kopf faellt nicht auf\n      ",
               r, UFT_GW_ERR_NO_TRK0);
        _fail++;
    }
    stand_ab(&s);
}

TEST(gesunder_seek_bleibt_gruen)
{
    /* Gegenprobe 1: der Fall darueber darf nicht einfach IMMER
     * fehlschlagen. Gleiche Spur, /TRK0 liegt NICHT an — das ist der
     * Normalfall und muss durchgehen. */
    stand_t s;
    ASSERT(stand_auf(&s, false, 1, 23));
    ASSERT(betriebsbereit(&s));

    ASSERT(uft_gw_seek(s.dev, 40) == UFT_GW_OK);
    stand_ab(&s);
}

TEST(spur_null_mit_trk0_geht_durch)
{
    /* Gegenprobe 2: die ERSTE Richtung der Pruefung. Auf Zylinder 0 MUSS
     * /TRK0 anliegen; tut es das, ist alles in Ordnung. Ohne diesen Fall
     * waere eine Fassung gruen, die schlicht jedes anliegende /TRK0
     * ablehnt. */
    stand_t s;
    ASSERT(stand_auf(&s, true, 1, 23));
    ASSERT(betriebsbereit(&s));

    ASSERT(uft_gw_seek(s.dev, 0) == UFT_GW_OK);
    stand_ab(&s);
}

TEST(der_automat_sieht_was_der_treiber_tat)
{
    /* Gegenprobe 3: die Bruecke darf nicht bloss plausible Bytes
     * zurueckgeben — der Automat muss den Befehl WIRKLICH ausgefuehrt
     * haben. Nach `uft_gw_seek(0)` steht sein Zylinderzaehler auf 0,
     * nach `seek(40)` auf 40. Eine Bruecke, die nur ACKs erfindet,
     * faellt hier auf. */
    stand_t s;
    ASSERT(stand_auf(&s, false, 1, 23));
    ASSERT(betriebsbereit(&s));

    ASSERT(uft_gw_seek(s.dev, 40) == UFT_GW_OK);
    ASSERT(s.fw.current_cyl == 40);

    ASSERT(uft_gw_select_head(s.dev, 1) == UFT_GW_OK);
    ASSERT(s.fw.current_head == 1);

    stand_ab(&s);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Greaseweazle: Treiber gegen Firmware-Automat (MF-848) ===\n");
    RUN(die_bruecke_traegt_einen_echten_rahmen);
    RUN(dekalibrierter_kopf_wird_erkannt);
    RUN(gesunder_seek_bleibt_gruen);
    RUN(spur_null_mit_trk0_geht_durch);
    RUN(der_automat_sieht_was_der_treiber_tat);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
