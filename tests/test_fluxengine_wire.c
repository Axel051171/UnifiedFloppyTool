/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_fluxengine_wire.c
 * @brief Die FluxEngine-Rahmenschicht gegen den Drahtautomaten (MF-857).
 *
 * ── Was hier geprueft wird ───────────────────────────────────────────
 *
 * `src/hal/uft_fluxengine.c` schreibt Kommandorahmen und liest
 * Antwortrahmen. `tests/emulators/fluxengine/wire_state_machine.c`
 * beantwortet sie wie die Firmware. Verbunden ueber die Byteebenen-Naht
 * `uft_fe_stream_ops_t`.
 *
 * Beide sind gegen dieselbe Quelle geschrieben
 * (`davidgiven/fluxengine`, Commit `909fac72`, GPL-2.0-only), aber gegen
 * VERSCHIEDENE Dateien: die Rahmenschicht gegen
 * `lib/usb/fluxengineusb.cc`, der Automat gegen
 * `FluxEngine.cydsn/main.c`. Das ist nicht dasselbe wie zwei
 * unabhaengige Quellen — die Zwei-Quellen-Regel ist NICHT erfuellt
 * (Gutachten §7: keine unabhaengige Umsetzung des Board-Protokolls
 * gefunden) —, aber es sind zwei Blickrichtungen statt einer
 * Handfolge, die ihre eigene Erwartung bestaetigt (die fuenfte Frage,
 * MF-644/760).
 *
 * ── Grenze ───────────────────────────────────────────────────────────
 *
 * Geprueft wird gegen ein MODELL, nicht gegen ein Geraet. Dieses
 * Projekt hat keins (MF-310). Die bekannten Abweichungen stehen in
 * `tests/emulators/fluxengine/WIRE_DIVERGENCES.md` und bleiben Sache
 * einer Bench-Sitzung.
 */
#include "emulators/fluxengine/wire_state_machine.h"
#include "uft/hal/uft_fluxengine.h"

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

/* ─── Die Bruecke: Automat hinter der Naht ──────────────────────────── */

/*
 * Antworten entstehen beim LESEN, nicht beim Schreiben.
 *
 * Erste Fassung erzeugte die Antwort in `write` und legte sie ab. Das
 * ist kein Geraet: ein Geraet kann auf EIN Kommando MEHRERE Rahmen
 * senden — die Debug-Rahmen sind genau dieser Fall, und der Treiber
 * liest so lange, bis der erwartete Typ kommt. Zwei Faelle sind an der
 * ersten Fassung gescheitert, und beide Male lag es an der
 * VORRICHTUNG, nicht am Treiber.
 */
typedef struct {
    uft_fe_stream_ops_t ops;      /* zuerst: der Zeiger bleibt stabil */
    fe_wire_t          *fw;

    uint8_t             letzter[UFT_FE_FRAME_SIZE];
    size_t              letzter_len;
    bool                offen;    /* ein Kommando wartet auf Antwort */

    unsigned            rahmen;
    bool                stumm;    /* Geraet antwortet gar nicht      */
    size_t              kuerze_auf; /* >0: Antwort so weit abschneiden */
} draht_t;

static int op_write(void *user, const uint8_t *data, size_t len)
{
    draht_t *d = (draht_t *)user;
    if (len > sizeof d->letzter) return UFT_FE_ERR_INVALID;
    memcpy(d->letzter, data, len);
    d->letzter_len = len;
    d->offen = true;
    d->rahmen++;
    return UFT_FE_OK;
}

static int op_read(void *user, uint8_t *data, size_t max, size_t *actual,
                   int timeout_ms)
{
    (void)timeout_ms;
    draht_t *d = (draht_t *)user;
    if (d->stumm || !d->offen) return UFT_FE_ERR_TIMEOUT;

    uint8_t roh[UFT_FE_FRAME_SIZE];
    size_t n = 0;
    if (!fe_wire_handle(d->fw, d->letzter, d->letzter_len, roh, &n) || n == 0)
        return UFT_FE_ERR_TIMEOUT;      /* das Geraet schweigt */

    /* Solange Debug-Rahmen ausstehen, bleibt das Kommando offen — der
     * Automat liefert danach die eigentliche Antwort. */
    if (roh[0] != UFT_FE_F_DEBUG) d->offen = false;

    if (d->kuerze_auf > 0 && d->kuerze_auf < n) n = d->kuerze_auf;
    if (n > max) n = max;
    memcpy(data, roh, n);
    if (actual) *actual = n;
    return UFT_FE_OK;
}

typedef struct {
    fe_wire_t        fw;
    draht_t          draht;
    uft_fe_device_t *dev;
} stand_t;

