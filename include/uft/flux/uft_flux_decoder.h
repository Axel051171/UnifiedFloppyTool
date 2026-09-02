/**
 * @file uft_flux_decoder.h
 * @brief Universal Flux-to-Sector Decoder
 *
 * Decodes raw flux timing data into sector data for various encodings:
 * - MFM (Modified Frequency Modulation) - PC, Amiga, Atari ST
 * - FM (Frequency Modulation) - older 8" drives, Apple II
 * - GCR (Group Coded Recording) - C64, Apple II
 * 
 * Supports flux data from:
 * - SuperCard Pro (.scp)
 * - KryoFlux (.raw)
 * - DiscFerret (.dfi)
 * - Greaseweazle (.gw)
 * 
 * Reference: Various ROMs, ROL analysis
 */

#ifndef UFT_FLUX_DECODER_H
#define UFT_FLUX_DECODER_H

#include "uft/flux/uft_track_verdikt.h"
#include "uft/core/uft_unified_types.h"
#include "uft/flux/uft_media_profile.h"  /* uft_media_kind_t (MF-471) */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Standard bit cell times in nanoseconds */
#define FLUX_MFM_HD_BITCELL_NS      1000    /* 1µs for HD MFM (500 kbps) */
#define FLUX_MFM_DD_BITCELL_NS      2000    /* 2µs for DD MFM (250 kbps) */
#define FLUX_MFM_ED_BITCELL_NS       500    /* 0.5µs for ED MFM (1 Mbps) */
#define FLUX_FM_BITCELL_NS          4000    /* 4µs for FM (125 kbps) */
#define FLUX_GCR_C64_BITCELL_NS     3200    /* ~3.2µs for C64 GCR */
#define FLUX_GCR_APPLE_BITCELL_NS   4000    /* 4µs for Apple II GCR */

/* Sync patterns */
#define MFM_SYNC_PATTERN            0x4489  /* MFM sync (A1 with missing clock) */
#define FM_SYNC_PATTERN             0xF57E  /* FM sync (FE with clock) */
#define FM_IAM_PATTERN              0xF77A  /* FM Index Address Mark */

/* Address marks */
#define MFM_IDAM                    0xFE    /* ID Address Mark */
#define MFM_DAM                     0xFB    /* Data Address Mark */
#define MFM_DDAM                    0xF8    /* Deleted Data Address Mark */

/* Tolerances */
#define FLUX_PLL_GAIN               0.05    /* PLL adjustment gain */

/* Maximum values */
#define FLUX_MAX_SECTORS            64
#define FLUX_MAX_TRACK_SIZE         65536
#define FLUX_MAX_REVOLUTIONS        16

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Flux encoding types
 */
typedef enum {
    FLUX_ENC_AUTO = 0,      /* Auto-detect encoding */
    FLUX_ENC_MFM,           /* MFM (PC, Amiga, Atari ST) */
    FLUX_ENC_FM,            /* FM (8", early systems) */
    FLUX_ENC_GCR_C64,       /* GCR Commodore 64 */
    FLUX_ENC_GCR_APPLE,     /* GCR Apple II */
    FLUX_ENC_AMIGA,         /* Amiga-specific MFM */
    FLUX_ENC_RAW            /* Raw bits, no decoding */
} flux_encoding_t;

/**
 * @brief Decoder status codes
 */
