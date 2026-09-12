/**
 * @file uft_v9t9.c
 * @brief V9T9 / TI-99 Sector Dump Format (SDF)
 *
 * V9T9 ist ein kopfloser Sektorabzug einer TI-99/4A-Diskette: die
 * Sektoren stehen in der **logischen** Reihenfolge des TI-Dateisystems,
 * 256 Byte je Sektor, ohne Spurdaten.
 *
 * ── Referenz ────────────────────────────────────────────────────────
 *
 * MAME `src/lib/formats/ti99_dsk.cpp` / `.h`, **LGPL-2.1+**, Copyright
 * Michael Zapf. **Gelesen, nicht uebernommen** — Kanal *Spec* nach
 * MF-695; eigenstaendige Umsetzung nach dem dokumentierenden Kommentar
 * und den dort genannten Zahlen. Getragen haben vier Stellen:
 *
 *   Z. 883-908  der Kommentar zur Spurreihenfolge (unten woertlich)
 *   Z. 936-944  `identify()` — die sieben zulaessigen Dateigroessen
 *   Z. 989-991  die Fehlsektorkarte: 768 Byte werden abgezogen, wenn
 *               `(groesse - 768) % (256*360) == 0`
 *   Z. 1077     die Versatzformel:
 *               `logicaltrack = (head==0)? track : (2*trackcount - track - 1)`
 *   ti99_dsk.h:70-84  die Feldlagen der VIB (Volume Information Block)
 *
 * ── Die Spurreihenfolge, die MF-1027 gefunden hat ───────────────────
 *
 * MAMEs Kommentar sagt es woertlich:
 *
 *     The TI file system orders all tracks on side 0 as going inwards,
 *     and then all tracks on side 1 going outwards.
 *
 *         00 01 02 03 ... 38 39     side 0
 *         79 78 77 76 ... 41 40     side 1
 *
 *     The SDF format stores the tracks and their sectors in logical
 *     order: 00 01 02 03 ... 38 39 [40 41 ... 79]
 *
 * Die Datei ist also **kopf-dur** angeordnet (erst die ganze Seite 0,
 * dann Seite 1), und **innerhalb der Seite 1 laeuft die Spurzahl
 * rueckwaerts**: die logische Spur 40 ist die INNERSTE Spur der
 * zweiten Seite, die logische Spur 79 die aeusserste.
 *
 * Hier stand bis MF-1027 `off = (cyl * heads + head) * spt * 256` —
 * zylinder-verschraenkt UND ohne Umkehrung, also zwei unabhaengige
 * Abweichungen. Gemessen:
 *
 *   SSSD  (92160 Byte)  40 Spuren   **0 von 40** falsch
 *   DSSD (184320 Byte)  80 Spuren  **78 von 80** falsch, bis 179712 Byte
 *   DSDD (368640 Byte)  80 Spuren  **78 von 80** falsch, bis 359424 Byte
 *
 * Bei einseitigen Dateien geht die alte Formel in die richtige ueber
 * (`heads == 1` macht aus `cyl*1+0` genau `cyl`) — deshalb ist der
 * Fehler nie aufgefallen. Zweiseitig trafen nur zwei Spuren, und die
 * zweite davon (Kopf 1, Spur 26) rein zufaellig, weil `2t+1 = 79-t`
 * bei t=26 aufgeht.
 *
 * Beispiel DSSD, Kopf 1 Spur 0: das ist die **aeusserste** Spur der
 * zweiten Seite, liegt also bei Versatz 182016 (logische Spur 79) —
 * UFT suchte sie an Position 2304.
 *
 * ── Vier weitere Befunde ────────────────────────────────────────────
 *
 *  2. **Vier von sieben Dateigroessen wurden abgewiesen.** Angenommen
 *     waren 92160, 184320 und 368640; MAMEs `identify()` fuehrt dazu
 *     163840 (SSDD16), 327680 (DSDD16), 737280 (DSDD80) und 1474560
 *     (DSQD). Die beiden 80-Spur-Formate waren dabei ohnehin
 *     unerreichbar, weil `p->cyl` fest auf 40 stand.
 *
 *  3. **Die VIB wurde nie gelesen.** Sektor 0 einer formatierten
 *     TI-Diskette ist der Volume Information Block und traegt
 *     `"DSK"` bei Versatz 13, die Sektoren je Spur bei 12, die Spuren
 *     je Seite bei 17 und die Seitenzahl bei 18. Die Sonde ignorierte
 *     den Inhalt vollstaendig (`(void)d; (void)s;`) und vergab
 *     Konfidenz 40 — nach MF-729 die Stufe „nur die Groesse", was
 *     ohne Inhaltspruefung auch richtig war.
 *
 *  4. **184320 Byte sind ZWEIDEUTIG, und die Zweideutigkeit war still
 *     aufgeloest.** SSDD (1 Seite, 40 Spuren, 18 Sektoren) und DSSD
 *     (2 Seiten, 40 Spuren, 9 Sektoren) haben dieselbe Groesse. MAMEs
 *     Kommentar sagt genau das („SSDD happens to be the same size as
 *     DSSD in the sector dump format, so we need to ask the VIB") und
 *     fragt deshalb die VIB; ohne VIB nimmt es DSSD an. UFT nahm
 *     **immer** DSSD — also MAMEs Rueckfall, auch wenn die Datei
 *     selbst SSDD sagt.
 *
 *  5. **Keine obere Schranke.** Ein Zylinder oder Kopf ausserhalb der
 *     Geometrie ging in die Versatzrechnung ein; beim Schreiben haette
 *     er bestimmt, WOHIN geschrieben wird.
 *
 * ── Wo dieses Werkzeug dem Orakel bewusst NICHT folgt ───────────────
 *
 * MAME benutzt die Angaben der VIB auch dann, wenn sie der Dateigroesse
 * widersprechen, und gibt nur eine Warnung aus. Hier gilt: die VIB ist
 * eine **Behauptung**, die Dateigroesse eine **Tatsache**. Stimmen sie
 * nicht zusammen, wird die aus der Groesse abgeleitete Geometrie
 * genommen — eine Geometrie, die nicht in die Datei passt, fuehrt beim
 * Lesen hinter das Dateiende und erzeugt damit genau die erfundenen
 * Daten, die dieses Werkzeug nicht liefern soll.
 *
 * Regressionsschutz: `tests/test_v9t9_gegen_mame.c`.
 */
