/**
 * @file uft_2img.c
 * @brief 2IMG / 2MG (Apple II Universal Disk Image) Plugin
 *
 * 2IMG umhuellt Apple-II-Sektordaten mit einem **64 Byte** langen Kopf.
 *
 * Kopf (64 Byte, alle Zahlen little-endian):
 *   0x00 4  Kennung `"2IMG"` — **oder `"GMI2"`**, siehe unten
 *   0x04 4  Erzeuger (4 ASCII-Zeichen)
 *   0x08 2  Kopfgroesse (64)
 *   0x0A 2  Version (1)
 *   0x0C 4  Anordnung (0 = DOS 3.3, 1 = ProDOS, 2 = NIB)
 *   0x10 1  DOS-3.3-Volumennummer
 *   0x11 1  Bit 0: Volumennummer gueltig
 *   0x14 4  ProDOS-Bloecke (0 = aus der Datenlaenge ableiten)
 *   0x18 4  Datenversatz
 *   0x1C 4  Datenlaenge
 *   0x20 4  Kommentar-Versatz (0 = keiner)
 *   0x24 4  Kommentar-Laenge
 *   0x28 4  Erzeuger-eigener Versatz
 *   0x2C 4  Erzeuger-eigene Laenge
 *   0x30-3F Reserviert
 *
 * ── Referenz ────────────────────────────────────────────────────────
 *
 * MAME `src/lib/formats/ap_dsk35.cpp` (**BSD-3-Clause**; **nur
 * gelesen**, Kanal *Spec* nach MF-695), Fassung aus `FORMATS.ZIP`:
 *
 *   Z. 452-459  `s_formats[]` — die **drei** gueltigen Kombinationen
 *               aus Anordnung und Datenlaenge, mit Formfaktor
 *   Z. 463-501  `identify()` — Kennung, Tafelabgleich, Groessenprobe
 *   Z. 503-580  `load()` — Geometrie, Zonentafel, Sektornummern
 *
 * Zweite, **unabhaengige** Hand: `hxcfe`s `APPLE2_2MG`-Modul (GPL-2,
 * nur **ausgefuehrt**). Es liest die Pruefdatei und gibt ihre 143 360
 * Nutzbytes ueber `-conv:APPLE2_DO` **byteidentisch** zurueck (560 von
 * 560 Sektoren). An zwei Stellen widerspricht es MAME, und **MAME
 * gewinnt mit Begruendung** — siehe B1 und B2.
 *
 * ── MF-1031: acht Befunde, und zwei davon trafen jede 3,5"-Datei ────
 *
 * **B1 — `"GMI2"` wurde abgewiesen.** MAME nimmt die **byte-vertauschte**
 * Kennung ausdruecklich an, mit benanntem Erzeuger: *„Bernie ][ The
 * Rescue wrote 2MGs with the signature byte-flipped, other fields are
 * valid"* (Z. 469-470). UFT verlangte `"2IMG"` in Sonde **und** `open`
 * und wies damit jede solche Datei ab. `hxcfe` weist sie ebenfalls ab —
 * **ohne Grund zu nennen**; MAME nennt den Erzeuger. Ein Orakel ist
 * eine Referenz, kein Beweis (MF-1015), und hier tragen die Gruende
 * ungleich.
 *
 * **B2 — die 3,5"-Geometrie war erfunden.** UFT rechnete fuer JEDE
 * Datei `35 Spuren x 16 Sektoren x 256 Byte`, einseitig, und leitete
 * die Spurzahl aus `Datenlaenge / 4096` ab. Eine 3,5"-Apple-Diskette
 * hat **512-Byte-Sektoren** und eine **Zonentafel**: `ns = 12 -
 * Spur/16` (MAME Z. 560), also 12/11/10/9/8 Sektoren je Spur. Gemessen
 * an einer 819 264 Byte grossen 2MG (1600 Bloecke): UFT meldete 80
 * Zylinder x 16 x 256 = **327 680 von 819 200 Byte**, der Rest war
 * nicht erreichbar — und die 1280 gemeldeten „Sektoren" waren
 * 256-Byte-Haelften von 512-Byte-Bloecken mit falschen Nummern.
 * `hxcfe` liegt hier ebenfalls falsch (gemessen: 133 Spuren x 12
 * Sektoren x 256 Byte); die Zonentafel bestaetigt sich dagegen **aus
 * sich selbst**: `16 x (12+11+10+9+8) x 512 = 409 600` und das Doppelte
 * `819 200` sind genau die zwei Laengen in MAMEs eigener Tafel.
 *
 * **B3 — eine byte-vertauschte Datenlaenge wurde nicht erkannt.**
 * Dieselbe Erzeuger-Klasse wie B1: MAME prueft
 * `format.data_length == swapendian_int32(data_length)` und **berichtigt**
 * den Wert (Z. 483-486). UFT rechnete mit dem vertauschten Wert weiter.
 *
 * **B4 — die Sonde verwarf die Dateigroesse** (`(void)file_size`).
 * Damit war MAMEs Struktur-Probe `data_offset + data_length ==
 * Dateigroesse` (Z. 497-499) nicht nachbaubar. Das ist die Falle aus
 * **MF-1029**, zum **fuenften** Mal in diesem Baum.
 *
 * **B5 — es gab keine Tafel.** Jede Datenlaenge wurde angenommen, die
 * Geometrie daraus geraten. MAME weist ab, was nicht in `s_formats[]`
 * steht. Eine 2MG mit 500 000 Nutzbytes wurde von UFT als 80-Zylinder-
 * Diskette geoeffnet.
 *
 * **B6 — ein Datenversatz unter 64 wurde still auf 64 gehoben**
 * (`if (p->data_offset < IMG2_HDR_SIZE) p->data_offset = IMG2_HDR_SIZE;`).
 * Eine Datei, die ihre Daten bei Versatz 0 behauptet, wurde damit
 * woanders gelesen, als sie sagt — eine stille Veraenderung. Jetzt
 * wird abgesagt.
 *
 * **B7 — `p->cylinders` war ein `uint8_t`, und der Ueberlauf lag VOR
 * der Schranke.** `(uint8_t)(data_length / 4096)` wird bei
 * `data_length >= 1 048 576` zu 0, und die Zeile danach lautete
 * `if (p->cylinders == 0) p->cylinders = 35;`. Eine 1-MB-2MG wurde
 * also **still** zu einer 35-Spur-Diskette. Die Schranke `> 80` kam
 * erst danach und konnte das nicht sehen.
 *
 * **B8 — die Schreibseite trug denselben Fehler.** `img2_write_track()`
 * rechnete `cyl * spt * sector_size` mit denselben erfundenen Werten;
 * eine 3,5"-2MG wurde beim Schreiben zerstoert. Lehre aus MF-931: die
 * Schreibseite gegen die Leseseite halten, nicht nur die Leseseite
 * heben.
 *
 * ── Was NICHT geaendert wurde ───────────────────────────────────────
 *
 * Die Sektornummern folgen weiter der **Dateireihenfolge**, wie bei
 * `do` (MF-716) und `po`. Das ist dieselbe Zuordnung, die MAME trifft
 * (`sectors[si].sector = i`, Z. 567): `i` laeuft in Dateireihenfolge,
 * `si` ist nur der physische Platz auf der GCR-Spur. Die Anordnung bei
 * `0x0C` (DOS gegen ProDOS) sagt, in WELCHER logischen Ordnung die
 * Bloecke liegen — sie aendert die Nummern nicht, die UFT vergibt.
 * Geprueft und deshalb nicht umgestellt.
 *
 * `blocks` bei 0x14 wird geprueft, aber nicht als Quelle benutzt: sein
 * Rueckfall in MAME (`get_u24le(&header[0x1d]) / 2`) liest die **oberen
 * drei Bytes der Datenlaenge** — 0x1D..0x1F liegen in dem 4-Byte-Feld
 * bei 0x1C — und reduziert sich damit auf `data_length / 512`. Eine
 * Fundstelle, die wie ein eigenes Feld aussieht, ist der obere Teil
 * eines anderen.
 *
 * MAMEs `load()` laesst zusaetzlich `blocks == 16390` durch (ProDOS-
 * Festplatte) und liest danach trotzdem nur 80 Spuren. Das ist
 * **nicht** uebernommen; UFT weist ab, was seine Tafel nicht kennt.
 */
