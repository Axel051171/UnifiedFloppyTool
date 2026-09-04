/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_fluxengine.h
 * @brief FluxEngine-Drahtprotokoll: Rahmenschicht und Byteebenen-Naht (MF-857).
 *
 * ── Herkunft ─────────────────────────────────────────────────────────
 *
 * Verhalten nach `davidgiven/fluxengine`, Commit `909fac72`, Dateien
 * `protocol.h` und `lib/usb/fluxengineusb.cc` — **eigenstaendige
 * Umsetzung, kein Port**. Die Quelle steht unter GPL-2.0-only
 * (`COPYING.md`: „not GPL 2.0-or-later"); dieser Baum steht unter
 * GPL-2.0-or-later, die Kombination ist damit zulaessig. Gutachten:
 * `tools/uft-scout/out/fluxengine.gutachten.md` (MF-856).
 *
 * ── Warum die Naht ZUERST kommt ──────────────────────────────────────
 *
 * Dieses Projekt hat kein Geraet (MF-310). Treibercode, den niemand
 * gegen Silizium halten kann, ist die Bauform der fuenf fabrizierten
 * Parser (FMT-2/3/10/11/12): gegen eine gelesene Beschreibung gebaut,
 * gruen, falsch.
 *
 * MF-848 hat gezeigt, was stattdessen traegt: eine Byteebenen-Naht im
 * Treiber (`uft_gw_stream_ops_t`) und ein Firmware-Automat dahinter.
 * Deshalb traegt diese Schicht die Naht von der ersten Zeile an — nicht
 * als Nachruestung, sondern als Voraussetzung.
 *
 * **Der Einhaengepunkt ist NICHT der bestehende `FluxEngineRunner`.**
 * Der ist eine argv-Naht (`fluxengine_provider_v2.h:200-202`); ein
 * USB-Runner dahinter muesste Kommandozeilen PARSEN und erbte das
 * Dialektrisiko aus `DIVERGENCES.md` FE-3. Gemessen und begruendet im
 * Gutachten §8.
 *
 * ── Drei Stellen, an denen wir es anders machen als die Vorlage ──────
 *
 * **(1) Ausdrueckliche Byteversaetze statt Struct-Kopien.** Der
 * Original-Client legt seine Structs per `memcpy` auf die Leitung und
 * verlaesst sich darauf, dass Host- und PSoC-Compiler identisch
 * ausrichten. Fuellbytes gehen dabei mit ueber die Leitung. Das
 * funktioniert, ist aber nicht PRUEFBAR: die ARM-Seite wurde nie
 * gemessen (Gutachten, UNGEKLAERT #1). Hier steht jeder Versatz als
 * Zahl, und ein Test haelt ihn fest.
 *
 * **(2) Ein Zeitlimit.** Der Original-Client hat keins — kein
 * `set_timeout` im FluxEngine-Pfad, libusbp wartet unbegrenzt. Bei
 * einer READ-Antwort ueber 1 MiB droht laut Quelltext eine
 * wechselseitige Blockade. Ein forensisches Werkzeug darf nicht
 * haengen.
 *
 * **(3) Laengenpruefung beim Empfang.** Die Firmware prueft weder
 * Laenge noch `size`-Feld (`handle_command`, `main.c:820-875`: liest
 * bis zu 64 Byte, verzweigt nur ueber `type`). Wir pruefen, weil ein
 * verkuerzter Rahmen sonst als gueltig durchginge.
 *
 * ── Grenze, ausdruecklich ────────────────────────────────────────────
 *
 * **Eine Quelle, keine zweite.** Der Scout hat zweifach gesucht und
 * keine unabhaengige Umsetzung des Board-Protokolls gefunden
 * (Gutachten §7). Alles hier ruht auf `davidgiven/fluxengine` selbst.
 * Fuer ein Protokoll ist das die definierende Hand — Firmware und
 * Client uebersetzen denselben Header —, aber es ist nicht dasselbe wie
 * zwei unabhaengige Zeugen, und die Zwei-Quellen-Regel ist NICHT
 * erfuellt.
 */
#ifndef UFT_HAL_UFT_FLUXENGINE_H
#define UFT_HAL_UFT_FLUXENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Konstanten (protocol.h:4-38, Commit 909fac72) ─────────────────── */

#define UFT_FE_PROTOCOL_VERSION   17        /**< seit 2022-03-26 unveraendert */
#define UFT_FE_VID                0x1209
#define UFT_FE_PID                0x6e00

#define UFT_FE_EP_DATA_OUT        0x01
#define UFT_FE_EP_DATA_IN         0x82
#define UFT_FE_EP_CMD_OUT         0x03
#define UFT_FE_EP_CMD_IN          0x84

#define UFT_FE_FRAME_SIZE         64        /**< der Client liest IMMER 64 */
#define UFT_FE_TICK_FREQUENCY     12000000u /**< 12 MHz Abtasttakt          */

/* ─── Rahmentypen (protocol.h:44-70) ────────────────────────────────── */
typedef enum {
    UFT_FE_F_ERROR                  = 0,
    UFT_FE_F_DEBUG                  = 1,
    UFT_FE_F_GET_VERSION_CMD        = 2,
    UFT_FE_F_GET_VERSION_REPLY      = 3,
    UFT_FE_F_SEEK_CMD               = 4,
    UFT_FE_F_SEEK_REPLY             = 5,
    UFT_FE_F_MEASURE_SPEED_CMD      = 6,
    UFT_FE_F_MEASURE_SPEED_REPLY    = 7,
    UFT_FE_F_BULK_WRITE_TEST_CMD    = 8,
    UFT_FE_F_BULK_WRITE_TEST_REPLY  = 9,
    UFT_FE_F_BULK_READ_TEST_CMD     = 10,
    UFT_FE_F_BULK_READ_TEST_REPLY   = 11,
    UFT_FE_F_READ_CMD               = 12,
    UFT_FE_F_READ_REPLY             = 13,
    UFT_FE_F_WRITE_CMD              = 14,
    UFT_FE_F_WRITE_REPLY            = 15,
    UFT_FE_F_ERASE_CMD              = 16,
    UFT_FE_F_ERASE_REPLY            = 17,
    UFT_FE_F_RECALIBRATE_CMD        = 18,
    UFT_FE_F_RECALIBRATE_REPLY      = 19,
    UFT_FE_F_SET_DRIVE_CMD          = 20,
    UFT_FE_F_SET_DRIVE_REPLY        = 21,
    UFT_FE_F_MEASURE_VOLTAGES_CMD   = 22,
    UFT_FE_F_MEASURE_VOLTAGES_REPLY = 23,
} uft_fe_frame_t;

/* ─── Rahmengroessen, GEMESSEN statt gerechnet ──────────────────────
 *
 * Der Original-Client sendet `sizeof(struct …)` Bytes, samt Fuellung.
 * Die Zahlen hier stammen aus einem uebersetzten Testprogramm gegen
 * `protocol.h` (Gutachten §2), nicht aus einer Rechnung — dieselbe
 * Vorsicht wie bei jedem Feldversatz in diesem Baum.
 *
 * Was NICHT gemessen ist: die PSoC-Seite (Cortex-M3, ARM-EABI). Da alle
 * Feldtypen <= 4 Byte sind, richtet sie nach der ABI identisch aus —
 * das ist eine ABLEITUNG und steht so im DIVERGENCES des Automaten. */
#define UFT_FE_SZ_ANY             2u
#define UFT_FE_SZ_ERROR           3u
#define UFT_FE_SZ_DEBUG          62u
#define UFT_FE_SZ_VERSION         3u
#define UFT_FE_SZ_SEEK            3u
#define UFT_FE_SZ_MEASURESPEED    3u
#define UFT_FE_SZ_SPEED           4u    /**< period_ms uint16 @ 2       */
#define UFT_FE_SZ_READ            8u    /**< milliseconds uint16 @ 4    */
#define UFT_FE_SZ_WRITE          12u    /**< bytes_to_write uint32 @ 4  */
#define UFT_FE_SZ_SET_DRIVE       5u

/* ─── Fehlercodes ───────────────────────────────────────────────────── */
#define UFT_FE_OK                  0
#define UFT_FE_ERR_INVALID        -1
#define UFT_FE_ERR_IO             -2
#define UFT_FE_ERR_TIMEOUT        -3
#define UFT_FE_ERR_PROTOCOL       -4     /**< falscher Antworttyp/Laenge  */
#define UFT_FE_ERR_VERSION        -5     /**< Fassung != 17               */
#define UFT_FE_ERR_DEVICE         -6     /**< Geraet meldet F_FRAME_ERROR */
#define UFT_FE_ERR_NOMEM          -7
#define UFT_FE_ERR_NOT_CONNECTED  -8

/** Klartext zu einem Fehlercode. Nie NULL. */
const char *uft_fe_error_string(int err);

/* ─── Byteebenen-Naht (MF-857, Muster aus MF-686/848) ───────────────
 *
 * Ohne sie waere kein Byte dieser Schicht ohne Geraet pruefbar. Alle
 * Zeiger muessen gesetzt sein; `user` wird durchgereicht. */
typedef struct {
    /** Schreibt genau @p len Byte auf den Kommando-Endpunkt. */
    int (*write)(void *user, const uint8_t *data, size_t len);
    /** Liest bis zu @p max Byte; @p actual sagt wie viele. */
    int (*read)(void *user, uint8_t *data, size_t max, size_t *actual,
                int timeout_ms);
    void *user;
} uft_fe_stream_ops_t;

typedef struct uft_fe_device uft_fe_device_t;

/**
 * Oeffnet ein Geraet, dessen Leitung eingespeist ist — fuer den
 * Pruefstand. Kein USB, kein Handschlag.
 *
 * @param ops muss das Geraet ueberleben; wird nicht kopiert.
 */
int uft_fe_open_stream(const uft_fe_stream_ops_t *ops, uft_fe_device_t **out);

/** Gibt ein Geraet frei. NULL ist erlaubt. */
void uft_fe_close(uft_fe_device_t *dev);

/** Zeitlimit je Antwort in Millisekunden (Vorgabe 5000). */
void uft_fe_set_timeout(uft_fe_device_t *dev, int timeout_ms);

/* ─── Kommandos ─────────────────────────────────────────────────────── */

/**
 * Fragt die Protokollfassung ab.
 *
 * @param out_version darf NULL sein.
 * @return UFT_FE_OK auch bei abweichender Fassung — die Bewertung
 *         trennt @ref uft_fe_version_supported, damit sie ohne Geraet
 *         pruefbar bleibt (Lehre aus MF-849).
 */
int uft_fe_get_version(uft_fe_device_t *dev, uint8_t *out_version);

/**
 * Traegt diese Fassung? Eigene Zusage statt eines Zweigs im
 * Verbindungsaufbau — siehe MF-849.
 *
 * Die Vorlage bricht bei Abweichung hart ab (`error()`-Throw,
 * `fluxengineusb.cc:73-79`), ohne Aushandlung. Das uebernehmen wir der
 * Sache nach, aber als PRUEFBARE Funktion.
 */
bool uft_fe_version_supported(uint8_t version);

/** Faehrt den Kopf auf @p track. */
int uft_fe_seek(uft_fe_device_t *dev, uint8_t track);

/** Faehrt den Kopf auf Spur 0 zurueck. */
int uft_fe_recalibrate(uft_fe_device_t *dev);

/** Waehlt Laufwerk, Dichte und Index-Betriebsart. */
int uft_fe_set_drive(uft_fe_device_t *dev, uint8_t drive,
                     bool high_density, uint8_t index_mode);

/**
 * Misst die Umdrehungsdauer.
 *
 * @param hard_sector_count 0 fuer weich sektorierte Medien.
 * @param out_period_ms Dauer in Millisekunden; darf NULL sein.
 */
int uft_fe_measure_speed(uft_fe_device_t *dev, uint8_t hard_sector_count,
                         uint16_t *out_period_ms);

#ifdef __cplusplus
}
#endif
#endif /* UFT_HAL_UFT_FLUXENGINE_H */
