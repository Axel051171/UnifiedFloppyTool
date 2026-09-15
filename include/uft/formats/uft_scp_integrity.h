/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_scp_integrity.h
 * @brief SCP-Integritaetspruefung und Seiten-Deutung (FLUX-6).
 *
 * Zwei Aufgaben, die heute niemand im Baum erledigt:
 *
 *  1. KOPFPRUEFSUMME. Der SCP-Kopf traegt bei 0x0C eine 32-Bit-Summe ueber
 *     alle Bytes ab 0x10 bis Dateiende. `uft_scp_parser_v3.c:1113` liest sie
 *     ein und vergleicht sie nirgends. Werkzeuge, die die Spurtabelle
 *     nachtraeglich veraendern (z. B. SCPmodSideB.py), lassen sie stehen —
 *     die Datei traegt dann eine Summe, die nicht mehr zu ihrem Inhalt passt.
 *     Fuer ein Bewahrungswerkzeug ist das die einzige eingebaute Chance,
 *     nachtraegliche Veraenderung ueberhaupt zu bemerken.
 *
 *  2. SEITEN-DEUTUNG. Kopfbyte 0x0A sagt, welche Seiten aufgenommen wurden
 *     (0 = beide, 1 = nur Seite 0, 2 = nur Seite 1). `uft_scp_plugin.c:230`
 *     setzt stattdessen fest `geo->heads = 2`. Eine einseitige Seite-B-
 *     Aufnahme wird dadurch als leere Diskette gemeldet — genau die Luecke,
 *     die SCPmodSideB.py von aussen flickt, indem es die Tabelle faelscht.
 *     Siehe KNOWN_ISSUES FLUX-6.
 *
 * SPEC_STATUS
 *   Quelle A: Jim Drew, "SuperCard Pro Image Specification" v1.9 (17.09.2019),
 *             Kopf-Layout 0x00..0x0F, Spurtabelle 168 x 4 Byte ab 0x10,
 *             TDH-Signatur "TRK" + Spurnummer.
 *   Quelle B: a8rawconv 0.95, rawdiskscp.cpp:380-424 — bildet die Summe beim
 *             Schreiben ueber Spurkopf, Bitdaten, Zeitstempel, Name UND Footer.
 *             Das belegt unabhaengig, dass der Footer mitzaehlt.
 *   Quelle C: pySuperCardPro (NF6X), scpfile.py — dritte Implementierung
 *             derselben Kopfstruktur.
 *
 * Das Modul ist absichtlich abhaengigkeitsfrei (nur C11-Standard), damit es
 * ohne den restlichen Baum gebaut und geprueft werden kann.
 *
 * ── HERKUNFT UND ABNAHME (MF-1162) ──────────────────────────────────────
 *
 * EIGENSTAENDIGE ZULIEFERUNG des Eigentuemers (`neue-ideen/uft_scp_parser.zip`,
 * 2026-09-15), uebernommen wie geliefert. Keine Zeile fremden Codes: die
 * Quellen A/B/C oben sind GELESEN, nicht kopiert. UFT ist GPL-2, das Modul
 * GPL-2.0-or-later — vereinbar, keine Lizenzentscheidung noetig.
 *
 * **Die Pruefsummenregel ist unabhaengig im eigenen Baum bestaetigt.** MF-1055
 * hat sie fuer den SCHREIBER hergeleitet und dabei gemessen: die Summe geht
 * ab Versatz 0x10 bis EOF, Offset-Tafel, Herkunftszeichenketten und
 * 48-Byte-Footer eingeschlossen; im Kopf einer Pruefdatei stand 0x01FAA3EC,
 * die Regel ergab 0x01FAB15A. Quelle dort: `src/samdisk/scp.cpp:34` (MIT, im
 * Baum), das sie auch nachrechnet (`:157`). Damit stehen DREI Haende auf
 * derselben Regel — und die Leseseite hatte sie als einzige nicht.
 *
 * Abnahme der Zulieferung vor der Verdrahtung: eigenstaendig gebaut mit
 * `-Wall -Wextra -Wpedantic`, keine Warnung, **7 von 7 Faellen bestanden**,
 * darunter der SCPmodSideB-Fall — dort melden ZWEI unabhaengige Detektoren
 * dieselbe Datei (falsche Pruefsumme und 4 aliasierte Zylinder).
 *
 * ── VIER GRENZEN, beim Lesen gefunden und hier benannt ───────────────────
 *
 *  1. `uft_scp_integrity_compute_checksum()` gibt bei zu kleiner Datei **0** zurueck,
 *     und 0 ist von einer echt gerechneten Null nicht zu unterscheiden. Auf
 *     dem Weg durch `uft_scp_integrity_verify_checksum()` kann das nicht auftreten
 *     (Groessenpruefung steht davor); wer die Funktion direkt ruft, muss die
 *     Groesse selbst pruefen.
 *  2. Das Modul prueft die Kennung `"SCP"` NICHT. Es ist als Nachlauf zum
 *     Kopf-Zerleger gedacht, der sie schon geprueft hat; eigenstaendig
 *     gerufen wuerde es jede Datei ab 688 Byte begutachten.
 *  3. `out_of_range` prueft den Offset selbst, nicht ob dahinter noch ein
 *     TDH Platz hat. Ein Offset bei `size-1` faellt damit nicht auf; der
 *     Zerleger prueft das getrennt.
 *  4. `uft_scp_resolve_sides()` nimmt `start_trk` und benutzt es nicht
 *     (`(void)start_trk`). Das ist richtig — die Startspur begrenzt, was
 *     AUFGEZEICHNET ist, nicht wie die Diskette nummeriert ist
 *     (a8rawconv `rawdiskscp.cpp:106`, und MF-481 hat genau daran einen
 *     stillen Totalverlust behoben) —, aber ein Parameter, der nur
 *     verworfen wird, gehoert benannt.
 *
 * ── WARUM DIE FUNKTIONEN `uft_scp_integrity_*` HEISSEN ──────────────────
 *
 * Die Zulieferung nannte sie `uft_scp_verify_checksum()` und
 * `uft_scp_compute_checksum()`. **Den ersten Namen gibt es im Baum schon** —
 * `src/flux/uft_scp_parser.c:689`, deklariert bei
 * `include/uft/flux/uft_scp_parser.h:370`. Gefunden hat es der BINDER, und
 * nur weil zwei Testziele beide Dateien binden:
 *
 *     uft_scp_parser.c:690: multiple definition of `uft_scp_verify_checksum'
 *
 * Umbenannt statt eine der beiden entfernt (MF-1077), und mit Vorsatz:
 * ein Name, eine Bedeutung — die Lehre von MF-1158, wo sieben solche
 * Kollisionen aufgeloest wurden. Die beiden Fassungen sind auch nicht
 * austauschbar: die bestehende nimmt einen `uft_scp_ctx_t*` und liest die
 * DATEI, diese nimmt `(data, size)` und prueft einen PUFFER. Der v3-Zerleger
 * hat einen Puffer.
 *
 * **Drei Dinge sind dabei mitgemessen worden, und alle drei gehoeren
 * aufgeschrieben:**
 *
 *  a) Die bestehende Fassung rechnet DIESELBE Regel („from offset 0x10 to
 *     end"). Damit steht die Regel im Baum VIERFACH: hier,
 *     `src/flux/uft_scp_parser.c`, `uft_scp_writer.c`
 *     (`pruefsumme_ueber_datei`, MF-1055) und `src/samdisk/scp.cpp`. Klasse
 *     MF-1015 — nur diesmal stimmen sie ueberein.
 *  b) Die bestehende Fassung hat **null Aufrufer**. Ein Verifizierer war da,
 *     rechnete richtig, und niemand rief ihn (Klasse P3-204).
 *  c) Und sie traegt einen Defekt, den diese Fassung nicht hat:
 *
 *         if (ctx->header.flags & UFT_SCP_FLAG_RW) return true;
 *
 *     Sie meldet „Pruefsumme in Ordnung", OHNE zu rechnen, sobald das
 *     R/W-Bit steht — und `uft_scp_writer.c:159` setzt genau dieses Bit in
 *     jede von UFT geschriebene SCP. Ein Erkenner, der nie nein sagt.
 *     Nicht hier behoben; steht als offener Punkt.
 *
 * **Und keines der beiden Namens-Tore haette das gefunden:**
 * `audit_typkollision.py` (MF-1155) verlangt gleiche Aritaet — hier 1 gegen
 * 4 —, und `extern_decl_conflicts.py` vergleicht Deklaration gegen
 * Definition, nicht Definition gegen Definition. Eine doppelte DEFINITION
 * sieht heute nur der Binder, und nur wenn ein Ziel beide bindet. Das ist
 * eine benannte Torluecke.
 *
 * ── UND EINE ZUSCHREIBUNG IM BAUM, DIE NICHT TRAEGT ─────────────────────
 *
 * Die Zylinderformel dieses Moduls ist bewusst dieselbe wie in
 * `uft_scp_plugin.c` — und die Zeile dort widerspricht ihrem eigenen
 * Kommentar:
 *
 *     geo->cylinders = ((int)end + 1 + 1) / 2;   (Kommentar: a8rawconv
 *                                                 rechnet (mEndTrack+1)/2)
 *
 * Der Code rundet AUF, die genannte Autoritaet rundet AB. Bei geradem `end`
 * unterscheiden sie sich um einen Zylinder (`end = 166` ergibt 84 gegen 83).
 * Gemessen ist der CODE richtig — Zylinder 83 existiert, also sind es 84 —,
 * aber der Kommentar nennt eine Quelle und weicht schweigend von ihr ab.
 * Klasse MF-938. Steht als offener Punkt, hier nicht mitgeaendert.
 */

#ifndef UFT_SCP_INTEGRITY_H
#define UFT_SCP_INTEGRITY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────────────────────────────────────
 * Konstanten (Spezifikation v1.9)
 * ───────────────────────────────────────────────────────────────────────── */

#define UFT_SCP_HEADER_SIZE      16u    /* 0x00..0x0F                        */
#define UFT_SCP_MAX_TRACKS      168u    /* 84 Zylinder x 2 Koepfe            */
#define UFT_SCP_TDHT_OFFSET      UFT_SCP_HEADER_SIZE
#define UFT_SCP_TDHT_SIZE       (UFT_SCP_MAX_TRACKS * 4u)   /* 672 Byte      */
#define UFT_SCP_MIN_FILE_SIZE   (UFT_SCP_TDHT_OFFSET + UFT_SCP_TDHT_SIZE)

#define UFT_SCP_OFF_VERSION     0x03u
#define UFT_SCP_OFF_DISKTYPE    0x04u
#define UFT_SCP_OFF_REVS        0x05u
#define UFT_SCP_OFF_START_TRK   0x06u
#define UFT_SCP_OFF_END_TRK     0x07u
#define UFT_SCP_OFF_FLAGS       0x08u
#define UFT_SCP_OFF_CELLWIDTH   0x09u
#define UFT_SCP_OFF_HEADS       0x0Au   /* 0=beide, 1=nur Seite 0, 2=nur B   */
#define UFT_SCP_OFF_RESOLUTION  0x0Bu   /* ab v1.7                           */
#define UFT_SCP_OFF_CHECKSUM    0x0Cu   /* 32 Bit LE, Summe ab 0x10 bis EOF  */

/* Werte des Kopfbytes 0x0A. */
#define UFT_SCP_HEADS_BOTH      0u
#define UFT_SCP_HEADS_SIDE0     1u
#define UFT_SCP_HEADS_SIDE1     2u

/* ─────────────────────────────────────────────────────────────────────────
 * Pruefsumme
 * ───────────────────────────────────────────────────────────────────────── */

typedef enum {
    UFT_SCP_CKSUM_OK = 0,       /* gespeicherte Summe stimmt                 */
    UFT_SCP_CKSUM_NOT_STORED,   /* Feld ist 0 — nicht gebildet, kein Befund  */
    UFT_SCP_CKSUM_MISMATCH,     /* Datei wurde nach der Aufnahme veraendert  */
    UFT_SCP_CKSUM_TOO_SHORT     /* Datei kuerzer als Kopf + Tabelle          */
} uft_scp_cksum_result_t;

/**
 * Bildet die Summe ueber data[0x10 .. size-1], 32 Bit mit Ueberlauf.
 * Nach Spezifikation v1.9 zaehlt der optionale Footer mit (Beleg: Quelle B).
 */
uint32_t uft_scp_integrity_compute_checksum(const uint8_t *data, size_t size);

/**
 * Vergleicht die berechnete Summe mit dem Kopffeld bei 0x0C.
 * @param out_stored    optional: gespeicherter Wert
 * @param out_computed  optional: berechneter Wert
 *
 * Ein Feldwert von 0 gilt als "nicht gebildet" und ergibt NOT_STORED, nicht
 * MISMATCH — es gibt Erzeuger, die das Feld leer lassen, und ein Fehlalarm
 * an dieser Stelle wuerde die Warnung wertlos machen.
 */
uft_scp_cksum_result_t uft_scp_integrity_verify_checksum(const uint8_t *data, size_t size,
                                               uint32_t *out_stored,
                                               uint32_t *out_computed);

/* ─────────────────────────────────────────────────────────────────────────
 * Befund der Spurtabelle
 * ───────────────────────────────────────────────────────────────────────── */

typedef struct {
    /* Belegung */
    unsigned populated;          /* Eintraege != 0                           */
    unsigned populated_side0;    /* davon gerade Indizes                     */
    unsigned populated_side1;    /* davon ungerade Indizes                   */
    unsigned first_cyl;          /* niedrigster Zylinder mit Eintrag, 0xFFFF
                                    wenn keiner                              */
    unsigned last_cyl;

    /* Auffaelligkeiten */
    unsigned aliased_pairs;      /* Zylinder mit offset[2c] == offset[2c+1]
                                    und != 0 — Kennzeichen einer gefaelschten
                                    Tabelle (SCPmodSideB.py)                 */
    unsigned out_of_range;       /* Offset zeigt hinter das Dateiende        */
    unsigned below_tdht;         /* Offset zeigt in Kopf oder Tabelle hinein */

    /* Ableitung */
    bool side0_empty;            /* kein gerader Eintrag belegt              */
    bool side1_empty;            /* kein ungerader Eintrag belegt            */
    bool looks_aliased;          /* aliased_pairs deckt alle belegten
                                    Zylinder ab und es sind mindestens zwei  */
} uft_scp_tdht_audit_t;

/**
 * Untersucht die Spurtabelle. Veraendert nichts und urteilt nicht — die
 * Auswertung macht der Aufrufer.
 */
bool uft_scp_audit_tdht(const uint8_t *data, size_t size,
                        uft_scp_tdht_audit_t *out);

/* ─────────────────────────────────────────────────────────────────────────
 * Seiten-Deutung (FLUX-6)
 * ───────────────────────────────────────────────────────────────────────── */

typedef enum {
    UFT_SCP_SIDES_AUTO = 0,  /* aus Kopfbyte 0x0A, ergaenzt um den Tabellen-
                                befund wenn der Kopf nichts sagt             */
    UFT_SCP_SIDES_BOTH,
    UFT_SCP_SIDES_SIDE0,
    UFT_SCP_SIDES_SIDE1
} uft_scp_sides_t;

/** Erzwungene Deutung, Gegenstueck zu a8rawconv `scp-ss40` … `scp-ds80`. */
typedef enum {
    UFT_SCP_INTERP_NONE = 0, /* nichts erzwingen                             */
    UFT_SCP_INTERP_SS40,     /* einseitig,  40 Zylinder, Doppelschritt       */
    UFT_SCP_INTERP_DS40,     /* zweiseitig, 40 Zylinder, Doppelschritt       */
    UFT_SCP_INTERP_SS80,     /* einseitig,  80 Zylinder                      */
    UFT_SCP_INTERP_DS80      /* zweiseitig, 80 Zylinder                      */
} uft_scp_interp_t;

typedef struct {
    uft_scp_sides_t sides;     /* aufgeloeste Deutung, nie AUTO              */
    unsigned head_count;        /* 1 oder 2                                  */
    unsigned first_head;        /* 0 oder 1 — bei SIDE1 ist es 1             */
    unsigned cylinders;         /* aus Spurbereich oder erzwungener Deutung  */
    bool double_step;           /* 48-TPI-Medium in 96-TPI-Laufwerk          */

    /* Herkunft der Entscheidung — fuer Protokoll und Oberflaeche */
    bool from_override;         /* Benutzer hat erzwungen                    */
    bool from_header;           /* Kopfbyte 0x0A war eindeutig               */
    bool from_tdht;             /* aus dem Tabellenbefund abgeleitet         */
    bool header_contradicts;    /* Kopf sagt etwas anderes als die Tabelle   */
} uft_scp_side_plan_t;

/**
 * Loest die Seiten-Deutung auf.
 *
 * Reihenfolge: erzwungene Deutung schlaegt Benutzerwahl schlaegt Kopfbyte
 * schlaegt Tabellenbefund. Widersprueche werden gemeldet (header_contradicts),
 * niemals stillschweigend geglaettet.
 *
 * @param heads_byte  Kopfbyte 0x0A
 * @param start_trk   Kopfbyte 0x06
 * @param end_trk     Kopfbyte 0x07
 * @param audit       Ergebnis von uft_scp_audit_tdht(), darf NULL sein
 * @param want        Benutzerwahl, UFT_SCP_SIDES_AUTO fuer "entscheide du"
 * @param interp      erzwungene Deutung, UFT_SCP_INTERP_NONE fuer keine
 */
void uft_scp_resolve_sides(uint8_t heads_byte,
                           uint8_t start_trk, uint8_t end_trk,
                           const uft_scp_tdht_audit_t *audit,
                           uft_scp_sides_t want,
                           uft_scp_interp_t interp,
                           uft_scp_side_plan_t *out);

/** Index in die Spurtabelle fuer (Zylinder, Kopf). */
static inline unsigned uft_scp_track_index(unsigned cyl, unsigned head) {
    return cyl * 2u + (head & 1u);
}

/* ─────────────────────────────────────────────────────────────────────────
 * Gesamtbefund
 * ───────────────────────────────────────────────────────────────────────── */

typedef struct {
    uft_scp_cksum_result_t checksum;
    uint32_t checksum_stored;
    uint32_t checksum_computed;
    uft_scp_tdht_audit_t tdht;
    uft_scp_side_plan_t  sides;
    bool tampered;   /* Pruefsumme falsch ODER Tabelle erkennbar gefaelscht */
} uft_scp_integrity_t;

/**
 * Fuehrt alle Pruefungen aus. Gibt false nur zurueck, wenn die Datei zu klein
 * ist, um ueberhaupt eine SCP-Datei zu sein — ein Befund ist kein Fehlschlag.
 */
bool uft_scp_check_integrity(const uint8_t *data, size_t size,
                             uft_scp_sides_t want, uft_scp_interp_t interp,
                             uft_scp_integrity_t *out);

/**
 * Schreibt eine mehrzeilige Zusammenfassung fuer Protokoll und Oberflaeche.
 * @return Zahl der geschriebenen Zeichen ohne Abschluss-Null.
 */
size_t uft_scp_integrity_summary(const uft_scp_integrity_t *in,
                                 char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SCP_INTEGRITY_H */