static bool stand_auf(stand_t *s)
{
    memset(s, 0, sizeof *s);
    fe_wire_reset(&s->fw);
    s->draht.fw        = &s->fw;
    s->draht.ops.write = op_write;
    s->draht.ops.read  = op_read;
    s->draht.ops.user  = &s->draht;
    return uft_fe_open_stream(&s->draht.ops, &s->dev) == UFT_FE_OK && s->dev;
}

static void stand_ab(stand_t *s) { uft_fe_close(s->dev); s->dev = NULL; }

/* ------------------------------------------------------------------ */

TEST(fassung_kommt_unveraendert_an)
{
    /* Gegenprobe 0: ohne diese wuesste keiner der Faelle unten, ob
     * ueberhaupt ein Byte den Weg geht. */
    stand_t s;
    ASSERT(stand_auf(&s));

    uint8_t v = 0;
    ASSERT(uft_fe_get_version(s.dev, &v) == UFT_FE_OK);
    ASSERT(v == UFT_FE_PROTOCOL_VERSION);
    ASSERT(v == 17);                      /* protocol.h:6, Commit 909fac72 */
    ASSERT(s.draht.rahmen == 1);
    ASSERT(s.fw.unknown_cmds == 0);       /* der Automat kannte den Rahmen */
    stand_ab(&s);
}

TEST(fremde_fassung_faellt_auf)
{
    /* Die Bewertung ist eine EIGENE Zusage, kein Zweig im
     * Verbindungsaufbau (Lehre aus MF-849): so ist sie ohne Geraet
     * pruefbar. Die Vorlage bricht bei Abweichung hart ab
     * (fluxengineusb.cc:73-79), ohne Aushandlung. */
    stand_t s;
    ASSERT(stand_auf(&s));
    s.fw.version = 16;

    uint8_t v = 0;
    ASSERT(uft_fe_get_version(s.dev, &v) == UFT_FE_OK);
    ASSERT(v == 16);
    ASSERT(!uft_fe_version_supported(v));
    ASSERT(uft_fe_version_supported(17));
    stand_ab(&s);
}

TEST(der_automat_sieht_was_der_treiber_tat)
{
    /* Die Bruecke darf nicht bloss plausible Rahmen zurueckgeben — der
     * Automat muss den Befehl WIRKLICH ausgefuehrt haben. Eine Fassung,
     * die nur Antworttypen erfindet, faellt hier auf. */
    stand_t s;
    ASSERT(stand_auf(&s));

    ASSERT(uft_fe_recalibrate(s.dev) == UFT_FE_OK);
    ASSERT(s.fw.current_track == 0);

    ASSERT(uft_fe_seek(s.dev, 40) == UFT_FE_OK);
    ASSERT(s.fw.current_track == 40);

    ASSERT(uft_fe_set_drive(s.dev, 1, true, 2) == UFT_FE_OK);
    ASSERT(s.fw.drive == 1);
    ASSERT(s.fw.high_density == true);
    ASSERT(s.fw.index_mode == 2);
    stand_ab(&s);
}

TEST(die_umdrehungsdauer_wird_klein_endig_gelesen)
{
    /* `speed_frame.period_ms` liegt als uint16 LE bei Versatz 2.
     *
     * Der Original-Client liest den Wert an DIESER Stelle ROH
     * (fluxengineusb.cc:164) — obwohl er `bytes_to_write` und die
     * Spannungswerte ausdruecklich wandelt („the board always operates
     * in little-endian mode"). Auf einem Little-Endian-Host folgenlos,
     * auf einem Big-Endian-Host falsch.
     *
     * 0x0141 = 321 ist bewusst gewaehlt: bei vertauschten Bytes kaeme
     * 0x4101 = 16641 heraus, also keine plausible Umdrehungsdauer —
     * ein Wert wie 200 (0x00C8) wuerde den Fehler verdecken, weil das
     * hohe Byte null ist. */
    stand_t s;
    ASSERT(stand_auf(&s));
    s.fw.period_ms = 0x0141;

    uint16_t p = 0;
    ASSERT(uft_fe_measure_speed(s.dev, 0, &p) == UFT_FE_OK);
    if (p != 0x0141) {
        printf("\n      period_ms = %u, erwartet %u\n"
               "      -> Byteordnung vertauscht\n      ", p, 0x0141u);
        _fail++;
    }
    stand_ab(&s);
}

TEST(ohne_diskette_kein_erfundener_messwert)
{
    /* Ein Geraet ohne Medium hat keinen Indexpuls. Es darf KEINE
     * Umdrehungsdauer melden — auch keine null. Der Unterschied
     * zwischen „nicht gemessen" und „gemessen: 0" ist genau der, gegen
     * den dieser Baum geschrieben ist. */
    stand_t s;
    ASSERT(stand_auf(&s));
    s.fw.disk_present = false;

    uint16_t p = 0xFFFF;
    int r = uft_fe_measure_speed(s.dev, 0, &p);
    ASSERT(r == UFT_FE_ERR_DEVICE);
    ASSERT(p == 0xFFFF);                  /* unberuehrt gelassen */
    stand_ab(&s);
}