typedef enum {
    FLUX_OK = 0,
    FLUX_ERR_NO_SYNC,       /* No sync pattern found */
    FLUX_ERR_BAD_HEADER,    /* Sync gefunden, Info-Long unbrauchbar (MF-454) */
    FLUX_ERR_BAD_CRC,       /* CRC mismatch */
    FLUX_ERR_NO_DATA,       /* No data after ID */
    FLUX_ERR_WEAK_BITS,     /* Unreliable flux timing */
    FLUX_ERR_OVERFLOW,      /* Buffer overflow */
    FLUX_ERR_UNDERFLOW,     /* Not enough data */
    FLUX_ERR_INVALID,       /* Invalid parameters */

    /* MF-764: „kein Sync" war VIER Zustaende in einem Wort.
     *
     * Gemessen mit drei synthetischen Spuren durch den echten Dekoder —
     * eine geloeschte, eine gleichfoermig beschriebene mit drei
     * Bruchstellen, eine verrauschte. Alle drei lieferten
     * FLUX_ERR_NO_SYNC, 0 Sektoren, alle Zaehler 0. Physisch voellig
     * verschiedene Medienzustaende, ein Verdikt.
     *
     * X-Copys Handbuch von 1991 (3.4, Abschnitt 7.2) trennt sie: „keine
     * Lesemarkierungen gefunden" heisst dort ausdruecklich
     * „wahrscheinlich ein Kopierschutz ODER FREMDFORMAT" — nicht „leer"
     * und nicht „unlesbar".
     *
     * Unterschieden wird am INTERVALL-HISTOGRAMM, das dieser Baum seit
     * MF-488 hat. Gemessen an denselben drei Spuren:
     *
     *     leer          0 Berge   coverage 0,000
     *     gleichfoermig 1 Berg    coverage 0,000
     *     verrauscht    8 Berge   coverage 0,187
     *
     * Kein neues Verfahren, keine Erfindung — ein vorhandenes Modul,
     * dessen Aussage bisher niemand las. Angehaengt statt eingefuegt:
     * die Werte davor behalten ihre Zahlen. */
    FLUX_ERR_UNFORMATTED,   /**< keine Wechsel-Struktur: leere/geloeschte Spur */
    FLUX_ERR_NOISE          /**< Wechsel ohne gemeinsamen Takt: unlesbar */,

    /* MF-766: „ich kann die Kodierung nicht bestimmen" ist eine Auskunft,
     * kein ungueltiges Argument. Vorher lief dieser Fall in
     * FLUX_ERR_INVALID („Invalid parameters") — eine Meldung, die den
     * Benutzer an seinem Aufruf zweifeln laesst statt an der Spur.
     * Angehaengt statt eingefuegt. */
    FLUX_ERR_ENCODING_UNKNOWN
} flux_status_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Raw flux data input
 */
typedef struct {
    uint32_t *transitions;      /* Flux transition times (in sample ticks) */
    size_t    transition_count;
    uint32_t  sample_rate;      /* Sample rate in Hz */
    uint32_t *index_times;      /* Index pulse positions */
    size_t    index_count;
} flux_raw_data_t;

/**
 * @brief Decoded sector information
 */
typedef struct {
    uint8_t  cylinder;
    uint8_t  head;
    uint8_t  sector;
    uint8_t  size_code;         /* 0=128, 1=256, 2=512, 3=1024 */
    
    uint8_t *data;
    size_t   data_size;
    
    /* MF-452: 32 Bit, nicht 16.
     *
     * IBM/ISO-CRCs sind 16-bittig und passten. AmigaDOS speichert je eine
     * 32-Bit-Pruefsumme fuer Header und Daten — decode_amiga_sector() schrieb
     * sie in ein uint16_t und verlor die obere Haelfte. Die Gueltigkeitsflags
     * daneben waren richtig (verglichen wurde vor der Zuweisung mit voller
     * Breite), aber der GESPEICHERTE Wert war halbiert. Ein Bericht, der die
     * Pruefsumme ausgibt, gab damit eine Zahl aus, die nicht auf der Diskette
     * steht. */
    uint32_t id_crc;            /* CRC/checksum from ID field (32 bit: Amiga) */
    uint32_t data_crc;          /* CRC/checksum from data field */
    bool     id_crc_ok;
    bool     data_crc_ok;
    bool     deleted;           /* Deleted data mark */

    /* AmigaDOS sector label — 16 Byte OS-Recovery-Information (MF-452).
     *
     * Liegt im Sektorkopf zwischen Info-Long und Header-Pruefsumme, geht in
     * die Header-Pruefsumme ein und wurde bis MF-452 gelesen und mit
     * `(void)label;` verworfen: das Struct hatte kein Feld dafuer. Bei einem
     * Werkzeug mit dem Grundsatz "Kein Bit verloren" war das stiller
     * Datenverlust auf dem Hauptpfad — AmigaDOS legt dort
     * Wiederherstellungsdaten ab, und mehrere Schutzverfahren benutzen das
     * Feld als Ablage.
     *
     * label_present unterscheidet "16 Nullbytes gelesen" von "nicht
     * gelesen" — bei IBM/GCR-Formaten gibt es kein Label. */
    uint8_t  label[16];
    bool     label_present;
    
    /* Timing info */
    uint32_t id_position;       /* Position in flux stream */
    uint32_t data_position;
    double   bitrate;           /* Measured bitrate */
    
} flux_decoded_sector_t;

