/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file tests/emulators/greaseweazle/gw_wire_bridge.c
 * @brief Rueckweg des GW-Drahtprotokolls: Rahmen rein, Antwort raus (MF-848).
 *
 * Rahmenformat, aus `uft_gw_command()` gelesen (nicht geraten):
 *
 *   Hin:  [ befehl | gesamtlaenge (= 2 + params) | params... ]
 *   Zurueck: [ befehl (Echo) | ack ]  danach ggf. Nutzlast
 *
 * `gw_fw_build_packet()` im Automaten baut den HINweg in genau dieser
 * Form. Diese Datei ist die Gegenrichtung.
 */
#include "gw_wire_bridge.h"

#include <string.h>

/* ── Warteschlange Richtung Treiber ──────────────────────────────── */

static void sendet(gw_wire_t *w, const uint8_t *d, size_t n)
{
    if (w->aus_len + n > GW_WIRE_PUFFER) return;   /* Ueberlauf: Stille,
                                                    * der Treiber laeuft
                                                    * dann in seinen
                                                    * Zeitablauf — genau
                                                    * wie an echter
                                                    * Leitung. */
    memcpy(w->aus + w->aus_len, d, n);
    w->aus_len += n;
}

static void antwort(gw_wire_t *w, uint8_t befehl, gw_fw_ack_t ack)
{
    uint8_t kopf[2] = { befehl, (uint8_t)ack };
    sendet(w, kopf, 2);
}

/* ── Ein vollstaendiger Rahmen ───────────────────────────────────── */

static void verteile(gw_wire_t *w, const uint8_t *f, size_t len)
{
    const uint8_t befehl = f[0];
    gw_fw_ack_t ack;

    w->rahmen++;
    w->letzter_befehl = befehl;

    /* Ein Parameterbyte an Stelle i, oder 0 wenn der Rahmen kuerzer ist.
     * Ein zu kurzer Rahmen ist ein Treiberfehler und soll als solcher
     * auffallen — nicht als Lesen hinter dem Puffer. */
    #define P(i) ((len > (size_t)(2 + (i))) ? f[2 + (i)] : 0u)

    switch (befehl) {

    case UFT_GW_CMD_GET_INFO: {
        uint8_t nutz[32];
        ack = gw_fw_cmd_get_info(w->fw, P(0), nutz, sizeof nutz);
        antwort(w, befehl, ack);
        if (ack == GW_FW_ACK_OK) sendet(w, nutz, sizeof nutz);
        return;
    }

    case UFT_GW_CMD_GET_PIN: {
        uint8_t pegel = 0;
        ack = gw_fw_cmd_get_pin(w->fw, P(0), &pegel);
        antwort(w, befehl, ack);
        if (ack == GW_FW_ACK_OK) sendet(w, &pegel, 1);
        return;
    }

    case UFT_GW_CMD_SEEK:
        /* Der Treiber schickt den Zylinder VORZEICHENBEHAFTET
         * (`int8_t cyl8`), der Automat nimmt `int8_t`. */
        ack = gw_fw_cmd_seek(w->fw, (int8_t)P(0));
        break;

    case UFT_GW_CMD_HEAD:
        ack = gw_fw_cmd_head(w->fw, P(0));
        break;

    case UFT_GW_CMD_MOTOR:
        ack = gw_fw_cmd_motor(w->fw, P(0), P(1) != 0);
        break;

    case UFT_GW_CMD_SELECT:
        ack = gw_fw_cmd_select(w->fw, P(0));
        break;

    case UFT_GW_CMD_DESELECT:
        ack = gw_fw_cmd_deselect(w->fw);
        break;

    case UFT_GW_CMD_SET_BUS_TYPE:
        ack = gw_fw_cmd_set_bus_type(w->fw, (gw_fw_bus_type_t)P(0));
        break;

    case UFT_GW_CMD_SET_PIN:
        ack = gw_fw_cmd_set_pin(w->fw, P(0), P(1));
        break;

    case UFT_GW_CMD_RESET:
        ack = gw_fw_cmd_reset(w->fw);
        break;

    case UFT_GW_CMD_NO_CLICK_STEP:
        ack = gw_fw_cmd_no_click_step(w->fw);
        break;

    case UFT_GW_CMD_GET_FLUX_STATUS:
        ack = gw_fw_cmd_get_flux_status(w->fw);
        break;

    default:
        /* Kein stilles OK. Ein Befehl, den die Bruecke nicht kennt,
         * bekommt die Antwort, die echte Firmware gaebe — und wird
         * gezaehlt, damit ein Test die Luecke SEHEN kann statt sie zu
         * erben. Das ist der Unterschied zu einer Handfolge: die
         * schweigt ueber alles, wonach nicht gefragt wurde. */
        w->unbekannt++;
        ack = GW_FW_ACK_BAD_COMMAND;
        break;
    }
    #undef P

    antwort(w, befehl, ack);
}

/* ── Die drei Naht-Funktionen ────────────────────────────────────── */

static int op_write(void *user, const uint8_t *data, size_t len)
{
    gw_wire_t *w = (gw_wire_t *)user;
    if (!w || !data) return UFT_GW_ERR_INVALID;

    if (w->ein_len + len > GW_WIRE_PUFFER) return UFT_GW_ERR_OVERFLOW;
    memcpy(w->ein + w->ein_len, data, len);
    w->ein_len += len;

    /* Solange ein GANZER Rahmen dasteht, verarbeiten. Der Treiber darf
     * stueckweise schreiben; echte Leitungen tun das auch. */
    for (;;) {
        if (w->ein_len < 2) break;
        size_t rl = w->ein[1];
        if (rl < 2) {                  /* unmoegliche Laenge */
            w->ein_len = 0;
            return UFT_GW_ERR_PROTOCOL;
        }
        if (w->ein_len < rl) break;    /* noch unvollstaendig */

        verteile(w, w->ein, rl);

        memmove(w->ein, w->ein + rl, w->ein_len - rl);
        w->ein_len -= rl;
    }
    return UFT_GW_OK;
}

static int op_read_exact(void *user, uint8_t *data, size_t len, int timeout_ms)
{
    (void)timeout_ms;
    gw_wire_t *w = (gw_wire_t *)user;
    if (!w || !data) return UFT_GW_ERR_INVALID;
    if (w->aus_pos + len > w->aus_len) return UFT_GW_ERR_TIMEOUT;
    memcpy(data, w->aus + w->aus_pos, len);
    w->aus_pos += len;
    return UFT_GW_OK;
}

static int op_read_available(void *user, uint8_t *data, size_t max_len,
                             size_t *actual, int timeout_ms)
{
    (void)timeout_ms;
    gw_wire_t *w = (gw_wire_t *)user;
    if (!w || !data) return UFT_GW_ERR_INVALID;
    size_t da = w->aus_len - w->aus_pos;
    if (da > max_len) da = max_len;
    memcpy(data, w->aus + w->aus_pos, da);
    w->aus_pos += da;
    if (actual) *actual = da;
    return UFT_GW_OK;
}

void gw_wire_init(gw_wire_t *w, gw_fw_t *fw)
{
    if (!w) return;
    memset(w, 0, sizeof *w);
    w->fw = fw;
    w->ops.write          = op_write;
    w->ops.read_exact     = op_read_exact;
    w->ops.read_available = op_read_available;
    w->ops.user           = w;
}
