/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_v3_parsers.h
 * @brief Der gepruefte Uebergang zwischen den drei v3-Parsern und
 *        `src/formats/uft_v3_bridge.c` (MF-1130)
 *
 * ── Warum dieser Header existiert ─────────────────────────────────────────
 *
 * Er schliesst die Ursache von MF-442. Dort stand woertlich im Bruecken-
 * Quelltext:
 *
 *     „Since this file includes no header for the v3 parsers (they have
 *      none; their types live inside the .c files), the compiler could
 *      not see the mismatch. At runtime the callee would have written
 *      0.0f through whatever the fourth argument slot happened to hold:
 *      an arbitrary-address write."
 *
 * Drei Funktionen waren dort mit DREI Parametern deklariert, wo die
 * Definition VIER nimmt, und eine mit ZWEI statt DREI. Behoben wurde
 * damals die Deklaration — nicht der Grund, dass es ueberhaupt
 * handgeschriebene Deklarationen gab. Eine lokale `extern`-Zeile ist ein
 * Versprechen, das der Uebersetzer glaubt.
 *
 * Alles, was MF-1130 neu verdrahtet, laeuft deshalb durch DIESEN Header,
 * den beide Seiten einbinden: der Parser (der die Funktion definiert) und
 * die Bruecke (die sie ruft). Eine Abweichung ist damit ein Baufehler
 * statt eines Laufzeitschadens.
 *
 * ── Was hier bewusst NICHT steht ──────────────────────────────────────────
 *
 * Die Strukturen. `d64_disk_v3_t`, `g64_disk_t` und `scp_disk_t` bleiben
 * dateilokal in ihren `.c`-Dateien; hier stehen nur opake Zeiger. Das ist
 * Absicht: `g64_disk_t` ist im Baum ZWEIMAL definiert (in
 * `uft_g64_parser_v2.c` und `uft_g64_parser_v3.c`), und ein Header mit
 * Feldlayout wuerde die beiden Fassungen gegeneinander stellen. Wer die
 * Felder braucht, ruft eine Funktion.
 *
 * Die Bruecke kommt ohnehin nur ueber `*_disk_sizeof()` an ihren Puffer;
 * sie hat die Felder nie gesehen und soll sie nicht sehen.
 */

#ifndef UFT_V3_PARSERS_H
#define UFT_V3_PARSERS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct d64_disk_v3;
struct g64_disk;
struct scp_disk;

/* ═══════════════════════════════════════════════════════════════════════
 * Geometrie AUS DEM ABBILD
 * ═══════════════════════════════════════════════════════════════════════
 *
 * Die Bruecke gab hier bis MF-1130 Konstanten heraus und ignorierte ihr
 * eigenes Handle — `(void)handle;` stand in allen drei Funktionen:
 *
 *     d64: 35 / 1 / 21        g64: 42 / 1 / 21        scp: 84 / 2 / 0
 *
 * Gemessen ist daran zweierlei falsch. Erstens tragen alle drei
 * geparsten Strukturen die echte Geometrie (`d64.tracks`,
 * `g64.track_count`, `scp.start_track`/`end_track`/`heads`) — es wurde
 * also nicht geschaetzt, wo nichts bekannt war, sondern ein vorhandener
 * Messwert uebergangen. Zweitens ist „21 Sektoren" bei CBM fuer die
 * meisten Spuren schlicht unwahr: eine 1541-Diskette hat VIER
 * Geschwindigkeitszonen mit 21/19/18/17 Sektoren, und die Tafeln dafuer
 * (`d64_get_sectors()`, `g64_get_sectors()`) liegen in denselben Dateien.
 *
 * Deshalb heisst der dritte Rueckgabewert `max_sektoren` und nicht
 * `sectors`: er nennt das MAXIMUM ueber die Zonen, und der Name sagt das.
 * Wer die Sektorzahl EINER Spur braucht, fragt `*_v3_spur_sektoren()`.
 * Eine einzelne Zahl fuer eine gezonte Diskette ist eine Zahl ohne
 * Nenner (Klasse MF-1000).
 *
 * Rueckgabe: true nur, wenn die Werte aus dem Abbild kommen. Bei false
 * bleiben die Ausgaben 0 — es gibt KEINEN Rueckfall auf eine Konstante.
 */