/**
 * @brief Welche Zeitbasis das Ergebnis geliefert hat (MF-496).
 *
 * Die Reihenfolge ist die Reihenfolge der Kandidaten im Decoder, und
 * @ref FLUX_TIMING_INITIAL ist bewusst 0: eine auf 0 gesetzte Struktur
 * behauptet damit nichts, was nicht gemessen wurde.
 */
typedef enum {
    FLUX_TIMING_INITIAL = 0,  /**< erste Wahl: Profil, Histogramm, Nennwert */
    FLUX_TIMING_MEASURED,     /**< an den Sync-Marken gemessen (MF-492) */
    FLUX_TIMING_DEWARPED      /**< Strom entzerrt dekodiert (MF-495) */
} flux_timing_source_t;

/**
 * @brief Decoded track result
 */
typedef struct {
    flux_decoded_sector_t sectors[FLUX_MAX_SECTORS];
    size_t sector_count;
    
    flux_encoding_t detected_encoding;
    double avg_bitrate;
    uint32_t track_length_bits;
    
    /* Statistics */
    size_t good_sectors;
    size_t bad_id_crc;
    size_t bad_data_crc;
    size_t missing_data;

    /* Warum ein Sektor verworfen wurde (MF-454).
     *
     * X-Copy meldet pro Spur einen von acht Fehlercodes. Sie sind doppelt
     * belegt: 1-6 im Quelltext (xcop.s:1163-1174, Label `tofewsc`, `nosync`,
     * `no2sync`, `hecksum`, `headerr`, `blcksum`), 1-8 im Handbuch von 1992
     * (Siren Software, archive.org).
     *
     *   1 mehr oder weniger als 11 Sektoren    5 Fehler im Header-Longword
     *   2 kein Sync gefunden                   6 Datenblock-Pruefsumme
     *   3 nach dem Gap kein weiterer Sync      7 Longtrack
     *   4 Header-Pruefsumme                    8 Verify-Fehler
     *
     * UFT hatte davon drei: bad_id_crc (4), bad_data_crc (6) und teilweise
     * missing_data. Klasse 5 fiel bisher unter den Tisch — ein Sektor, dessen
     * Info-Long nicht mit 0xFF beginnt oder dessen Spur-/Sektornummer nicht in
     * die Geometrie passt, wurde als FLUX_ERR_NO_SYNC gemeldet, also als
     * "kein Sync gefunden". Das ist eine andere Diagnose: der Sync WAR da, der
     * Kopf dahinter passt nicht. Auf einer geschuetzten Diskette ist genau das
     * der interessante Fall.
     *
     * 1, 3, 7 und 8 sind hier nicht zaehlbar — sie sind Aussagen ueber die
     * ganze Spur bzw. ueber einen Schreibvorgang, nicht ueber einen Sektor.
     * Sie fehlen absichtlich statt mit einer Naeherung gefuellt zu werden. */
    size_t bad_header_format;   /**< X-Copy-Klasse 5: Sync ok, Info-Long nicht */
    
    /* Raw decoded bits (optional) */
    uint8_t *raw_bits;
    size_t   raw_bit_count;

    /* ── Woher die Zeitbasis kam (MF-496) ──────────────────────────────
     *
     * Der Decoder probiert seit MF-492/MF-495 mehrere Zeitbasen durch und
     * behaelt die beste. Bis hierher wusste das niemand ausserhalb: die
     * Messung entstand, entschied ueber das Ergebnis und wurde verworfen.
     * Ein Werkzeug, das den Menschen in der Schleife halten will, muss
     * sagen koennen, WAS es gemessen hat und WELCHE Wahl gewonnen hat.
     *
     * Angehaengt statt eingefuegt — eingefuegte Felder verschieben das
     * Layout aller nachfolgenden und brechen still jeden Aufrufer, der
     * gegen die alte Fassung uebersetzt wurde.
     *
     * Alle Felder sind 0, wenn nichts gemessen wurde; Aufrufer setzen die
     * Struktur ohnehin auf 0, bevor sie dekodieren lassen. */

    /** Zellendauer der ERSTEN Wahl in ns — Medienprofil, Histogramm oder
     *  Nennwert, je nachdem was greifen konnte. */
    double initial_cell_ns;

    /** Zellendauer, die die Sync-Suche im Strom GEMESSEN hat (MF-492), in
     *  ns. 0 = keine belastbare Messung (unter drei Fundstellen). */
    double measured_cell_ns;

    /** Verhaeltnis groesster zu kleinster Zellendauer entlang der Spur
     *  (MF-495). 0 = nicht bestimmt, 1,0 = kein Gleichlauffehler. */
    double warp_span;

    /** Zellendauer, mit der das behaltene Ergebnis tatsaechlich dekodiert
     *  wurde, in ns. Bei @ref FLUX_TIMING_DEWARPED ist das der Bezugstakt
     *  des entzerrten Stroms — also NICHT @ref measured_cell_ns.
     *
     *  Ohne diesen Wert laesst sich eine Bitposition nicht in eine Zeit
     *  und damit nicht in eine Winkellage umrechnen (MF-501). */
    double used_cell_ns;

    /** Welcher Kandidat das Ergebnis geliefert hat. */
    flux_timing_source_t timing_source;

    /* MF-765: das Spurverdikt als DREI Felder statt einer Ziffer.
     * ANGEHAENGT, nicht eingefuegt — die Felder davor behalten ihre
     * Lage. Gefuellt vom einen Bauer in `uft_track_verdikt.c`; die
     * fuenf Rueckgabestellen des Dekoders rufen ihn, damit jede
     * weitere Verfeinerung EINE Zeile beruehrt statt fuenf. */
    uft_track_verdikt_t verdikt;

} flux_decoded_track_t;