#include "uft/uft_format_common.h"

#define IMG2_MAGIC      0x474D4932u /* "2IMG" LE */
#define IMG2_MAGIC_FLIP 0x32494D47u /* "GMI2" LE — MAME ap_dsk35.cpp:469 */
#define IMG2_HDR_SIZE   64
#define IMG2_FMT_DOS    0
#define IMG2_FMT_PRODOS 1
#define IMG2_FMT_NIB    2
#define IMG2_M35_TRACKS 80

/* MAME `s_formats[]` (ap_dsk35.cpp:452-459) plus die Blockzahlen aus
 * `load()` (Z. 515). `typen` ist eine Bitmaske ueber die erlaubten
 * Werte des Feldes bei 0x0C. */
typedef struct {
    unsigned typen;        /* Bit n gesetzt = Anordnung n erlaubt */
    uint32_t datenlaenge;
    uint32_t bloecke;        /* ProDOS-Bloecke von 512 Byte (Feld 0x14) */
    uint32_t sektoren;       /* Sektoren der Geometrie — NICHT dasselbe */
    int      zylinder;
    int      koepfe;
    int      spt_max;      /* groesste Zone — siehe geometry.sectors */
    int      sektorgroesse;
    int      zoniert;      /* 1 = 3,5" mit Zonentafel 12/11/10/9/8 */
} img2_tafel_t;

