/**
 * @file uft_dms_plugin.c
 * @brief DMS (Disk Masher System) Plugin-B — Amiga compressed disk format
 *
 * DMS header: "DMS!" magic at offset 0, followed by 36-byte info header.
 * Track records follow, each with compressed or uncompressed data.
 *
 * MF-837: Dieses Plugin enthielt eine EIGENE, unabhaengige
 * Dekompression — und las den Track-Kopf vollstaendig falsch:
 *
 *   Byte    xDMS / uft_dms.c        was hier stand
 *   -----   ---------------------   -----------------------------
 *   2-3     number                  trk_num              (richtig)
 *   10-11   unpklen                 comp_mode            FALSCH
 *   12/13   flags / cmode           \
 *   14-15   usum                     > unpacked_size BE32 FALSCH
 *   16-17   dcrc                    \ packed_size BE32    FALSCH
 *   18-19   Kopf-CRC                /
 *
 * Nur die Tracknummer stimmte. `comp_mode` kam aus `unpklen` — auf einer
 * Amiga-Spur typisch 5632 (0x1600) —, traf damit den `default:`-Zweig und
 * liess die Spur bei ihrer 0xE5-Fuellung stehen. Der Kommentar dort nannte
 * das „forensic integrity"; 0xE5 ist die Formatfuellung von AmigaDOS, ein
 * Fehlschlag war also von einer leeren, formatierten Diskette nicht zu
 * unterscheiden. Das ist das Gegenteil von Integritaet: der Verlust wurde
 * als Datum ausgegeben.
 *
 * Zusaetzlich fehlten: die RLE-ZWEITstufe (Modi 2/3/4 sind immer
 * zweistufig), die Betriebsart 6 (HEAVY2), alle vier
 * Integritaetspruefungen, `flags & 1` (Woerterbuch ueber Trackgrenzen),
 * `flags & 2` (Huffman-Baeume des Vortracks), `flags & 4` und der
 * Heavy1/Heavy2-Fensterunterschied — und die Modi 4/5 waren vertauscht
 * (4 ist DEEP, 5 ist HEAVY1; xDMS 1.3, `pfile.c:39`:
 * `{"NOCOMP","SIMPLE","QUICK ","MEDIUM","DEEP  ","HEAVY1","HEAVY2"}`).
 *
 * Behoben durch ENTFERNEN, nicht durch Nachbessern: `uft_dms.c` im
 * gleichen Verzeichnis ist die gegen xDMS 1.3 (Andre Rodrigues de la
 * Rocha, 24.03.1999) verifizierte Portierung, reentrant ueber
 * `dms_ctx_t`, mit allen sieben Betriebsarten, beiden Entpackstufen, den
 * drei Tracklaengen und allen vier Pruefungen. Sie hatte ausserhalb
 * ihres Verzeichnisses genau EINEN Aufrufer, und der war ein Test —
 * kein Produktionspfad. Dieses Plugin ruft sie jetzt auf.
 *
 * Amiga disk geometry: 80 cyl x 2 heads x 11 spt x 512 = 901120 bytes
 * HD (geninfo Bit 4): das Doppelte.
 *
 * ── A-026: drei Tueren ohne Leser, und zwei haetten Befunde gemeldet ─────
 *
 * Anlass war ein vollstaendiger Abgleich gegen den Quellstand von
 * **xDMS 1.3.2** (Tarball 43 010 Byte, sha256 367ec4f02dd6a3a2...,
 * 1890 Zeilen `.c` in 12 Dateien; `COPYING` woertlich: „xdms is licensed
 * under PUBLIC DOMAIN"). Der Port selbst ist dabei TREU: `dms_read_info()`
 * liest dieselben acht Kopffelder an denselben acht Versaetzen wie
 * `pfile.c`, samt der 3-Byte-Lesart von `pkfsize` (Byte 21-23) und
 * `unpkfsize` (25-27). Und der Fassungsunterschied 1.3 -> 1.3.2 ist
 * gemessen klein: 1.3.1 war reine Portierung (C99-`stdint.h`,
 * `tmpnam()` -> `mkstemp()`), 1.3.2 fuegte **genau eine** Sache hinzu —
 * `-f`, „override errors … for desperate data salvation", und das ist
 * in `dms_unpack()`s `override_errors` schon da.
 *
 * Gefunden wurden stattdessen drei Funktionen der Bibliothek, die in
 * `src/` **je 0 Aufrufer ausserhalb ihrer eigenen Definition** hatten
 * (`dms_is_dms`, `dms_disk_type_name`, `dms_comp_mode_name`) — und zwei
 * Uebergaben, an denen dieses Plugin `NULL` stehen liess:
 *
 *   **(1) `track_cb` = NULL.** Damit wusste das Plugin nur, WIE WEIT der
 *   Entpacker kam, nicht WELCHE Spursaetze faul waren. Siehe den Block
 *   an `satz_faul` unten: gemessen meldeten 1760 von 1760 Sektoren
 *   `UFT_SECTOR_OK`, waehrend einer verfaelschte Bytes trug. BEHOBEN.
 *
 *   **(2) Die Sonde verglich vier Byte und meldete 98.** Siehe den Block
 *   an `dms_probe()`. BEHOBEN, und die Zahl ist jetzt abgeleitet
 *   (MF-1153; `docs/sondendoktrin_baseline.txt` faellt um eins).
 *
 * `dms_is_dms()` bleibt ohne Aufrufer, und das ist ABSICHT statt
 * Versaeumnis: es ist genau der Kennung-und-Kopf-CRC-Teil von
 * `dms_read_info()`, das die Sonde jetzt ruft. Einen redundanten Aufruf
 * einzufuegen, nur damit ein Symbol einen Aufrufer hat, waere die Zahl
 * als Motiv (MF-1077). Die Doppelung ist als solche benannt (D3).
 *
 * **Was der Abgleich NICHT behoben hat, und es ist benannt statt
 * verschwiegen** (Einzelheiten in `docs/OPEN_ITEMS.md` `P3-478`):
 * ein verschluesseltes Archiv bekommt die falsche Diagnose, weil
 * `password` fest `NULL` ist; Banner und FILEID.DIZ werden verworfen,
 * weil `info` fest `NULL` ist; bei einem HD-Archiv ist die obere
 * Haelfte ueber `read_track()` unerreichbar; und xDMS liest **8 von 19**
 * Kopffeldern, waehrend eine zweite unabhaengige Beschreibung
 * (Laurent Clevy, `DMS.txt`) CPU, Coprozessor, Maschinentyp,
 * OS-Fassung und Taktrate darin benennt.
 *
 * Reference: xDMS **1.3.2** (Rocha 1998-1999, gepflegt von Heikki
 * Orsila; Public Domain) ueber src/formats/dms/uft_dms.c — der
 * Quellstand ist gelesen, nicht uebernommen; die Portierung selbst ist
 * aelter und stammt aus 1.3.
 */