/**
 * @brief Decoder options
 */
typedef struct {
    flux_encoding_t encoding;   /* Encoding to use (AUTO = detect) */
    uint32_t bitcell_ns;        /* Expected bit cell time (0 = auto) */

    /* Medium, mit dem die Diskette BESCHRIEBEN wurde (MF-471).
     *
     * UFT_MEDIA_UNKNOWN (Default) laesst alles wie bisher: bitcell_ns == 0
     * bedeutet dann weiterhin "MFM DD annehmen".
     *
     * Ist hier ein Profil gesetzt UND traegt das Abbild mindestens zwei
     * Index-Impulse, rechnet der Decoder die Zellendauer aus der GEMESSENEN
     * Umdrehungsdauer statt sie anzunehmen. Das ist der Fall, ohne den eine
     * Atari-Diskette (288 min^-1) in einem 300-min^-1-Laufwerk um 4 %
     * daneben liegt. Ein ausdruecklich gesetztes bitcell_ns hat weiterhin
     * Vorrang — wer eine Zahl vorgibt, meint sie.
     *
     * Siehe include/uft/flux/uft_media_profile.h. */
    uft_media_kind_t media;

    /* Feineinsteller in Prozent fuer die Zellendauer, 50…200; 0 oder 100
     * bedeutet unveraendert. Entspricht a8rawconvs `-p` und wirkt nur,
     * wenn @ref media gesetzt ist. */
    double media_adjust_pct;
    bool     use_pll;           /* Use PLL for timing recovery */
    double   pll_gain;          /* PLL adjustment gain */
    bool     keep_raw_bits;     /* Keep raw decoded bits */

    /* Hier standen bis MF-669 drei weitere Felder: `tolerance`,
     * `revolution` und `decode_all_revs`. Alle drei wurden gesetzt und
     * von niemandem gelesen — gemessen von
     * `scripts/audit_setting_wiring.py`, das genau diese Klasse sucht.
     *
     * `revolution` und `decode_all_revs` waren Doppelgaenger: die
     * Umdrehungswahl gibt es wirklich, unter den Namen
     * `use_multiple_revs` und `synthetic_revolutions` in den
     * Wandlungsoptionen, mit sieben Lesestellen zusammen.
     *
     * `tolerance` hatte keinen Doppelgaenger, sondern keinen
     * Mechanismus. Wer es wieder einfuehrt, baut zuerst die Lesestelle
     * gegen eine benannte Referenz (Einfrier-Regel), dann das Feld —
     * nicht umgekehrt. Siehe GUI-7 in docs/OPEN_ITEMS.md. */

    /* Sync-Muster fuer den Amiga-Pfad (MF-453).
     *
     * NULL/0 bedeutet: nur der Standard-Sync 0x4489. Das ist der Default und
     * aendert nichts an bestehenden Ergebnissen.
     *
     * Wer geschuetzte Disketten lesen will, uebergibt hier
     * UFT_AMIGA_SYNC_PATTERNS / UFT_AMIGA_SYNC_COUNT aus
     * include/uft/formats/uft_amiga_syncs.h. Bis MF-453 suchte der Decoder
     * fest nur 0x4489, waehrend drei andere Module wussten, dass es
     * Arkanoid-, Beyond-the-Ice-Palace- und Mercenary-Syncs gibt — eine
     * solche Diskette dekodierte zu null Sektoren.
     *
     * Der Zeiger muss den Aufruf ueberleben; die Liste wird nicht kopiert. */
    const uint16_t *sync_patterns;
    size_t          sync_count;
} flux_decoder_options_t;

