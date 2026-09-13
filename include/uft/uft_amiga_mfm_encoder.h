/**
 * @file uft_amiga_mfm_encoder.h
 * @brief AmigaDOS-Trackdisk-Spur aus Sektordaten kodieren (MF-1081)
 *
 * Hebt den Blocker aus **MF-539** auf und gibt damit `ADF -> HFE` zurueck.
 *
 * ── Was vorher da war und warum es nicht trug ───────────────────────────
 *
 * Der Baum hat einen **IBM-System-34**-Encoder (`uft_mfm_encoder.c`, seit
 * MF-938 belegt) und einen **AmigaDOS-Dekoder**
 * (`decode_amiga_sector()` in `src/flux/uft_flux_decoder.c`). Was fehlte,
 * war die Gegenrichtung fuer Amiga. `uft_format_convert_bitstream.c`
 * schrieb deshalb bis MF-539 eine IBM-kodierte Spur mit einem
 * AMIGA_MFM-Kopf darueber; gemessen gegen die echte Aufnahme
 * `tests/corpus_free/gw_amigados.hfe`:
 *
 *                          echte HFE        UFTs Ausgabe
 *      Sync 0x4489              22                     0
 *      haeufigstes Byte    0x55 (12498)      0x4E (6712)
 *      rohe Nullbytes        0 von 12792   5969 von 12800
 *
 * Seit MF-539 wird die Wandlung deshalb ABGELEHNT statt eine Datei zu
 * erzeugen, die niemand lesen kann. Diese Datei liefert, was fehlte.
 *
 * ── Die Regel, und woher sie kommt ──────────────────────────────────────
 *
 * **Eigenstaendig geschrieben als exakte Umkehrung des baumeigenen,
 * bereits abgenommenen Dekoders** `decode_amiga_sector()`. Benannte
 * zweite Referenz: `mfmdisk` (Serge Vakulenko, **GPL-2.0**, im Baum unter
 * `tools/uft-scout/work/mfmdisk`, Klon `42f560badf`), dessen
 * `src/amiga.c` mit `mfm_write_amiga()` denselben Aufbau schreibt —
 * **gelesen, nicht uebernommen**; UFT steht selbst unter GPL-2.0, ein
 * Port waere also zulaessig, wird hier aber nicht gebraucht.
 *
 * Aufbau je Sektor, wie der Dekoder ihn liest:
 *
 *     2 Byte 0x00 (als MFM-Zellen), 2x Sync 0x4489 (ROH),
 *     info(4) + label(16) + hdr-Pruefsumme(4) + daten-Pruefsumme(4)
 *     + daten(512), jedes Feld ODD-Haelfte dann EVEN-Haelfte
 *
 * Je Byte gilt `roh_odd = (b >> 1) & 0x55`, `roh_even = b & 0x55`; die
 * Taktbits an den `0xAA`-Positionen kommen aus der MFM-Regel ueber den
 * ganzen Strom. Die Pruefsummen sind XOR der big-endian ROH-Langworte,
 * maskiert mit `0x55555555`.
 *
 * ── Warum die Taktbits eigens geprueft werden ───────────────────────────
 *
 * Der Dekoder maskiert nur die `0x55`-Positionen und **sieht die Taktbits
 * nicht an**. Ein Rundlauf durch die eigene Umkehrung koennte also
 * byteidentisch aufgehen, waehrend die Spur auf einem echten Laufwerk
 * unlesbar ist. Das ist woertlich die Falle aus MF-1079, wo jede
 * Spurlaenge auf das Bit stimmte und trotzdem 587 unmoegliche Zellpaare
 * darin standen. Die Abnahme misst deshalb die **Zellregel** mit:
 * keine zwei benachbarten 1-Zellen, hoechstens drei Nullen am Stueck.
 *
 * ── Grenze ──────────────────────────────────────────────────────────────
 *
 * Das **Sektor-Label** wird als 16 Nullbytes geschrieben, wenn der
 * Aufrufer keines liefert. Eine ADF traegt keine Labels; sie als Nullen
 * zu schreiben ist die Konvention jedes ADF-Schreibers und wird hier
 * benannt statt verschwiegen.
 */
#ifndef UFT_AMIGA_MFM_ENCODER_H
#define UFT_AMIGA_MFM_ENCODER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Rohe MFM-Bytes je Sektor: 2 Luecke + 4 Sync + 2*(4+16+4+4+512). */
#define UFT_AMIGA_SECTOR_MFM_BYTES  (2u + 4u + 1080u)

/**
 * @brief Kodiert eine ganze AmigaDOS-Spur.
 *
 * @param data      `sectors * 512` Byte Sektordaten, in Sektorreihenfolge
 * @param sectors   Sektoren je Spur (11 = DD, 22 = HD)
 * @param track     AmigaDOS-Spurnummer `cyl * 2 + head` (0..167)
 * @param labels    optional `sectors * 16` Byte Sektor-Label, oder NULL
 * @param out       Ausgabepuffer fuer die rohen MFM-Bytes
 * @param out_cap   Groesse von `out`
 *
 * @return Zahl der geschriebenen Bytes, oder 0 bei ungueltiger Eingabe
 *         oder zu kleinem Puffer. Gebraucht werden
 *         `sectors * UFT_AMIGA_SECTOR_MFM_BYTES` Byte.
 */
size_t uft_amiga_mfm_encode_track(const uint8_t *data, unsigned sectors,
                                  unsigned track, const uint8_t *labels,
                                  uint8_t *out, size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif /* UFT_AMIGA_MFM_ENCODER_H */