#include "uft/uft_format_common.h"
#include "uft/formats/uft_dms.h"
#include "uft/uft_log.h"

#define DMS_MAGIC       "DMS!"
#define DMS_HEADER_SIZE 56          /* 4 magic + 52 info header */
#define DMS_TRACK_HDR   20          /* track record header size */
#define AMIGA_TRACK_SIZE (11 * 512) /* 5632 bytes per track */
#define AMIGA_CYL       80
#define AMIGA_HEADS      2
#define AMIGA_SPT       11
#define AMIGA_SS        512
/* A-026: ein DMS-Spursatz umfasst BEIDE Koepfe eines Zylinders. Gemessen
 * am Korpusstueck traegt jeder der 80 Saetze `unpklen = 11 264`, und
 * 11 264 = 2 x 11 x 512; 80 x 11 264 = 901 120. Die Zahl steht deshalb
 * nicht als Konstante da, sondern als dieser Faktor. */
#define KOPF_JE_SATZ     2

typedef struct {
    uint8_t *adf;       /* Decompressed raw ADF image */
    size_t   adf_size;

    /* MF-1135: WIE WEIT der Entpacker wirklich gekommen ist.
     *
     * `open()` wusste das schon — `dms_unpack()` gibt es in `written`
     * zurueck, und der Fehlertoleranz-Zweig schreibt es sogar in die
     * Warnung („N von M Byte wiederhergestellt, Integritaet NICHT
     * bestaetigt — der Rest bleibt 0xE5"). Die Zahl wurde danach
     * verworfen.
     *
     * Damit sah ein Aufrufer, der `track->sectors[]` liest, gute
     * Sektoren mit 0xE5-Fuellung: `uft_format_add_sector()` setzt
     * unbedingt `UFT_SECTOR_OK`. Gemessen an einer gedrittelten Datei
     * kamen 292 864 von 901 120 Byte zurueck, und Spur 70/1 — die es in
     * der Datei nicht mehr gibt — meldete elf gute Sektoren.
     *
     * Das ist die Klasse, die dieser Baum viermal behoben hat: MF-1001
     * („gefuellt, nicht gelesen"), MF-1022 (`sap`s Fuellsektor galt als
     * guter Sektor mit gueltiger CRC), MF-1038 (`fds`, 36 erfundene Byte
     * je Seite als `UFT_SECTOR_OK`) und MF-980 („das Format sagt 0xE5"
     * und „hier wurde 0xE5 gelesen" sind zwei Aussagen).
     *
     * Der Dateikopf oben sagt es selbst ueber den Vorzustand: „der
     * Verlust wurde als Datum ausgegeben". Die Warnung hat den Bediener
     * erreicht, die Datenstruktur nicht. */
    size_t   gelesen;

    /* A-026: WELCHE Spursaetze faul waren — und das faengt den Fall, den
     * `gelesen` nicht sehen kann.
     *
     * `gelesen` ist eine GRENZE und traegt damit nur die abgeschnittene
     * Datei. Im toleranten Lauf laufen im Entpacker aber alle drei
     * Pruefungen durch (Daten-CRC, Entpackfehler, Pruefsumme) und die
     * Spur wird trotzdem geschrieben — `out_pos` waechst mit. Bei einem
     * LOCH IN DER MITTE ist `gelesen == adf_size`, und die Grenze deckt
     * das Loch mit ab.
     *
     * Gemessen am Vorzustand, an einer DMS mit EINEM gekippten Byte im
     * Spursatz 40 (alle 80 Spurkoepfe unveraendert): 1760 Sektoren, davon
     * **1760 mit UFT_SECTOR_OK und 0 gekennzeichnet**, waehrend die
     * Selbstbenennung nur noch 1759-mal trifft — ein Sektor trug
     * verfaelschte Bytes und meldete sich als guter Sektor mit gueltigen
     * CRC-Flags. Siebter Fall der Klasse MF-1001/MF-1022/MF-1038/
     * MF-980/MF-1040/MF-1135.
     *
     * Der Mechanismus dafuer lag die ganze Zeit im Baum und wurde nicht
     * gerufen: `dms_unpack()` nimmt einen Spur-Callback, und
     * `dms_track_info_t` traegt `crc_ok`, `checksum_ok` und (seit A-026)
     * `decomp_ok` JE SPURSATZ. Dieses Plugin uebergab dort `NULL`.
     * Klasse MF-930/P3-204.
     *
     * Ein DMS-Spursatz ist ein ZYLINDER, nicht eine Kopfspur: sein
     * `unpklen` ist gemessen 11 264 = 2 x 11 x 512. Deshalb ist der
     * Index hier der Zylinder, und eine faule Angabe kennzeichnet BEIDE
     * Koepfe — feiner kann sie nicht sein, weil der Entpacker keine
     * feinere Aussage macht. */
    uint8_t *satz_faul;     /* je Zylinder: 1 = nicht bestaetigt */
    size_t   saetze;        /* Laenge von satz_faul */
} dms_pd_t;