TEST(unbekannter_rahmen_wird_gezaehlt)
{
    /* Gegenprobe: der Automat darf eine Luecke nicht verschweigen.
     * Der Treiber schickt READ (noch nicht umgesetzt) — der Automat
     * kennt ihn nicht, meldet einen Fehlerrahmen UND zaehlt ihn. */
    stand_t s;
    ASSERT(stand_auf(&s));

    uint8_t f[UFT_FE_SZ_READ];
    memset(f, 0, sizeof f);
    f[0] = UFT_FE_F_READ_CMD;
    f[1] = UFT_FE_SZ_READ;
    ASSERT(op_write(&s.draht, f, sizeof f) == UFT_FE_OK);

    /* Erst das LESEN befragt den Automaten — genau wie an einer
     * Leitung. Er antwortet mit einem Fehlerrahmen und zaehlt den
     * unbekannten Typ. */
    uint8_t a[UFT_FE_FRAME_SIZE];
    size_t da = 0;
    ASSERT(op_read(&s.draht, a, sizeof a, &da, 1000) == UFT_FE_OK);
    ASSERT(da == UFT_FE_SZ_ANY);
    ASSERT(a[0] == UFT_FE_F_ERROR);
    ASSERT(s.fw.unknown_cmds == 1);
    stand_ab(&s);
}

TEST(debug_rahmen_werden_ueberlesen)
{
    /* Die heutige Firmware sendet nie `F_FRAME_DEBUG` (`print()` geht
     * auf UART, main.c:127-138) — der Client behandelt sie trotzdem,
     * und Altbestand, den niemand prueft, ist der Zustand, aus dem
     * P3-89 kam. Drei Debug-Rahmen vor der Antwort: das Ergebnis muss
     * unveraendert durchkommen. */
    stand_t s;
    ASSERT(stand_auf(&s));
    s.fw.debug_frames_pending = 3;

    uint8_t v = 0;
    ASSERT(uft_fe_get_version(s.dev, &v) == UFT_FE_OK);
    ASSERT(v == UFT_FE_PROTOCOL_VERSION);
    stand_ab(&s);
}

TEST(ein_stummes_geraet_haengt_nicht)
{
    /* DER UNTERSCHIED ZUR VORLAGE.
     *
     * Der Original-Client hat KEIN Zeitlimit — kein `set_timeout` im
     * FluxEngine-Pfad, libusbp wartet unbegrenzt; bei einer READ-Antwort
     * ueber 1 MiB droht laut Quelltext eine wechselseitige Blockade.
     *
     * Ein forensisches Werkzeug darf nicht haengen: hier kommt ein
     * Zeitlimit zurueck, kein Stillstand. */
    stand_t s;
    ASSERT(stand_auf(&s));
    s.draht.stumm = true;

    ASSERT(uft_fe_get_version(s.dev, NULL) == UFT_FE_ERR_TIMEOUT);
    stand_ab(&s);
}

TEST(abgeschnittene_antwort_gilt_nicht_als_gueltig)
{
    /* Die Firmware prueft weder Laenge noch `size`-Feld
     * (`handle_command`, main.c:820-875). Wir schon: eine Antwort, die
     * kuerzer ist als der Rahmen, den sie ankuendigt, ist keine
     * Antwort. Ohne diese Pruefung laese `uft_fe_get_version()` seine
     * Fassung aus einem Byte, das nie gesendet wurde. */
    stand_t s;
    ASSERT(stand_auf(&s));

    /* Die Leitung schneidet die Antwort auf zwei Byte ab: Typ und
     * Laengenfeld kommen an, das Fassungsbyte nicht. */
    s.draht.kuerze_auf = 2;

    uint8_t v = 0xAA;
    ASSERT(uft_fe_get_version(s.dev, &v) == UFT_FE_ERR_PROTOCOL);
    ASSERT(v == 0xAA);                    /* unberuehrt */
    stand_ab(&s);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== FluxEngine: Rahmenschicht gegen Drahtautomat (MF-857) ===\n");
    RUN(fassung_kommt_unveraendert_an);
    RUN(fremde_fassung_faellt_auf);
    RUN(der_automat_sieht_was_der_treiber_tat);
    RUN(die_umdrehungsdauer_wird_klein_endig_gelesen);
    RUN(ohne_diskette_kein_erfundener_messwert);
    RUN(unbekannter_rahmen_wird_gezaehlt);
    RUN(debug_rahmen_werden_ueberlesen);
    RUN(ein_stummes_geraet_haengt_nicht);
    RUN(abgeschnittene_antwort_gilt_nicht_als_gueltig);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