#include "uft/uft_format_common.h"

#define V9T9_SS       256
/* Drei Sektoren Fehlsektorkarte am Dateiende; von einem PC-Werkzeug
 * eingefuehrt, von MAME toleriert und ignoriert. */
#define V9T9_BADMAP   768
#define V9T9_SDKORN   (V9T9_SS * 360)   /* 92160 — MAMEs Modulus */

/* VIB-Feldlagen, MAME `ti99_dsk.h:70-84`. Die Struktur ist genau
 * 256 Byte gross und belegt damit Sektor 0 vollstaendig. */
#define VIB_SECSPERTRACK   12
#define VIB_ID             13
#define VIB_TRACKSPERSIDE  17
#define VIB_SIDES          18

typedef struct {
    uint8_t cyl;
    uint8_t heads;
    uint8_t spt;
} v9t9_geo_t;

typedef struct {
    FILE      *file;
    v9t9_geo_t geo;
} v9t9_pd_t;

/* Die sieben Groessen aus MAMEs `identify()` (Z. 936-944), je mit der
 * Geometrie, die `determine_sizes()` daraus ableitet. */
static const struct {
    long    size;
    uint8_t cyl, heads, spt;
    const char *name;
} v9t9_tafel[] = {
    {   92160, 40, 1,  9, "SSSD"   },
    {  163840, 40, 1, 16, "SSDD16" },
    {  184320, 40, 2,  9, "DSSD"   },  /* zweideutig, siehe Kopf */
    {  327680, 40, 2, 16, "DSDD16" },
    {  368640, 40, 2, 18, "DSDD"   },
    {  737280, 80, 2, 18, "DSDD80" },
    { 1474560, 80, 2, 36, "DSQD"   },
};

/** Dateigroesse ohne eine etwaige Fehlsektorkarte. */
static long v9t9_nutzgroesse(long fs)
{
    if (fs > V9T9_BADMAP && ((fs - V9T9_BADMAP) % V9T9_SDKORN) == 0)
        return fs - V9T9_BADMAP;
    return fs;
}