/* Sammelt je Spursatz, ob ALLE drei Integritaetsangaben bestaetigt sind.
 * `checksum_ok` und `decomp_ok` heissen seit A-026 „geprueft und gut" —
 * 0 deckt also auch „nicht geprueft" ab, und genau so soll es sein. */
static void dms_satz_gesehen(const dms_track_info_t *ti, void *user)
{
    dms_pd_t *p = (dms_pd_t *)user;
    if (!p || !p->satz_faul) return;
    if ((size_t)ti->number >= p->saetze) return;   /* Sondernummern (80, 0xffff) */
    if (!ti->crc_ok || !ti->decomp_ok || !ti->checksum_ok)
        p->satz_faul[ti->number] = 1;
}

/* ========================================================================= */

/**
 * @brief Erkennt eine DMS — und sagt nicht mehr, als sie gelesen hat.
 *
 * A-026. Vorzustand, gemessen: die Sonde verglich **vier Byte** und
 * meldete `98`. Eine Datei, die nur aus `DMS!` besteht, bekam damit
 * dieselbe Zahl wie ein vollstaendiges, in sich stimmiges Archiv. Zwei
 * Faellen des Baums auf einmal:
 *
 *   * `(void)file_size` — die Dateigroesse wurde verworfen, obwohl der
 *     Kopf sie nachrechenbar macht. Das ist die MF-1029-Falle, die
 *     dieser Baum sechsmal gemessen hat;
 *   * `dms_is_dms()` prueft die Kennung UND den Kopf-CRC, lag im
 *     Nachbarmodul und wurde nicht gerufen (MF-930/P3-204). Diese Sonde
 *     ruft jetzt `dms_read_info()`, das dieselbe Pruefung macht und
 *     zusaetzlich die Felder liefert — `dms_is_dms()` ist damit als
 *     Doppelung erkennbar (D3).
 *
 * Die Zahl ist seit A-026 nach `docs/SONDEN_DOKTRIN.md` ABGELEITET
 * (MF-1153), nicht vergeben, und jede Stufe haengt an einer am
 * Korpusstueck gemessenen Beziehung:
 *
 *   KENNUNG (+50)          `"DMS!"` an Versatz 0, formatspezifisch
 *   STRUKTUR (+15)         der Kopf-CRC bei Byte 54-55 ueber 4..53 —
 *                          ein Wert an BERECHNETER Stelle, 0xF0DC am
 *                          Korpusstueck
 *   SELBSTKONSISTENZ (+25) `56 + pkfsize + 20 * Spursaetze` trifft die
 *                          Dateigroesse. Gemessen:
 *                          56 + 38 720 + 80 x 20 = **40 376**, und
 *                          38 720 ist die Summe aller `pklen1`. Drei
 *                          Kopffelder gegen die Datei, nicht die
 *                          Groesse allein
 *   GEOMETRIE (+10)        `unpkfsize == Spursaetze * Satzgroesse`;
 *                          gemessen 80 x 11 264 = **901 120**
 *
 * **Die Selbstkonsistenz wird bei gesetztem `Appends`-Flag NICHT
 * beansprucht**, und das ist keine Vorsicht, sondern steht in der
 * Quelle: xDMS' `pfile.c` sagt zu `from`/`to` woertlich „May be
 * incorrect if archive is appended". Eine Rechnung mit einem Feld, das
 * die Quelle selbst als moeglicherweise falsch benennt, waere kein
 * Beleg.
 *
 * Ein DMS-Spursatz ist ein ZYLINDER (gemessen `unpklen = 11 264 =
 * 2 x 11 x 512`), und `geninfo` Bit 4 verdoppelt ihn — dieselbe
 * Unterscheidung, die `open()` fuer `adf_size` schon trifft.
 */
