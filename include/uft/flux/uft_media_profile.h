/**
 * @file uft_media_profile.h
 * @brief Bitzellendauer aus Medienprofil + GEMESSENER Umdrehungsdauer (MF-471).
 *
 * Das Problem, in einem Satz: **die Drehzahl beim Lesen bestimmt das Laufwerk,
 * nicht die Diskette.** Eine Atari-Diskette wurde mit 288 min⁻¹ beschrieben;
 * liest man sie in einem 300-min⁻¹-Laufwerk, laufen dieselben Bitzellen um
 * den Faktor 288/300 = 0,96 schneller vorbei. Wer mit der nominalen Zellen-
 * dauer des Mediums dekodiert, liegt um 4 % daneben — genug, damit ein
 * PLL-Fenster von ±20 % am Rand kippt.
 *
 * Der Ausweg braucht die Laufwerksdrehzahl gar nicht zu kennen: sie steckt
 * bereits in der gemessenen Umdrehungsdauer des Rohabbilds.
 *
 *     Zellen je Umdrehung  =  Datenrate × 60 / Medien-Drehzahl     (Profil)
 *     Zellendauer          =  gemessene Umdrehungsdauer / Zellen je Umdrehung
 *
 * Das ist der Rechenweg von a8rawconv 0.95 (`src/a8rawconv/`, Referenz-Orakel,
 * wird nicht gebaut):
 *
 *     a8rawconv.cpp:133-136
 *       // Atari disk timing produces 250,000 clocks per second at 288 RPM.
 *       // We must compute the effective sample rate given the actual disk rate.
 *       const double cells_per_rev = 250000.0 / (288.0 / 60.0) * (hd ? 2 : 1);
 *       double scks_per_cell = rawTrack.mSamplesPerRev / cells_per_rev
 *                              * g_clockPeriodAdjust;
 *
 * `mSamplesPerRev` ist dort gemessen, nicht angenommen — aus den Index-Marken
 * des Abbilds (`rawdiskkf.cpp:203-206` für KryoFlux-Ströme,
 * `rawdiskscp.cpp:167-175` für SCP).
 *
 * `g_clockPeriodAdjust` ist der Faktor aus a8rawconvs `-p`: ein
 * Feineinsteller in Prozent, mit dem sich marginale Disketten oft noch lesen
 * lassen (das Handbuch empfiehlt 95…105 in kleinen Schritten). Er steht hier
 * im selben Ausdruck, weil er dort steht — ihn wegzulassen hiesse, die Formel
 * halb zu übernehmen.
 *
 * **Grenze, ausdrücklich:** dieses Modul rechnet, es dekodiert nicht. Es
 * kennt keine Kopierschutz-Varianten, keine Zonen-Aufzeichnung (Commodore,
 * Apple: dort gilt die Rechnung je Zone, nicht je Diskette) und keinen
 * Drehzahl-Schlupf innerhalb einer Umdrehung.
 */
#ifndef UFT_MEDIA_PROFILE_H
#define UFT_MEDIA_PROFILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Womit das Medium beschrieben wurde — bestimmt Datenrate und Drehzahl. */
typedef enum {
    UFT_MEDIA_UNKNOWN = 0,
    UFT_MEDIA_ATARI_FM,     /**< Atari 810/1050 SD+ED, 288 min⁻¹, 4 µs   */
    UFT_MEDIA_ATARI_MFM,    /**< Atari DD (XF551 u. a.), 288 min⁻¹, 2 µs */
    UFT_MEDIA_PC_360K,      /**< 5,25" DD,   300 min⁻¹, 2 µs             */
    UFT_MEDIA_PC_12M,       /**< 5,25" HD,   360 min⁻¹, 1 µs             */
    UFT_MEDIA_PC_720K,      /**< 3,5"  DD,   300 min⁻¹, 2 µs             */
    UFT_MEDIA_PC_144M,      /**< 3,5"  HD,   300 min⁻¹, 1 µs             */
    UFT_MEDIA_AMIGA_DD,     /**< AmigaDOS DD, 300 min⁻¹, 2 µs            */
    UFT_MEDIA_APPLE2_GCR,   /**< Apple II 5,25", 300 min⁻¹, 4 µs         */
    UFT_MEDIA_COUNT
} uft_media_kind_t;

/**
 * @brief Liegen die Sektoren fest zum Indexloch? (MF-483)
 *
 * Entscheidet, ob EINE Umdrehung genügt. Bei einem nicht indexsynchronen
 * Format kann ein Sektor über die Indexmarke hinauslaufen; ein
 * Ein-Umdrehungs-Abbild schneidet ihn dann mitten durch, und zwar
 * **unauffällig** — es fehlt einfach ein Sektor, so wie bei einem Lesefehler.
 *
 * a8rawconv sagt das für Atari wörtlich (`rawdiskscp.cpp:120-124`,
 * Referenz-Orakel, wird nicht gebaut):
 *
 *     if (fileHeader.mNumRevs <= 1) {
 *         printf("Warning: Only one disk revolution found in image. Atari
 *                 disks are not index aligned and require at least two
 *                 revolutions.\n");
 *     }
 *
 * Dort steht die Warnung fest verdrahtet, weil a8rawconv nur Atari kennt.
 * UFT kennt viele Formate, also gehört die Eigenschaft ins Profil — sonst
 * würde entweder bei jedem Format gewarnt oder bei keinem.
 */