/**
 * @brief PLL state for timing recovery
 */
typedef struct {
    double   period;            /* Current bit cell period */
    double   phase;             /* Current phase */
    double   freq_gain;         /* Frequency adjustment gain */
    double   phase_gain;        /* Phase adjustment gain */
    uint32_t last_transition;   /* Last transition time */
    bool     use_pll;           /* Enable PLL tracking */
} flux_pll_t;

/* ============================================================================
 * Initialization Functions
 * ============================================================================ */

/**
 * @brief Initialize decoder options with defaults
 */
void flux_decoder_options_init(flux_decoder_options_t *opts);

/**
 * @brief Initialize PLL state
 */
void flux_pll_init(flux_pll_t *pll, double initial_period);

/* ── Zwei PLL-Profile statt einer festen Einstellung (MF-808) ────────────
 *
 * `flux_pll_init()` setzt seit jeher EINE Einstellung: `freq_gain=0.02`,
 * `phase_gain=0.5`. Es gibt keine zweite, und `read_flux` hat auch keinen
 * Weg, dieselbe Aufnahme mit einer anderen noch einmal zu dekodieren.
 *
 * Die Referenz führt **zwei benannte Profile** und fährt sie kaskadiert:
 * erst das eine, und solange Sektoren fehlen, dasselbe Flussbild noch
 * einmal durch das andere. Beides gemessen an
 * `keirf/greaseweazle`, Commit
 * `a0ae343d7e2603b9f3fdc0149ef8e89de5399f58` (v1.23, **Unlicense**,
 * Public Domain), `src/greaseweazle/track.py` Zeilen 10–40:
 *
 *     PLL('period=5:phase=60')   „An aggressive PLL which will quickly
 *                                 sync to extreme bit timings."
 *     PLL('period=1:phase=10')   „A conservative PLL which is good at
 *                                 ignoring noise in otherwise fairly
 *                                 well-behaved tracks."
 *
 * `gw read --help` beschreibt beide Werte als *„adjustment as percentage
 * of phase error"* — nachgesehen, nicht angenommen.
 *
 * ── WAS HIER NICHT BEHAUPTET WIRD ───────────────────────────────────────
 *
 * Dass die Zahlen dasselbe BEDEUTEN. Gemessen an
 * `src/flux/uft_flux_decoder.c`:
 *
 *   * Die Periodenanpassung hat dieselbe Form —
 *     `period += error * freq_gain / num_cells`, also ein Bruchteil des
 *     Fehlers. Hier ist `5 %` → `0.05` eine saubere Übertragung.
 *   * Die Phase NICHT. UFT führt einen leckenden Integrator
 *     (`phase = (1-α)·phase + α·error`); die Referenz beschreibt eine
 *     unmittelbare Korrektur um einen Prozentsatz. Gleiche Rolle,
 *     andere Form.
 *
 * Die Profile sind deshalb **übernommene Ausgangspunkte**, keine
 * portierten Konstanten. Was sie taugen, entscheidet die Messung, nicht
 * die Herkunft — `tests/test_flux_pll_profile.c` belegt, dass sie sich
 * auf demselben Flussbild UNTERSCHIEDLICH verhalten. Ohne diesen Beleg
 * wäre ein zweites Profil Zierat.
 *
 * `flux_pll_init()` bleibt UNVERÄNDERT bei 0.02/0.5. Eine geänderte
 * Vorgabe wäre eine Verhaltensänderung an jedem bestehenden Dekodierlauf
 * ohne Messung dahinter.
 */