static bool dms_probe(const uint8_t *data, size_t size, size_t file_size,
                       int *confidence)
{
    if (!data || size < 4) return false;
    if (memcmp(data, DMS_MAGIC, 4) != 0) return false;

    unsigned belege = UFT_BELEG_KENNUNG;

    dms_info_t info;
    if (size >= DMS_HEADER_SIZE &&
        dms_read_info(data, size, &info) == DMS_OK) {

        /* Der Kopf-CRC hat gestimmt — sonst waere `dms_read_info()` mit
         * `DMS_ERR_HEADER_CRC` zurueckgekommen. */
        belege |= UFT_BELEG_STRUKTUR;

        const uint32_t saetze = (info.track_hi >= info.track_lo)
            ? (uint32_t)(info.track_hi - info.track_lo + 1u) : 0u;

        if (saetze != 0u && file_size != 0u &&
            !(info.geninfo & DMS_INFO_APPENDS)) {
            const uint64_t erwartet = (uint64_t)DMS_HEADER_SIZE
                                    + (uint64_t)info.packed_size
                                    + (uint64_t)DMS_TRACK_HDR * saetze;
            if (erwartet == (uint64_t)file_size)
                belege |= UFT_BELEG_SELBSTKONSISTENZ;
        }

        if (saetze != 0u) {
            const uint64_t satz = (info.geninfo & DMS_INFO_HD)
                ? 2u * (uint64_t)KOPF_JE_SATZ * AMIGA_TRACK_SIZE
                :      (uint64_t)KOPF_JE_SATZ * AMIGA_TRACK_SIZE;
            if ((uint64_t)info.unpacked_size == satz * (uint64_t)saetze)
                belege |= UFT_BELEG_GEOMETRIE;
        }

        dms_info_free(&info);
    }

    *confidence = uft_probe_konfidenz(belege);
    return true;
}