static const img2_tafel_t IMG2_TAFEL[] = {
    /* Typ 0 ODER 1, 143360 Byte: 5,25", 35 x 1 x 16 x 256, 280 Bloecke */
    { (1u << IMG2_FMT_DOS) | (1u << IMG2_FMT_PRODOS),
      143360u,  280u, 560u, 35, 1, 16, 256, 0 },
    /* Typ 1, 409600 Byte: 3,5" einseitig, 800 Bloecke, Zonentafel */
    { (1u << IMG2_FMT_PRODOS),  409600u,  800u,  800u, IMG2_M35_TRACKS, 1, 12, 512, 1 },
    /* Typ 1, 819200 Byte: 3,5" zweiseitig, 1600 Bloecke */
    { (1u << IMG2_FMT_PRODOS),  819200u, 1600u, 1600u, IMG2_M35_TRACKS, 2, 12, 512, 1 },
};
#define IMG2_TAFEL_N (int)(sizeof(IMG2_TAFEL) / sizeof(IMG2_TAFEL[0]))

typedef struct {
    FILE*    file;
    uint32_t data_offset;
    uint32_t data_length;
    uint32_t img_format;
    const img2_tafel_t *g;
} img2_data_t;

/* MAME ap_dsk35.cpp:560 — `int ns = 12 - (track/16);` */
int uft_2img_zone_spt(int cyl)
{
    if (cyl < 0 || cyl >= IMG2_M35_TRACKS) return 0;
    return 12 - (cyl / 16);
}

static uint32_t img2_swap32(uint32_t v)
{
    return ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8)
         | ((v & 0x00FF0000u) >> 8)  | ((v & 0xFF000000u) >> 24);
}

/**
 * Kennung pruefen. MAME nimmt beide an — die byte-vertauschte stammt
 * von einem benannten Erzeuger (B1).
 */
int uft_2img_signatur_ok(const uint8_t *data)
{
    uint32_t m = uft_read_le32(data);
    return m == IMG2_MAGIC || m == IMG2_MAGIC_FLIP;
}

/**
 * Tafeleintrag zu Anordnung und Datenlaenge suchen; erlaubt MAMEs
 * Byte-Vertauschung der Laenge (B3) und gibt den berichtigten Wert
 * heraus.
 */