typedef enum {
    /** Folgt extremen Bitzeiten schnell — für lange und ratenvariable
     *  Spuren. Referenz: `period=5:phase=60`. */
    UFT_PLL_PROFIL_AGGRESSIV = 0,
    /** Überhört Hochfrequenzrauschen — für sonst gutmütige Spuren mit
     *  Schmutz oder Schimmel. Referenz: `period=1:phase=10`. */
    UFT_PLL_PROFIL_KONSERVATIV,
    UFT_PLL_PROFIL_ANZAHL
} uft_pll_profil_t;

/** Kurzname für Protokolle und Tests („aggressiv" / „konservativ"). */
const char *uft_pll_profil_name(uft_pll_profil_t profil);

/**
 * PLL mit einem benannten Profil aufsetzen.
 *
 * Ein unbekanntes Profil setzt AGGRESSIV und meldet das — es wird nicht
 * stillschweigend etwas anderes gewählt.
 */
void flux_pll_init_profil(flux_pll_t *pll, double initial_period,
                          uft_pll_profil_t profil);

/**
 * @brief Initialize decoded track structure
 */
void flux_decoded_track_init(flux_decoded_track_t *track);

/**
 * @brief Free decoded track resources
 */
void flux_decoded_track_free(flux_decoded_track_t *track);

/* ============================================================================
 * Main Decoding Functions
 * ============================================================================ */

/**
 * @brief Decode flux data to sectors
 * 
 * @param flux Raw flux data
 * @param track Output decoded track
 * @param opts Decoder options (NULL for defaults)
 * @return Status code
 */
flux_status_t flux_decode_track(const flux_raw_data_t *flux,
                                flux_decoded_track_t *track,
                                const flux_decoder_options_t *opts);

/**
 * @brief Decode MFM flux data
 */
flux_status_t flux_decode_mfm(const flux_raw_data_t *flux,
                              flux_decoded_track_t *track,
                              const flux_decoder_options_t *opts);

/**
 * @brief Decode FM flux data
 */
flux_status_t flux_decode_fm(const flux_raw_data_t *flux,
                             flux_decoded_track_t *track,
                             const flux_decoder_options_t *opts);

/**
 * @brief Decode C64 GCR flux data
 */
flux_status_t flux_decode_gcr_c64(const flux_raw_data_t *flux,
                                  flux_decoded_track_t *track,
                                  const flux_decoder_options_t *opts);

/**
 * @brief Decode Apple II GCR flux data
 */
flux_status_t flux_decode_gcr_apple(const flux_raw_data_t *flux,
                                    flux_decoded_track_t *track,
                                    const flux_decoder_options_t *opts);

/**
 * @brief Decode Amiga (AmigaDOS trackdisk) MFM flux data
 *
 * Amiga MFM is whole-track MFM with a layout unrelated to IBM System-34:
 * 11 sectors/track, each = 2x 0x4489 sync, an odd/even-split info long,
 * a 16-byte sector label, header + data checksums, and a 512-byte
 * odd/even-split data block. The IBM-MFM decoder (flux_decode_mfm)
 * cannot parse it — there are no IDAM/DAM address marks.
 */