static uft_error_t dms_open(uft_disk_t *disk, const char *path, bool ro)
{
    (void)ro;
    size_t file_size = 0;
    uint8_t *raw = uft_read_file(path, &file_size);
    if (!raw) return UFT_ERROR_FILE_OPEN;

    /* MF-837: Kopf ueber die verifizierte Bibliothek lesen — sie prueft
     * dabei auch den Kopf-CRC (Byte 54-55 ueber 4..53), was diese Datei
     * vorher nie getan hat. */
    dms_info_t info;
    dms_error_t de = dms_read_info(raw, file_size, &info);
    if (de != DMS_OK) {
        UFT_WARN("DMS: Kopf nicht lesbar (%s)", dms_error_string(de));
        free(raw);
        return UFT_ERROR_FORMAT_INVALID;
    }

    /* HD-Archive (geninfo Bit 4) tragen 1760 KB, nicht 880 KB. Das
     * verdrahtete Plugin rechnete immer mit 880 KB. */
    size_t adf_size = (info.geninfo & DMS_INFO_HD)
                    ? 2u * (size_t)AMIGA_CYL * AMIGA_HEADS * AMIGA_TRACK_SIZE
                    :      (size_t)AMIGA_CYL * AMIGA_HEADS * AMIGA_TRACK_SIZE;

    uint8_t *adf = malloc(adf_size);
    if (!adf) { dms_info_free(&info); free(raw); return UFT_ERROR_NO_MEMORY; }
    memset(adf, 0xE5, adf_size);

    /* A-026: die Plugin-Daten stehen JETZT, nicht erst nach dem Entpacken
     * — der Spur-Callback schreibt seine Befunde hinein, waehrend
     * `dms_unpack()` laeuft. */
    dms_pd_t *p = calloc(1, sizeof(dms_pd_t));
    if (!p) { free(adf); dms_info_free(&info); free(raw); return UFT_ERROR_NO_MEMORY; }
    p->saetze    = AMIGA_CYL;                  /* ein Spursatz = ein Zylinder */
    p->satz_faul = calloc(p->saetze, 1);
    if (!p->satz_faul) {
        free(p); free(adf); dms_info_free(&info); free(raw);
        return UFT_ERROR_NO_MEMORY;
    }

    /* Erster Versuch STRENG: jede der vier Integritaetsangaben zaehlt.
     * Nur wenn das scheitert, wird mit `override_errors` erneut versucht
     * — dann sind die Daten da UND der Mangel ist benannt. Ein Befund
     * darf den Zugriff nicht verstellen (MF-830), aber er darf auch nicht
     * verschwiegen werden. */
    size_t written = 0;
    de = dms_unpack(raw, file_size, adf, adf_size, &written,
                    NULL, 0, NULL, dms_satz_gesehen, p);

    if (de != DMS_OK) {
        UFT_WARN("DMS: strenger Lauf abgebrochen (%s) — Wiederholung mit "
                 "Fehlertoleranz", dms_error_string(de));
        memset(adf, 0xE5, adf_size);
        /* A-026: die Befundtafel wird MIT dem Puffer zurueckgesetzt. Sonst
         * traege sie Angaben aus einem Lauf, dessen Daten verworfen sind. */
        memset(p->satz_faul, 0, p->saetze);
        written = 0;
        dms_error_t de2 = dms_unpack(raw, file_size, adf, adf_size, &written,
                                     NULL, 1, NULL, dms_satz_gesehen, p);
        if (de2 != DMS_OK || written == 0) {
            UFT_WARN("DMS: nichts wiederherstellbar (%s) — Datei wird NICHT "
                     "als leere Diskette ausgegeben",
                     dms_error_string(de2 != DMS_OK ? de2 : de));
            free(p->satz_faul);
            free(p);
            free(adf);
            dms_info_free(&info);
            free(raw);
            return UFT_ERROR_FORMAT_INVALID;
        }
        /* A-026: die Warnung nennt jetzt auch, WIE VIELE Spursaetze
         * unbestaetigt sind — „901 120 von 901 120 Byte wiederhergestellt"
         * allein liest sich wie ein Erfolg. */
        size_t faul = 0;
        for (size_t i = 0; i < p->saetze; i++) faul += p->satz_faul[i] ? 1u : 0u;
        UFT_WARN("DMS: %zu von %zu Byte wiederhergestellt, %zu von %zu "
                 "Spursaetzen NICHT bestaetigt — der Rest bleibt 0xE5",
                 written, adf_size, faul, p->saetze);
    }

    dms_info_free(&info);
    free(raw);

    p->adf = adf;
    p->adf_size = adf_size;
    /* MF-1135: die Grenze zwischen GELESEN und GEFUELLT wird behalten.
     * Bei einem strengen Erfolg (`de == DMS_OK`) hat `dms_unpack()`
     * bis `written` geschrieben und alles bestaetigt; im
     * Fehlertoleranz-Zweig ist `written` genau die Stelle, ab der
     * 0xE5-Fuellung steht. */
    p->gelesen = written;

    disk->plugin_data = p;
    disk->geometry.cylinders = AMIGA_CYL;
    disk->geometry.heads = AMIGA_HEADS;
    disk->geometry.sectors = AMIGA_SPT;
    disk->geometry.sector_size = AMIGA_SS;
    disk->geometry.total_sectors =
        (uint32_t)(adf_size / AMIGA_SS);
    return UFT_OK;
}

