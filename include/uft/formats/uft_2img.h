/**
 * @file uft_2img.h
 * @brief 2IMG / 2MG (Apple II Universal Disk Image) — pruefbare Teile
 *
 * Hier stehen nur die Funktionen, die eine **Zahl** liefern und
 * deshalb einzeln gegen die Referenz gehalten werden koennen. Der
 * vollstaendige Formatbericht samt der acht Befunde aus MF-1031 steht
 * im Kopf von `src/formats/2img/uft_2img.c`.
 *
 * Referenz: MAME `src/lib/formats/ap_dsk35.cpp` (**BSD-3-Clause**, nur
 * gelesen — Kanal *Spec* nach MF-695): `s_formats[]` Z. 452-459,
 * `apple_2mg_format::identify()` Z. 463-501, `::load()` Z. 503-580.
 * Zweite Hand, nur **ausgefuehrt**: `hxcfe`s `APPLE2_2MG`-Modul.
 */

#ifndef UFT_2IMG_H
#define UFT_2IMG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Kopfgroesse; gleichzeitig der kleinste erlaubte Datenversatz. */
#define UFT_2IMG_HDR_SIZE   64

/** Spurzahl einer 3,5"-Apple-Diskette (MAME `load()` Z. 558). */
#define UFT_2IMG_M35_TRACKS 80

/**
 * @brief Kennung pruefen — `"2IMG"` **oder** `"GMI2"`.
 *
 * MAME nimmt die byte-vertauschte Kennung ausdruecklich an, mit
 * benanntem Erzeuger: *„Bernie ][ The Rescue wrote 2MGs with the
 * signature byte-flipped, other fields are valid"* (Z. 469-470).
 * `hxcfe` weist sie ab, **ohne Grund zu nennen** (gemessen).
 *
 * @param data mindestens 4 Byte
 * @return 1 wenn eine der beiden Kennungen steht
 */
int uft_2img_signatur_ok(const uint8_t *data);

/**
 * @brief Sektoren einer 3,5"-Spur aus der Zonentafel.
 *
 * MAME `load()` Z. 560: `int ns = 12 - (track/16);` — also 12/11/10/9/8
 * fuer die Spurbereiche 0-15/16-31/32-47/48-63/64-79.
 *
 * Die Tafel bestaetigt sich aus sich selbst: `16 * (12+11+10+9+8) *
 * 512 = 409600` und das Doppelte `819200` sind genau die beiden
 * 3,5"-Laengen in MAMEs `s_formats[]`.
 *
 * @return 12..8, oder 0 wenn `cyl` ausserhalb 0..79 liegt
 */
int uft_2img_zone_spt(int cyl);

/**
 * @brief Tafeleintrag zu Anordnung und Datenlaenge suchen.
 *
 * Deckt MAMEs `s_formats[]` ab und erlaubt dessen Byte-Vertauschung der
 * Datenlaenge (`format.data_length == swapendian_int32(data_length)`,
 * Z. 483-486); `*berichtigt` erhaelt den gueltigen Wert.
 *
 * @return Zeiger auf den Eintrag (opak), oder NULL
 */
const void *uft_2img_tafel(uint32_t typ, uint32_t datenlaenge,
                           uint32_t *berichtigt);

/**
 * @brief Versatz einer Spur in der Datei.
 *
 * Nicht zoniert (5,25"): `data_offset + (cyl * heads + head) * spt * ss`.
 *
 * Zoniert (3,5"): **Spur aussen, Kopf innen**, Sektorzahl je Spur aus
 * `uft_2img_zone_spt()` — MAME `load()` Z. 558-562.
 *
 * @return Versatz, oder -1 bei unmoeglichen Koordinaten
 */
long uft_2img_track_offset(int cyl, int head, int zoniert, int heads,
                           int spt, int sektorgroesse, uint32_t data_offset);

#ifdef __cplusplus
}
#endif

#endif /* UFT_2IMG_H */