/**
 * @brief Decode AmigaDOS sectors from an already-recovered BITSTREAM.
 *
 * The bitstream-level half of flux_decode_amiga(), split out in MF-437 so
 * callers that already hold recovered cells — an HFE container stores cells,
 * not flux — can decode without synthesising flux and running a PLL over it.
 *
 * The buffer belongs to the caller: this never frees it and never stores it
 * in track->raw_bits.
 */
/**
 * @brief Build flux_raw_data_t from ns INTERVALS (what SCP readers produce).
 *
 * flux_raw_data_t::transitions holds cumulative TIMES; SCP-family readers
 * hand out intervals. Passing intervals directly decodes to nothing, silently
 * (MF-438). Zero entries are SCP overflow placeholders and are skipped.
 *
 * Caller owns out->transitions — release with flux_raw_free().
 */
flux_status_t flux_raw_from_ns_intervals(const uint32_t *intervals,
                                         size_t count,
                                         flux_raw_data_t *out);

/**
 * @brief Wie flux_raw_from_ns_intervals(), aber mit der GEMESSENEN
 *        Umdrehungsdauer des Datensatzes (MF-475).
 *
 * Die Schwesterfunktion ohne Umdrehungsdauer liefert eine Spur ohne
 * Index-Impulse (`index_count == 0`). Fuer die Zellendauer-Bestimmung nach
 * MF-471 ist das der Unterschied zwischen einer Messung und einer Annahme:
 * ohne Index-Impulse faellt sie auf „MFM DD" zurueck, und eine Diskette mit
 * abweichender Drehzahl — Atari 810/1050/XF551 mit 288 min⁻¹ — wird
 * schweigend um 4 % daneben dekodiert.
 *
 * @p revolution_ns ist die Zeit vom Indexpuls, an dem der Datensatz beginnt,
 * bis zum naechsten. Ein SCP-Umdrehungskopf traegt sie als `duration`
 * (`src/formats/scp/uft_scp_plugin.c` → `uft_track_t::metrics.index_time_ns`).
 * Daraus entstehen genau zwei Marken: 0 und @p revolution_ns.
 *
 * **Nicht uebernommen wird eine Dauer, die nicht zum Datenstrom passt.** Deckt
 * der Strom mehrere Umdrehungen oder nur einen Bruchteil, waeren zwei Marken
 * eine falsche Aussage ueber seine Struktur; dann bleibt @p out ohne Marken
 * und die Zellendauer faellt auf ihren Vorgabewert zurueck. Toleranz ist
 * ±50 % der kumulierten Flusszeit — weit genug fuer eine angeschnittene
 * Umdrehung, eng genug, um zwei davon auszuschliessen.
 *
 * @p revolution_ns == 0 heisst „nicht gemessen" und ist kein Fehler; die
 * Funktion verhaelt sich dann wie ihre Schwester.
 *
 * Caller owns out->transitions und out->index_times — beides gibt
 * flux_raw_free() frei.
 */
flux_status_t flux_raw_from_ns_intervals_indexed(const uint32_t *intervals,
                                                 size_t count,
                                                 uint32_t revolution_ns,
                                                 flux_raw_data_t *out);

/**
 * @brief Zeitachse einer Rohspur umdrehen — Flippy-Rueckseite (MF-484).
 *
 * Eine Flippy-Diskette wurde beschrieben, indem man sie im einseitigen
 * Laufwerk umdrehte. Liest man sie spaeter im zweiseitigen Laufwerk vom
 * zweiten Kopf, laeuft dieselbe Spur RUECKWAERTS am Kopf vorbei — der
 * Datenstrom ist zeitlich gespiegelt, und kein Sync-Muster passt mehr.
 * Fuer Bestaende mit C64-, Atari- und Apple-Disketten ist das der
 * Regelfall, nicht die Ausnahme.
 *
 * Portierung von a8rawconvs `-r` (`reverse_track`, disk.cpp:63-89):
 * jede Zeit wird zu `max_time - t`, danach werden beide Folgen umgedreht.
 * Beide bleiben dadurch aufsteigend, und der Abstand zweier Indexmarken
 * bleibt gleich — die gemessene Umdrehungsdauer ueberlebt die Spiegelung.
 *
 * Arbeitet in-place und belegt nichts; zweimal angewandt ist es die
 * Identitaet (bis auf einen etwaigen Versatz, wenn der letzte Uebergang
 * nicht auf @c max_time liegt).
 *
 * @return FLUX_OK; FLUX_ERR_INVALID bei NULL; FLUX_ERR_NO_DATA wenn die
 *         Spur weder Uebergaenge noch Indexmarken traegt.
 */