bool d64_v3_geometrie(const struct d64_disk_v3* disk,
                      int* spuren, int* koepfe, int* max_sektoren);
bool g64_v3_geometrie(const struct g64_disk* disk,
                      int* spuren, int* koepfe, int* max_sektoren);
bool scp_v3_geometrie(const struct scp_disk* disk,
                      int* spuren, int* koepfe, int* max_sektoren);

/* Sektoren EINER Spur (0-basierter Zylinder), aus der Zonentafel des
 * Abbilds. -1, wenn die Spur nicht bekannt ist. Fuer SCP ist die Antwort
 * immer -1: ein Flussabbild hat keine Sektorebene, bevor es dekodiert
 * ist, und eine 0 wuerde „keine Sektoren" behaupten statt „nicht
 * anwendbar". */
int d64_v3_spur_sektoren(const struct d64_disk_v3* disk, int zylinder);
int g64_v3_spur_sektoren(const struct g64_disk* disk, int zylinder);
int scp_v3_spur_sektoren(const struct scp_disk* disk, int zylinder);

/* ═══════════════════════════════════════════════════════════════════════
 * Spurdaten lesen
 * ═══════════════════════════════════════════════════════════════════════
 *
 * `uft_format_handler_t::read_track` existierte als Feld
 * (`include/uft/uft_formats_extended.h:56`) und wurde von KEINER Datei in
 * `src/` gesetzt — gemessen ueber `git ls-files`. Die 93 Dateien mit
 * `.read_track` gehoeren zum anderen Typ, `uft_format_plugin_t`. Damit
 * konnte `uft_advanced_get_track_quality()` nie messen; die erfundene
 * Guete 1.0 aus MF-1129 hat genau diese Luecke verdeckt.
 *
 * Diese drei Funktionen fuellen sie — und zwar durch ENTNAHME aus der
 * bereits geparsten Struktur, nicht durch einen zweiten Decoder. Was
 * jeweils herauskommt, ist das, was das Format an dieser Stelle WIRKLICH
 * traegt, und nicht eine einheitliche Fiktion:
 *
 *   d64  die Sektordaten der Spur, hintereinander (256 Byte je Sektor,
 *        in Sektorreihenfolge). D64 ist ein Sektorabbild; einen Zellstrom
 *        hat es nicht, und einen zu erzeugen waere eine Erfindung.
 *   g64  die rohen GCR-Bytes der Spur (`gcr_data`/`gcr_size`) — G64 IST
 *        ein Bitstromformat.
 *   scp  nichts. SCP traegt Flusszeiten, keine Bytes; sie in einen
 *        Bytepuffer zu schreiben waere eine stille Umdeutung. Die
 *        Funktion sagt ab, damit ein Aufrufer den Unterschied merkt
 *        statt Muell zu bekommen.
 *
 * Vertrag, fuer alle drei gleich:
 *   - `groesse` ist beim Aufruf die Kapazitaet von `aus`, danach die
 *     Anzahl WIRKLICH geschriebener Byte;
 *   - passt die Spur nicht, wird NICHTS geschrieben, `*groesse` nennt
 *     den Bedarf und die Funktion gibt false — kein Kuerzen (die Klasse
 *     MF-1040, wo ein 70 000-Byte-Block still auf 65 535 fiel);
 *   - `aus == NULL` mit gueltigem `groesse` fragt nur den Bedarf ab;
 *   - false heisst immer: in `aus` steht nichts Neues.
 */
bool d64_v3_spur_lesen(const struct d64_disk_v3* disk, int zylinder, int kopf,
                       uint8_t* aus, size_t* groesse);
bool g64_v3_spur_lesen(const struct g64_disk* disk, int zylinder, int kopf,
                       uint8_t* aus, size_t* groesse);
bool scp_v3_spur_lesen(const struct scp_disk* disk, int zylinder, int kopf,
                       uint8_t* aus, size_t* groesse);