const void *uft_2img_tafel(uint32_t typ, uint32_t datenlaenge,
                           uint32_t *berichtigt)
{
    int i;
    if (typ > 31u) return NULL;
    for (i = 0; i < IMG2_TAFEL_N; i++) {
        const img2_tafel_t *t = &IMG2_TAFEL[i];
        if (!(t->typen & (1u << typ))) continue;
        if (t->datenlaenge == datenlaenge) {
            if (berichtigt) *berichtigt = datenlaenge;
            return t;
        }
        if (t->datenlaenge == img2_swap32(datenlaenge)) {
            if (berichtigt) *berichtigt = t->datenlaenge;
            return t;
        }
    }
    return NULL;
}

/**
 * Versatz einer Spur in der Datei.
 *
 * Nicht zoniert: `data_offset + (cyl * heads + head) * spt * ss`.
 * Zoniert (3,5"): Spur aussen, Kopf innen, Sektorzahl je Spur aus der
 * Zonentafel — MAME `load()` Z. 558-562.
 */
long uft_2img_track_offset(int cyl, int head, int zoniert, int heads,
                           int spt, int sektorgroesse, uint32_t data_offset)
{
    long bloecke = 0;
    int c;
    if (cyl < 0 || head < 0 || head >= heads) return -1;
    if (!zoniert)
        return (long)data_offset
             + (long)(cyl * heads + head) * spt * sektorgroesse;
    if (cyl >= IMG2_M35_TRACKS) return -1;
    for (c = 0; c < cyl; c++)
        bloecke += (long)heads * uft_2img_zone_spt(c);
    bloecke += (long)head * uft_2img_zone_spt(cyl);
    return (long)data_offset + bloecke * sektorgroesse;
}

bool img2_probe(const uint8_t *data, size_t size, size_t file_size,
                int *confidence)
{
    uint32_t typ, laenge, kopfgroesse, versatz, berichtigt = 0;
    const img2_tafel_t *t;

    if (!data || size < IMG2_HDR_SIZE) return false;
    if (!uft_2img_signatur_ok(data)) return false;

    kopfgroesse = uft_read_le16(data + 0x08);
    typ    = uft_read_le32(data + 0x0C);
    versatz = uft_read_le32(data + 0x18);
    laenge  = uft_read_le32(data + 0x1C);

    /* B6: der Datenversatz darf nicht in den Kopf zeigen. */
    if (kopfgroesse == 0) kopfgroesse = IMG2_HDR_SIZE;
    if (versatz < kopfgroesse) return false;

    /* B5: nur was in der Tafel steht. */
    t = (const img2_tafel_t *)uft_2img_tafel(typ, laenge, &berichtigt);
    if (!t) return false;

    /* B4: die Struktur-Probe braucht die DATEIgroesse — MAME Z. 497-499
     * (`data_length + get_u32le(&header[0x18]) == size`). Dass sie hier
     * ueberhaupt moeglich ist, ist der Unterschied zu MF-1029. */
    if (confidence) {
        *confidence = ((uint64_t)versatz + berichtigt == (uint64_t)file_size)
                      ? 95   /* Kennung UND Struktur */
                      : 80;  /* nur die Kennung (MAMEs FIFID_SIGN) */
    }
    return true;
}