/** Geometrie aus der Nutzgroesse; false, wenn die Groesse keine ist. */
static bool v9t9_geo_aus_groesse(long nutz, v9t9_geo_t *g)
{
    for (size_t i = 0; i < sizeof(v9t9_tafel) / sizeof(v9t9_tafel[0]); i++) {
        if (v9t9_tafel[i].size != nutz) continue;
        g->cyl   = v9t9_tafel[i].cyl;
        g->heads = v9t9_tafel[i].heads;
        g->spt   = v9t9_tafel[i].spt;
        return true;
    }
    return false;
}

/** Traegt Sektor 0 eine lesbare VIB? */
static bool v9t9_vib_gueltig(const uint8_t *d, size_t s)
{
    if (!d || s < V9T9_SS) return false;
    if (d[VIB_ID] != 'D' || d[VIB_ID + 1] != 'S' || d[VIB_ID + 2] != 'K')
        return false;
    /* MAME: „Do we have a broken VIB? The Pascal disks are known to
     * have such incomplete VIBs" — eine Null in einem der drei Felder
     * macht die Angabe unbrauchbar. */
    return d[VIB_SECSPERTRACK] != 0 && d[VIB_TRACKSPERSIDE] != 0
           && d[VIB_SIDES] != 0;
}

/**
 * Geometrie bestimmen: erst die Groesse (sie entscheidet, ob es
 * ueberhaupt eine V9T9 ist), dann die VIB als Verfeinerung — aber nur,
 * wenn ihre Angaben in die Datei PASSEN. Siehe Kopf, Abschnitt „wo
 * dieses Werkzeug dem Orakel bewusst nicht folgt".
 */
static bool v9t9_geometrie(const uint8_t *d, size_t s, long nutz,
                           v9t9_geo_t *g, bool *aus_vib)
{
    if (aus_vib) *aus_vib = false;
    if (!v9t9_geo_aus_groesse(nutz, g)) return false;
    if (!v9t9_vib_gueltig(d, s)) return true;

    {
        const long passt = (long)d[VIB_TRACKSPERSIDE] * d[VIB_SIDES]
                           * d[VIB_SECSPERTRACK] * V9T9_SS;
        if (passt != nutz || d[VIB_SIDES] > 2) return true;
        g->cyl   = d[VIB_TRACKSPERSIDE];
        g->heads = d[VIB_SIDES];
        g->spt   = d[VIB_SECSPERTRACK];
        if (aus_vib) *aus_vib = true;
    }
    return true;
}

/**
 * Byteversatz der Spur (cyl, head).
 *
 * MAME `ti99_sdf_format::load_track()` Z. 1077:
 *     logicaltrack = (head==0)? track : (2*trackcount - track - 1)
 */
static long v9t9_track_offset(int cyl, int head, const v9t9_geo_t *g)
{
    const int logical = (head == 0) ? cyl : (2 * g->cyl - cyl - 1);
    return (long)logical * g->spt * V9T9_SS;
}

/* ---- probe ---- */
static bool v9t9_probe(const uint8_t *d, size_t s, size_t fs, int *c)
{
    v9t9_geo_t g;
    bool aus_vib = false;
    if (!v9t9_geometrie(d, s, v9t9_nutzgroesse((long)fs), &g, &aus_vib))
        return false;

    /* MF-729: 30..49 heisst „nur die Groesse", 80..100 „Merkmal
     * getroffen". Eine gueltige VIB IST ein Merkmal — die drei Bytes
     * `"DSK"` bei Versatz 13 stehen in keinem Zufallspuffer. Fehlt sie
     * (unformatierte Diskette), bleibt es bei der Groesse. */
    *c = v9t9_vib_gueltig(d, s) ? 85 : 40;
    (void)aus_vib;
    return true;
}