/* ═══════════════════════════════════════════════════════════════════════
 * Der Spurbefund — die GEMESSENE Guete, nicht eine gerechnete
 * ═══════════════════════════════════════════════════════════════════════
 *
 * `uft_advanced_get_track_quality()` hat seine Guete bis MF-1130 aus
 * einer Byteanalyse des rohen Spurinhalts gewonnen, und die war auf die
 * erreichbaren Formate NICHT ANWENDBAR:
 *
 *   - sie sucht `A1 A1 A1 FE`, eine IBM-MFM-Adressmarke. D64 und G64
 *     tragen Commodore-GCR; diese Folge kommt darin nicht vor, also war
 *     `error_count` immer 0;
 *   - ihre Schwachbit-Erkennung verlangt ACHT gleiche Bytes (0x00/0xFF).
 *     Eine G64-Synchronmarke ist fuenf — also traf auch das nie;
 *   - und die CRC lief ueber ACHT Byte ab dem ersten `A1` mit Erwartung
 *     0. Richtig sind ZEHN (`A1 A1 A1 FE C H R N CRC1 CRC2`), erst dann
 *     ist der Rest 0. Ueber acht Byte kommt der CRC-WERT heraus, nie
 *     null — auf einer echten MFM-Diskette haette also JEDE Adressmarke
 *     als Fehler gezaehlt.
 *
 * Ergebnis war `quality = 1.0 - 0 - 0`, also bestaendig 1.0 — gemessen
 * an `tests/corpus/c64pp_aliensyndrome.g64`, einer KOPIERGESCHUETZTEN
 * Diskette, ueber alle 40 Spuren. Das ist kein Vorgabewert mehr (den hat
 * MF-1129 entfernt), sondern das Ergebnis einer Rechnung, die zu diesem
 * Format nicht passt. Dieselbe Klasse in anderer Gestalt.
 *
 * Diese Funktionen liefern stattdessen, was der Parser beim ECHTEN
 * GCR-Lauf gezaehlt hat. Jede Zahl hat einen Nenner, und keine wird zu
 * einer einzelnen Note verrechnet, bevor der Aufrufer sie gesehen hat.
 *
 * Rueckgabe false heisst: fuer diese Spur liegt kein Befund vor. `aus`
 * ist dann genullt — eine 0 in `gueltig` ohne `erwartet` ist keine
 * Aussage (Klasse MF-1000).
 */
typedef struct {
    int  erwartet;   /* Sektoren, die die Zonentafel des Abbilds nennt */
    int  gefunden;   /* im Abbild wirklich angetroffen */
    int  gueltig;    /* davon mit stimmender Pruefsumme */
    int  fehler;     /* davon mit falscher Pruefsumme */
    bool schwach;    /* Schwachbits laut Parser */
    bool geschuetzt; /* Schutzmerkmal laut Parser */
} uft_v3_spurbefund_t;

bool d64_v3_spur_befund(const struct d64_disk_v3* disk, int zylinder, int kopf,
                        uft_v3_spurbefund_t* aus);
bool g64_v3_spur_befund(const struct g64_disk* disk, int zylinder, int kopf,
                        uft_v3_spurbefund_t* aus);
bool scp_v3_spur_befund(const struct scp_disk* disk, int zylinder, int kopf,
                        uft_v3_spurbefund_t* aus);

/* ── Und dieselben drei auf BRUECKEN-Ebene ─────────────────────────────────
 *
 * Die drei oben nehmen die PARSER-Struktur. Was ein Aufrufer aus
 * `handler->open()` bekommt, ist aber der opake Griff der Bruecke, und
 * die Parser-Struktur liegt DARIN. Beides zu verwechseln ist ein Cast auf
 * die falsche Stelle — genau die Klasse, die MF-442 in dieser Bruecke
 * gefunden hat, nur mit einem Typ statt einer Parameterzahl.
 *
 * Die drei hier nehmen deshalb den GRIFF und packen ihn aus. Wer nur den
 * Griff hat (`uft_advanced_mode.c` etwa), benutzt diese. */
bool uft_v3_griff_befund_d64(void* griff, int zylinder, int kopf,
                             uft_v3_spurbefund_t* aus);
bool uft_v3_griff_befund_g64(void* griff, int zylinder, int kopf,
                             uft_v3_spurbefund_t* aus);
bool uft_v3_griff_befund_scp(void* griff, int zylinder, int kopf,
                             uft_v3_spurbefund_t* aus);

#ifdef __cplusplus
}
#endif

#endif /* UFT_V3_PARSERS_H */