static uft_error_t img2_open(uft_disk_t *disk, const char *path, bool ro)
{
    FILE *f;
    uint8_t hdr[IMG2_HDR_SIZE];
    img2_data_t *p;
    const img2_tafel_t *t;
    uint32_t kopfgroesse, typ, versatz, laenge, berichtigt = 0, bloecke;

    f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    if (fread(hdr, 1, IMG2_HDR_SIZE, f) != IMG2_HDR_SIZE) {
        fclose(f); return UFT_ERROR_IO;
    }
    if (!uft_2img_signatur_ok(hdr)) { fclose(f); return UFT_ERROR_FORMAT_INVALID; }

    kopfgroesse = uft_read_le16(hdr + 0x08);
    if (kopfgroesse == 0) kopfgroesse = IMG2_HDR_SIZE;
    typ     = uft_read_le32(hdr + 0x0C);
    bloecke = uft_read_le32(hdr + 0x14);
    versatz = uft_read_le32(hdr + 0x18);
    laenge  = uft_read_le32(hdr + 0x1C);

    /* MF-725: NIB-Inhalt wird ABGEWIESEN, nicht missdeutet.
     *
     * Der Kopf sagt bei `0x0C == 2`, dass innen der rohe Nibble-Strom
     * liegt — kein Sektor-Abbild. Bis MF-725 setzte dieser Zweig
     * trotzdem `spt = 16`, `sector_size = 256`, und `img2_read_track()`
     * rechnete daraus die Schrittweite `cyl * 4096`. Eine NIB-Spur ist
     * **6656 Byte** gross (`mamedev/mame` `ap2_dsk.h:150`
     * `nibbles_per_track = 0x1a00`, und `src/formats/nib/uft_nib.c:9`).
     *
     * Gemessen am Rotbeweis (`tests/test_2img_nib_stride.c`): Spur 1
     * wurde bei Versatz 4160 statt 6720 gelesen — mitten in Spur 0.
     *
     * Die zweite Folge wiegt schwerer als die erste: die gelesenen
     * Bytes wurden als **Sektoren 0..15** ausgegeben. Rohe Nibbles sind
     * keine Sektoren; sie sind GCR-kodiert und tragen ihre Nummern in
     * Adressfeldern. Das ist erfundene Struktur, und sie waere auch bei
     * richtiger Schrittweite falsch.
     *
     * Warum nicht dekodiert wird, obwohl der Baum seit MF-715/721 einen
     * GCR-Dekoder hat: `nib` steht nach GCR-3 (MF-723) auf
     * Unabhaengigkeit gesperrt — der einzige verfuegbare Erzeuger
     * benutzt dieselben `nibblize_*.c` wie das Oracle, gegen das unser
     * Dekoder geeicht ist. Ein Differenzlauf waere tautologisch. Bis das
     * geoeffnet ist, ist „ich kann diesen Inhalt nicht" die ehrliche
     * Antwort. MAME sagt an derselben Stelle dasselbe: `if(secttype > 1)
     * return false; // nibble images not supported` (Z. 511). */
    if (typ == IMG2_FMT_NIB) { fclose(f); return UFT_ERROR_NOT_SUPPORTED; }

    /* B6 */
    if (versatz < kopfgroesse) { fclose(f); return UFT_ERROR_FORMAT_INVALID; }

    /* B5 + B3 */
    t = (const img2_tafel_t *)uft_2img_tafel(typ, laenge, &berichtigt);
    if (!t) { fclose(f); return UFT_ERROR_FORMAT_INVALID; }

    /* `blocks` wird geprueft, nicht benutzt. 0 heisst „ableiten", und
     * MAMEs Ableitung ist `data_length / 512` (siehe Kopfkommentar). */
    if (bloecke != 0 && bloecke != t->bloecke) {
        fclose(f); return UFT_ERROR_FORMAT_INVALID;
    }

    p = calloc(1, sizeof(img2_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    p->img_format = typ;
    p->data_offset = versatz;
    p->data_length = berichtigt;
    p->g = t;

    disk->plugin_data = p;
    disk->geometry.cylinders = (uint32_t)t->zylinder;
    disk->geometry.heads     = (uint32_t)t->koepfe;
    /* Bei zonierten 3,5"-Abbildern ist das die GROESSTE Zone; die
     * Sektorzahl je Spur liefert `uft_2img_zone_spt()`. Dieselbe
     * Darstellung wie bei `victor9k` (MF-1026). */
    disk->geometry.sectors     = (uint32_t)t->spt_max;
    disk->geometry.sector_size = (uint32_t)t->sektorgroesse;
    /* SEKTOREN, nicht ProDOS-Bloecke: bei 5,25" sind es 560 Sektoren
     * von 256 Byte, aber nur 280 Bloecke von 512. Der erste Entwurf
     * hier setzte `t->bloecke`, und der Test hat es gefangen. */
    disk->geometry.total_sectors = t->sektoren;
    return UFT_OK;
}

static void img2_close(uft_disk_t *d)
{
    img2_data_t *p = d->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); d->plugin_data = NULL; }
}