/* ---- open ---- */
static uft_error_t v9t9_open(uft_disk_t *disk, const char *path, bool ro)
{
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    /* Sektor 0 lesen, damit die VIB die Zweideutigkeit bei 184320 Byte
     * aufloesen kann. Ein kurzer Lesevorgang ist kein Fehler — eine
     * unformatierte Diskette hat keine VIB. */
    uint8_t vib[V9T9_SS];
    size_t gelesen = fread(vib, 1, V9T9_SS, f);
    memset(vib + gelesen, 0, V9T9_SS - gelesen);

    v9t9_geo_t g;
    if (!v9t9_geometrie(vib, gelesen, v9t9_nutzgroesse(sz), &g, NULL)) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }

    v9t9_pd_t *p = calloc(1, sizeof(v9t9_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    p->geo  = g;

    disk->plugin_data = p;
    disk->geometry.cylinders   = g.cyl;
    disk->geometry.heads       = g.heads;
    disk->geometry.sectors     = g.spt;
    disk->geometry.sector_size = V9T9_SS;
    disk->geometry.total_sectors = (uint32_t)g.cyl * g.heads * g.spt;
    return UFT_OK;
}

/* ---- close ---- */
static void v9t9_close(uft_disk_t *disk)
{
    v9t9_pd_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

/* ---- read_track ---- */
static uft_error_t v9t9_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track)
{
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    v9t9_pd_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;

    /* MF-1027: die obere Schranke fehlte. Ohne sie ging ein zu grosser
     * Zylinder in die Versatzrechnung ein — und weil die Umkehrung auf
     * Kopf 1 `2*cyl_max - cyl - 1` rechnet, konnte daraus sogar ein
     * NEGATIVER Versatz werden. */
    if (cyl >= p->geo.cyl || head >= p->geo.heads)
        return UFT_ERROR_INVALID_PARAM;

    uft_track_init(track, cyl, head);

    const long off = v9t9_track_offset(cyl, head, &p->geo);
    uint8_t buf[V9T9_SS];
    for (int s = 0; s < p->geo.spt; s++) {
        if (fseek(p->file, off + (long)s * V9T9_SS, SEEK_SET) != 0)
            return UFT_ERROR_IO;
        /* MF-980: der kurze Lesevorgang wird GEMERKT, nicht nur
         * gefuellt. `uft_format_add_sector*()` legt jeden Sektor mit
         * `status = UFT_SECTOR_OK` und „CRC gueltig" an — die Fuellung
         * war damit von echten Daten nicht zu unterscheiden. Die Bytes
         * bleiben stehen, sie gelten nur nicht mehr als Messwert. */
        const bool kurz = (fread(buf, 1, V9T9_SS, p->file) != V9T9_SS);
        if (kurz) memset(buf, 0xE5, V9T9_SS);
        /* MF-1027: die TI-Sektornummern sind **0-basiert** (0..8 bzw.
         * 0..17); MAMEs `load_track()` setzt `sector[i] = secno` mit
         * `secno` aus `0..sectorcount-1`. `uft_format_add_sector()`
         * haette laut eigenem Kopf 1 addiert. Gestalt von MF-1016. */
        uft_format_add_sector_with_id(track, (uint8_t)s, buf, V9T9_SS,
                                      (uint8_t)cyl, (uint8_t)head);
        if (kurz) uft_format_mark_last_missing(track);
    }
    return UFT_OK;
}

/* ---- write_track ---- */
static uft_error_t v9t9_write_track(uft_disk_t *disk, int cyl, int head,
                                     const uft_track_t *track)
{
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

    v9t9_pd_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    /* MF-1027, und hier traegt die Schranke die ganze Last: die
     * Umkehrung auf Kopf 1 kann aus einem zu grossen Zylinder einen
     * negativen Versatz machen. */
    if (cyl >= p->geo.cyl || head >= p->geo.heads)
        return UFT_ERROR_INVALID_PARAM;

    const long off = v9t9_track_offset(cyl, head, &p->geo);
    for (size_t s = 0; s < track->sector_count && (int)s < p->geo.spt; s++) {
        if (fseek(p->file, off + (long)s * V9T9_SS, SEEK_SET) != 0)
            return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[V9T9_SS];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, V9T9_SS);
            data = pad;
        }
        if (fwrite(data, 1, V9T9_SS, p->file) != V9T9_SS)
            return UFT_ERROR_IO;
    }
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_v9t9_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_v9t9 = {
    .name = "V9T9", .description = "TI-99/4A V9T9",
    .extensions = "v9t9;dsk", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = v9t9_probe, .open = v9t9_open, .close = v9t9_close,
    .read_track = v9t9_read_track, .write_track = v9t9_write_track,
    .verify_track = uft_generic_verify_track,
    /* Geprueft und NICHT angefasst: TI-99 hat keine offizielle
     * Formatspezifikation, MAMEs Umsetzung ist selbst eine
     * RE-Referenz. Die Verifikationsstufe traegt das Tier-System. */
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_v9t9_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_v9t9_features) / sizeof(uft_format_plugin_v9t9_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(v9t9)