static void dms_close(uft_disk_t *disk)
{
    dms_pd_t *p = disk->plugin_data;
    if (p) {
        free(p->adf);
        free(p->satz_faul);     /* A-026 */
        free(p);
        disk->plugin_data = NULL;
    }
}

static uft_error_t dms_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track)
{
    dms_pd_t *p = disk->plugin_data;
    if (!p || !p->adf) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    size_t trk_off = ((size_t)cyl * AMIGA_HEADS + head) * AMIGA_TRACK_SIZE;
    for (int s = 0; s < AMIGA_SPT; s++) {
        size_t soff = trk_off + (size_t)s * AMIGA_SS;
        if (soff + AMIGA_SS > p->adf_size) break;
        uft_format_add_sector(track, (uint8_t)s, p->adf + soff,
                              AMIGA_SS, (uint8_t)cyl, (uint8_t)head);

        /* MF-1135: liegt der Sektor GANZ ODER TEILWEISE hinter dem, was
         * der Entpacker wirklich geschrieben hat, dann traegt er
         * 0xE5-Fuellung und keine gelesenen Daten. Er wird deshalb
         * gekennzeichnet.
         *
         * `uft_format_add_sector()` setzt unbedingt `UFT_SECTOR_OK` (das
         * sagt sein eigener Kopf), also muss die Kennzeichnung DANACH
         * kommen. Ohne sie war eine erfundene 0xE5-Flaeche von einer
         * echten AmigaDOS-Formatfuellung nicht zu unterscheiden — und
         * genau das nennt der Dateikopf oben als den behobenen Fehler
         * des Vorgaengers: „der Verlust wurde als Datum ausgegeben".
         *
         * Die Daten bleiben stehen; ein Befund darf den Zugriff nicht
         * verstellen (MF-830). Er darf nur nicht verschwiegen werden. */
        if (soff + AMIGA_SS > p->gelesen && track->sector_count > 0) {
            uft_sector_mark_missing(&track->sectors[track->sector_count - 1]);
        }

        /* A-026: und zusaetzlich — nicht stattdessen — die Angabe des
         * Entpackers ueber DIESEN Spursatz. `p->gelesen` ist eine Grenze
         * und sieht nur das Ende; ein Loch in der Mitte liegt davor und
         * bliebe unbemerkt (gemessen: 1760 von 1760 Sektoren meldeten
         * UFT_SECTOR_OK, waehrend ein Sektor verfaelschte Bytes trug).
         *
         * Beide Kennzeichnungen sind noetig und keine ersetzt die andere:
         * die Grenze traegt die ABGESCHNITTENE Datei, fuer die es gar
         * keinen Spursatz mehr gibt, ueber den der Callback etwas sagen
         * koennte; die Tafel traegt das LOCH in einer vollstaendigen
         * Datei.
         *
         * Die Daten bleiben stehen (MF-830) — gekennzeichnet wird, nicht
         * verschwiegen und nicht verstellt. */
        if (p->satz_faul && (size_t)cyl < p->saetze && p->satz_faul[cyl]
            && track->sector_count > 0) {
            uft_sector_mark_missing(&track->sectors[track->sector_count - 1]);
        }
    }
    return UFT_OK;
}

