/**
 * @file test_gw_nak_resync.c
 * @brief Was tut unser Greaseweazle-Pfad nach einem NAK? (MF-686)
 *
 * ── Dies ist ein EXPERIMENT, kein Beweis einer Behauptung ────────────────
 *
 * gwnbd (EUPL-1.2, MF-680) berichtet, dass Greaseweazle-Firmware nach
 * einem NAK Bytes im Strom lassen kann, und hat dafür ein `resync()`.
 * Das ist **eine** Quelle. Nach der Zwei-Quellen-Regel reicht das nicht,
 * um daraus einen Fix abzuleiten — und ohne Hardware (MF-310) können wir
 * die Firmware nicht selbst befragen.
 *
 * Was wir sehr wohl können: messen, **was unser Pfad täte**, wenn es so
 * wäre. Dieses Ergebnis ist die fehlende zweite Hälfte — in welche
 * Richtung auch immer:
 *
 *   * Erholt sich der Treiber, ist gwnbds Sorge für uns gegenstandslos.
 *   * Erholt er sich nicht, ist der Fehler unabhängig von gwnbds
 *     Firmware-Beobachtung belegt: er liegt dann in UNSEREM Code, und
 *     zwar für jeden Fall, in dem irgendetwas Bytes im Strom lässt —
 *     ein flackerndes Kabel genügt.
 *
 * Der zweite Punkt ist der Grund, warum dieses Experiment die
 * Zwei-Quellen-Regel nicht verbiegt: es prüft nicht, ob die Firmware
 * sich so verhält, sondern ob unser Treiber es überlebt.
 *
 * ── Wie eingespeist wird ─────────────────────────────────────────────────
 *
 * Über die Byteebenen-Naht aus MF-686. Der Emulator kann das nicht: er
 * modelliert die Zustandsmaschine als Funktionsaufrufe, es gibt dort
 * keinen Bytestrom (`DIVERGENCES.md` §D-2, „Detection: none").
 *
 * Die Attrappe ist eine **Warteschlange**, kein Antwortgeber. Das ist
 * der Punkt: ein echter serieller Puffer vergisst nichts. Was der
 * Treiber nicht abholt, liegt beim nächsten Lesen noch da — und genau
 * das soll gemessen werden.
 */

#include "uft/hal/uft_greaseweazle_full.h"

#include <stdio.h>
#include <string.h>

static int fehler;

#define PRUEFE(bed, ...)                                                   \
    do { if (!(bed)) { printf("  FAIL "); printf(__VA_ARGS__);             \
                       printf("\n"); fehler++; } } while (0)

/* ── Die Leitung: eine Warteschlange, die nichts vergisst ──────────── */

#define PUFFER 512

typedef struct {
    uint8_t  aus[PUFFER];     /* was das "Geraet" zu senden hat */
    size_t   aus_len, aus_pos;
    uint8_t  ein[PUFFER];     /* was der Treiber geschrieben hat */
    size_t   ein_len;
    int      schreib_aufrufe;
} leitung_t;

static void leitung_sendet(leitung_t *l, const uint8_t *d, size_t n)
{
    if (l->aus_len + n > PUFFER) return;
    memcpy(l->aus + l->aus_len, d, n);
    l->aus_len += n;
}

static int op_write(void *user, const uint8_t *data, size_t len)
{
    leitung_t *l = (leitung_t *)user;
    l->schreib_aufrufe++;
    if (l->ein_len + len <= PUFFER) {
        memcpy(l->ein + l->ein_len, data, len);
        l->ein_len += len;
    }
    return UFT_GW_OK;
}

static int op_read_exact(void *user, uint8_t *data, size_t len, int timeout_ms)
{
    (void)timeout_ms;
    leitung_t *l = (leitung_t *)user;
    if (l->aus_pos + len > l->aus_len) return UFT_GW_ERR_TIMEOUT;
    memcpy(data, l->aus + l->aus_pos, len);
    l->aus_pos += len;
    return UFT_GW_OK;
}