flux_status_t flux_raw_reverse(flux_raw_data_t *raw);

/** @brief Release a flux_raw_data_t built by flux_raw_from_ns_intervals(). */
void flux_raw_free(flux_raw_data_t *raw);

flux_status_t flux_decode_amiga_bits(const uint8_t *bits, size_t bit_count,
                                     flux_decoded_track_t *track,
                                     const flux_decoder_options_t *opts);

flux_status_t flux_decode_amiga(const flux_raw_data_t *flux,
                                flux_decoded_track_t *track,
                                const flux_decoder_options_t *opts);

/* ============================================================================
 * Format-Specific Decoders
 * ============================================================================ */

/**
 * @brief Decode SCP file to disk image
 */
flux_status_t flux_decode_scp_file(const char *path,
                                   uft_disk_image_t **out_disk,
                                   const flux_decoder_options_t *opts);

/**
 * @brief Decode KryoFlux stream files to disk image
 */
flux_status_t flux_decode_kryoflux_files(const char *base_path,
                                         uft_disk_image_t **out_disk,
                                         const flux_decoder_options_t *opts);

/**
 * @brief Decode DFI file to disk image
 */
flux_status_t flux_decode_dfi_file(const char *path,
                                   uft_disk_image_t **out_disk,
                                   const flux_decoder_options_t *opts);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Detect encoding from flux data
 */
flux_encoding_t flux_detect_encoding(const flux_raw_data_t *flux);

/**
 * @brief Calculate CRC-16 CCITT
 */
uint16_t flux_crc16_ccitt(const uint8_t *data, size_t len);

/**
 * @brief Calculate CRC-16 for MFM (init=0xFFFF, poly=0x1021)
 */
uint16_t flux_crc16_mfm(const uint8_t *data, size_t len);

/**
 * @brief MFM decode byte pair to data byte
 */
uint8_t flux_mfm_decode_byte(uint16_t mfm_word);

/**
 * @brief MFM encode data byte to byte pair
 */
uint16_t flux_mfm_encode_byte(uint8_t data, bool prev_bit);

/**
 * @brief FM decode byte
 */
uint8_t flux_fm_decode_byte(uint16_t fm_word);

/**
 * @brief Convert flux times to bit stream
 */
flux_status_t flux_to_bitstream(const flux_raw_data_t *flux,
                                uint8_t *bits, size_t *bit_count,
                                double bitcell_ns, flux_pll_t *pll);

/**
 * @brief Find sync pattern in bitstream
 */
int flux_find_sync(const uint8_t *bits, size_t bit_count,
                   uint16_t pattern, size_t start_pos);

/**
 * @brief Erste Fundstelle IRGENDEINES der Muster (MF-453)
 *
 * Ein Durchlauf, ein Schieberegister, alle Muster pro Position verglichen —
 * so wie X-Copy es macht (`xcop.s:2119-2138`: rol.l plus sechs Vergleiche).
 * N-mal flux_find_sync() zu rufen waere N Durchlaeufe und wuerde ausserdem
 * den fruehesten Treffer verfehlen, wenn ein spaeteres Muster frueher steht.
 *
 * @param which  optional: Index des getroffenen Musters in @p patterns
 * @return Bitposition des Treffers, oder -1
 */
int flux_find_sync_any(const uint8_t *bits, size_t bit_count,
                       const uint16_t *patterns, size_t pattern_count,
                       size_t start_pos, size_t *which);

/**
 * @brief Get sector size from size code
 */
static inline size_t flux_sector_size(uint8_t size_code) {
    return 128u << (size_code & 3);
}

/**
 * @brief Get encoding name
 */
const char* flux_encoding_name(flux_encoding_t enc);

/**
 * @brief Get status name
 */
const char* flux_status_name(flux_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLUX_DECODER_H */