typedef enum {
    /** Nicht belegt. Es wird NICHT gewarnt: eine Warnung ohne Beleg ist
     *  eine erfundene Aussage über das Medium. */
    UFT_IDXSYNC_UNKNOWN = 0,
    /** Sektorlage ist an die Indexmarke gebunden (IBM System 34 hat eine
     *  Index-Adressmarke am Spuranfang). Eine Umdrehung genügt. */
    UFT_IDXSYNC_INDEXED,
    /** Sektoren liegen frei zur Indexmarke. Mindestens zwei Umdrehungen. */
    UFT_IDXSYNC_NONE
} uft_index_sync_t;

/**
 * @brief Nominale Eigenschaften eines Mediums — was beim SCHREIBEN galt.
 *
 * Bewusst nicht dasselbe wie `uft_drive_profile_t`: das beschreibt ein
 * Laufwerk (Schrittzeit, Anfahrzeit, Zylinderzahl), dies beschreibt die
 * Aufzeichnung auf der Diskette. Beide zusammen ergeben erst den Fall, um
 * den es hier geht — Medium des einen Systems, Laufwerk eines anderen.
 */
typedef struct {
    uft_media_kind_t kind;
    const char *name;
    double       rpm;          /**< Drehzahl beim Schreiben (min⁻¹)      */
    double       data_rate_hz; /**< Bitzellen je Sekunde                 */
    double       bitcell_ns;   /**< nominale Zellendauer bei @ref rpm    */

    /* MF-483 — angehaengt, nicht eingefuegt. */

    /** Ob die Sektorlage an die Indexmarke gebunden ist. Je Profil belegt;
     *  wo kein Beleg vorliegt, bleibt es UNKNOWN. */
    uft_index_sync_t index_sync;
} uft_media_profile_t;

/**
 * @brief Wie viele Umdrehungen ein Abbild dieses Mediums mindestens braucht.
 *
 * ABGELEITET aus @ref uft_media_profile_t::index_sync, nicht daneben
 * gespeichert: zwei Felder, die dasselbe sagen, laufen irgendwann
 * auseinander — und dann gilt das, was zufällig gelesen wird. Genau diese
 * Klasse Fehler hat dieses Projekt mehrfach getroffen (MF-475, MF-479,
 * MF-481).
 *
 * @return 2 wenn das Medium nicht indexsynchron ist, sonst 1.
 *         Auch bei UNKNOWN 1 — geraten wird nicht.
 */
int uft_media_min_revolutions(uft_media_kind_t kind);

/** @return Profil oder NULL, wenn @p kind unbekannt ist. */
const uft_media_profile_t *uft_media_profile(uft_media_kind_t kind);

/** Anzahl der Profile in der Tabelle (ohne UFT_MEDIA_UNKNOWN). */
size_t uft_media_profile_count(void);

/**
 * @brief Bitzellen je Umdrehung, wie das Medium beschrieben wurde.
 *
 * `Datenrate × 60 / Drehzahl` — die linke Hälfte von a8rawconv.cpp:135.
 *
 * @return Zellen je Umdrehung, oder 0.0 bei unbekanntem Profil.
 */
double uft_media_cells_per_rev(uft_media_kind_t kind);

/**
 * @brief Tatsächliche Zellendauer aus der gemessenen Umdrehungsdauer.
 *
 * Der Kern des Ganzen. @p rev_ns kommt aus dem Abbild (Abstand zweier
 * Index-Impulse), nicht aus einer Annahme über das Laufwerk.
 *
 * @param kind         Medienprofil
 * @param rev_ns       gemessene Dauer EINER Umdrehung, in Nanosekunden
 * @param adjust_pct   Feineinsteller in Prozent; 100 = unverändert.
 *                     Entspricht a8rawconvs `-p` (dort 50…200).
 * @param out_cell_ns  Ergebnis: Zellendauer in Nanosekunden
 * @return true bei Erfolg; false bei unbekanntem Profil, nicht-positiver
 *         Umdrehungsdauer oder @p adjust_pct ausserhalb 50…200.
 *
 * Fehlschlag lässt @p out_cell_ns unberührt — der Aufrufer bekommt keine
 * halb berechnete Zahl, die er für eine Messung halten könnte.
 */
bool uft_media_cell_ns_from_rev(uft_media_kind_t kind,
                                double rev_ns,
                                double adjust_pct,
                                double *out_cell_ns);

/**
 * @brief Umdrehungsdauer aus Index-Impulsen, gemittelt.
 *
 * Wie `rawdiskkf.cpp:204` — Abstand des ersten zum letzten Index geteilt
 * durch die Zahl der Zwischenräume, nicht der Abstand des ersten Paares.
 * Ein einzelner Zwischenraum trägt den vollen Motor-Jitter; über N gemittelt
 * fällt er heraus.
 *
 * @param index_ticks  aufsteigende Index-Positionen in Sample-Ticks
 * @param count        Anzahl davon; unter 2 gibt es keine Umdrehung
 * @param sample_rate_hz  Abtastrate des Abbilds
 * @param out_rev_ns   Ergebnis in Nanosekunden
 * @return true bei Erfolg; false bei zu wenigen Impulsen, nicht
 *         aufsteigender Folge oder Abtastrate 0.
 */
bool uft_media_rev_ns_from_index(const uint32_t *index_ticks,
                                 size_t count,
                                 uint32_t sample_rate_hz,
                                 double *out_rev_ns);

/**
 * @brief Gemessene Drehzahl des LESENDEN Laufwerks (min⁻¹).
 *
 * Nur zur Anzeige und zur Plausibilitätsprüfung — für die Zellendauer wird
 * sie nicht gebraucht, die Umdrehungsdauer genügt. Wer sie ausgibt, sollte
 * dazusagen, dass sie das Laufwerk beschreibt und nicht das Medium.
 *
 * @return min⁻¹, oder 0.0 bei @p rev_ns <= 0.
 */
double uft_media_rpm_from_rev_ns(double rev_ns);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MEDIA_PROFILE_H */