static int img2_spt(const img2_data_t *p, int cyl)
{
    return p->g->zoniert ? uft_2img_zone_spt(cyl) : p->g->spt_max;
}

static uft_error_t img2_read_track(uft_disk_t *d, int cyl, int head,
                                   uft_track_t *t)
{
    img2_data_t *p;
    long off;
    int spt, s, ss;
    uint8_t buf[512];

    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    p = d->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (cyl >= p->g->zylinder || head >= p->g->koepfe)
        return UFT_ERROR_INVALID_PARAM;

    uft_track_init(t, cyl, head);
    ss = p->g->sektorgroesse;
    spt = img2_spt(p, cyl);
    off = uft_2img_track_offset(cyl, head, p->g->zoniert, p->g->koepfe,
                                spt, ss, p->data_offset);
    if (off < 0) return UFT_ERROR_INVALID_PARAM;
    if (fseek(p->file, off, SEEK_SET) != 0) return UFT_ERROR_IO;

    for (s = 0; s < spt; s++) {
        if (fread(buf, 1, (size_t)ss, p->file) != (size_t)ss)
            return UFT_ERROR_IO;
        /* Apple-Sektoren sind 0-basiert (ARCH-20), und die Nummer ist
         * die Stelle in der DATEI — wie bei `do`/`po` (MF-716) und wie
         * MAMEs `sectors[si].sector = i`. */
        uft_format_add_sector_with_id(t, (uint8_t)s, buf, (size_t)ss,
                                      (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

static uft_error_t img2_write_track(uft_disk_t *d, int cyl, int head,
                                    const uft_track_t *t)
{
    img2_data_t *p;
    long off;
    int spt, ss;
    size_t s;

    /* MF-529: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. MF-519 hat das fuer
     * read_track getan und write_track uebersehen. Das ASan-Tor
     * der CI fand die Folge an d80_write_track: die Schranke
     * `cyl >= D80_TRACKS` laesst -1 durch, und d80_spt[-1] liest
     * vor der Tabelle.
     *
     * Beim SCHREIBEN wiegt das schwerer als beim Lesen: ein
     * falscher Index liefert nicht nur falsche Daten, er bestimmt,
     * WOHIN geschrieben wird. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    p = d->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (d->read_only) return UFT_ERROR_NOT_SUPPORTED;
    /* B8: dieselbe Geometrie wie die Leseseite, nicht eine zweite
     * (MF-931). */
    if (cyl >= p->g->zylinder || head >= p->g->koepfe)
        return UFT_ERROR_INVALID_PARAM;

    ss = p->g->sektorgroesse;
    spt = img2_spt(p, cyl);
    off = uft_2img_track_offset(cyl, head, p->g->zoniert, p->g->koepfe,
                                spt, ss, p->data_offset);
    if (off < 0) return UFT_ERROR_INVALID_PARAM;

    for (s = 0; s < t->sector_count && (int)s < spt; s++) {
        const uint8_t *data = t->sectors[s].data;
        uint8_t pad[512];
        if (fseek(p->file, off + (long)s * ss, SEEK_SET) != 0)
            return UFT_ERROR_IO;
        if (!data || t->sectors[s].data_len == 0) {
            memset(pad, 0xE5, (size_t)ss); data = pad;
        }
        if (fwrite(data, 1, (size_t)ss, p->file) != (size_t)ss)
            return UFT_ERROR_IO;
    }
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_2img_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_2img = {
    .name = "2IMG", .description = "Apple II Universal Disk Image",
    .extensions = "2img;2mg", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = img2_probe, .open = img2_open, .close = img2_close,
    .read_track = img2_read_track, .write_track = img2_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_OFFICIAL_FULL,  /* 2IMG v1 header spec public since Apple IIgs Sweet16 days */
    .features = uft_format_plugin_2img_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_2img_features) / sizeof(uft_format_plugin_2img_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(2img)