static int op_read_available(void *user, uint8_t *data, size_t max_len,
                             size_t *actual, int timeout_ms)
{
    (void)timeout_ms;
    leitung_t *l = (leitung_t *)user;
    size_t da = l->aus_len - l->aus_pos;
    if (da > max_len) da = max_len;
    memcpy(data, l->aus + l->aus_pos, da);
    l->aus_pos += da;
    if (actual) *actual = da;
    return UFT_GW_OK;
}

static const uft_gw_stream_ops_t OPS = {
    .write = op_write,
    .read_exact = op_read_exact,
    .read_available = op_read_available,
    .user = NULL,
};

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Greaseweazle: was passiert nach einem NAK? (MF-686)\n\n");

    leitung_t l;
    memset(&l, 0, sizeof(l));

    uft_gw_stream_ops_t ops = OPS;
    ops.user = &l;

    uft_gw_device_t *dev = NULL;
    if (uft_gw_open_stream(&ops, &dev) != UFT_GW_OK || !dev) {
        printf("  FAIL Geraet mit eingespeister Leitung nicht zu oeffnen\n");
        return 1;
    }

    /* ── Erster Befehl: das Geraet antwortet mit NAK und laesst Muell ──
     *
     * Die drei Muellbytes stehen fuer alles, was ein Geraet nach einer
     * Ablehnung noch heraussenden kann — eine angefangene Nutzlast, ein
     * Rest aus dem vorigen Befehl, Stoerung auf der Leitung. Ihr Inhalt
     * ist gleichgueltig; entscheidend ist, dass sie DA sind. */
    const uint8_t nak_antwort[] = {
        UFT_GW_CMD_SEEK, UFT_GW_ACK_NO_TRK0,   /* Echo + Ablehnung */
        0xDE, 0xAD, 0xBE                        /* und Reste im Puffer */
    };
    leitung_sendet(&l, nak_antwort, sizeof(nak_antwort));

    int rc1 = uft_gw_seek(dev, 0);
    printf("  Befehl 1 (NAK erwartet): rc=%d\n", rc1);
    PRUEFE(rc1 != UFT_GW_OK,
           "ein NAK muss als Fehler durchschlagen, rc war %d", rc1);

    size_t rest = l.aus_len - l.aus_pos;
    printf("  danach ungelesen im Puffer: %zu Byte\n", rest);

    /* ── Zweiter Befehl: das Geraet antwortet sauber ───────────────────
     *
     * Hier entscheidet sich alles. Hat der Treiber die drei Reste
     * liegengelassen, liest er sie jetzt als Kopf des NEUEN Befehls —
     * 0xDE statt des Echos — und meldet einen Protokollfehler, obwohl
     * das Geraet korrekt geantwortet hat. */
    const uint8_t gute_antwort[] = { UFT_GW_CMD_SEEK, UFT_GW_ACK_OK };
    leitung_sendet(&l, gute_antwort, sizeof(gute_antwort));

    int rc2 = uft_gw_seek(dev, 0);
    printf("  Befehl 2 (sauber): rc=%d\n", rc2);

    /* Das ist die Messung. Sie ist absichtlich als Pruefung formuliert:
     * ist sie rot, haben wir den Befund, den gwnbd vermutet hat — und
     * zwar an unserem Code, unabhaengig von deren Firmware. */
    PRUEFE(rc2 == UFT_GW_OK,
           "der Folgebefehl scheitert (rc=%d), obwohl das Geraet korrekt "
           "geantwortet hat. Der Treiber hat die %zu Restbytes nach dem "
           "NAK nicht abgeraeumt und liest sie als Kopf des naechsten "
           "Befehls. gwnbd hat dafuer ein resync(); wir haben keines",
           rc2, rest);

    if (rc2 == UFT_GW_OK)
        printf("  ok   der Treiber erholt sich vom NAK\n");

    uft_gw_close(dev);

    printf("\n%s (%d Abweichungen)\n",
           fehler ? "FEHLGESCHLAGEN" : "OK", fehler);
    return fehler ? 1 : 0;
}