/* Write track: modifies decompressed ADF buffer in memory.
 * The original DMS file is NOT modified (re-compression not supported).
 * This enables format conversion workflows (read DMS -> modify -> write as ADF). */
static uft_error_t dms_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track)
{
    dms_pd_t *p = disk->plugin_data;
    if (!p || !p->adf) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    /* MF-883: Hier stand eine Speicher-Mutation, die `UFT_OK` meldete.
     *
     * Gemessen: in dieser Datei steht keine einzige Schreiboperation
     * (`fwrite`/`fputc`/`fprintf`/`ftruncate`/`WriteFile`), das Plugin hat
     * kein `.flush`, und `close()` gibt den Puffer frei. Kein Byte hat je
     * die Platte erreicht — der Aufrufer bekam Erfolg gemeldet.
     *
     * Und es gibt auch keinen allgemeinen Rueckweg: `plugin->flush` wird im
     * ganzen Baum von NIEMANDEM gerufen (gemessen ueber `git ls-files`,
     * kommentarfrei), `uft_disk_close()` ruft nur `close`. Selbst ein
     * Plugin MIT Flush kaeme nicht durch.
     *
     * Betroffen war auch der Wandlungspfad: `uft_disk_convert.c:41` zaehlt
     * `tracks_converted++` bei `UFT_OK` und schreibt danach nichts hinaus.
     *
     * Warum kein echter Schreiber gebaut wurde: die EINFRIER-REGEL
     * (MF-363/498) verlangt benannte Referenz, gemessene Zahlen und die
     * Referenz im Header. Neun Container-Schreiber gegen diese Lage waeren
     * neun Wetten. Die Zusage wahr zu machen ist der kleinere und richtige
     * Schritt — dieselbe Entscheidung wie MF-880 (PRO).
     *
     * `write_track` bleibt GESETZT statt NULL: ein Nullzeiger gaebe dem
     * Aufrufer keine Begruendung. */
    (void)track;
    (void)cyl;      /* A-026: vorbestehende -Wextra-Warnungen, D7 */
    (void)head;
    return UFT_ERROR_NOT_SUPPORTED;
}

static const uft_plugin_feature_t uft_format_plugin_dms_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_UNSUPPORTED,
      "MF-883: schreibt nur in den Speicher — kein fwrite in der Datei, kein flush, close() gibt frei" },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_dms = {
    .name = "DMS", .description = "Amiga DMS (Disk Masher System)",
    .extensions = "dms", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_VERIFY,
    .probe = dms_probe, .open = dms_open, .close = dms_close,
    .read_track = dms_read_track, .write_track = dms_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_dms_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_dms_features) / sizeof(uft_format_plugin_dms_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(dms)
