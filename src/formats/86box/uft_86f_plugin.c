/**
 * @file uft_86f_plugin.c
 * @brief 86F (86Box/PCem) — Leser gegen die Spezifikation (MF-961)
 *
 * ── Was 86F ist ─────────────────────────────────────────────────────────
 *
 * Ein OBERFLAECHENFORMAT. Es speichert je Spur die MFM-/FM-Bitzellen,
 * nicht CHS-Sektoren. Sektoren entstehen erst durch Dekodieren des
 * Bitstroms — hier durch `uft_mfm_decode_track()`, den geprueften
 * Sektor-Dekoder des Baums (`src/flux/uft_mfm_sector_parser.c`).
 *
 * ── Die Quelle ──────────────────────────────────────────────────────────
 *
 * `dev/formats/86f.rst` im Dokumentations-Repositorium von 86Box selbst
 * (github.com/86Box/docs, abgerufen 2026-09-08). Keine Sekundaerquelle
 * und keine Rueckentwicklung: die Beschreibung des Formats durch dessen
 * Urheber. Zweite, unabhaengige Quelle: `Digitoxin1/DiskImageTool`,
 * `ImageFormats/86F/86FImage.vb` — ein eigenstaendiger Leser, der
 * dasselbe Magic und dieselbe Kopfgroesse nennt (belegt in MF-708).
 *
 * KEINE Zeile fremden Quellcodes ist dabei gelesen worden — weder
 * 86Box' `fdd_86f.c` noch fluxfox' Rust-Leser. Der Kanal ist die
 * SPEZIFIKATION (MF-695), und die Struktur ist an einer echten Datei
 * nachgemessen, bevor hier eine Zeile stand.
 *
 * ── Aufbau, wie die Spezifikation ihn angibt ────────────────────────────
 *
 *   0x00  Magic "86BF"
 *   0x04  Minor version (0x0C)
 *   0x05  Major version (0x02)
 *   0x06  Disk flags, 16 Bit
 *   0x08  Spur-Offsets, je 32 Bit, JE SEITE JE SPUR in der Reihenfolge
 *         Spur 0 Seite 0, Spur 0 Seite 1, Spur 1 Seite 0, ...
 *
 * Disk flags, soweit dieser Leser sie braucht:
 *   Bit 0     Oberflaechenbeschreibung vorhanden (weak bits / Loecher)
 *   Bit 3     1 = zwei Seiten
 *   Bit 7     1 = Zellzahl im Spurkopf vorhanden
 *   Bit 12    deutet Bit 7 um: die Zahl ist die GESAMTzahl, nicht ein Zusatz
 *
 * Spurkopf, mit Zellzahl (Bit 7):
 *   +0x00  Spur-Flags, 16 Bit
 *   +0x02  Zellzahl, 32 Bit
 *   +0x06  Indexloch-Position in Bitzellen, 32 Bit
 *   +0x0A  Bitzellen, (Zellzahl+7)/8 Byte
 *   danach Oberflaechenbeschreibung gleicher Laenge, falls Bit 0
 *
 * Ohne Zellzahl faellt das Feld weg und die Daten beginnen bei +0x06.
 *
 * Spur-Flags:
 *   Bits 2-0  Bitrate  (0=500, 1=300, 2=250, 3=1000, 5=2000 kbps)
 *   Bits 4-3  Kodierung (0=FM, 1=MFM, 2=M2FM, 3=GCR)
 *   Bits 7-5  Drehzahl
 *
 * ── An einer echten Datei nachgemessen ──────────────────────────────────
 *
 * `tests/corpus/fluxfox_sector_test/sector_test_360k.86f`, eine
 * 5,25-Zoll-360K-Diskette (dbalsom/fluxfox, MIT):
 *
 *     Magic 86BF, Version 2.0C, Disk-Flags 0x1088
 *     Tabelle 512 Eintraege, davon 172 belegt (86 Zylinder x 2 Seiten)
 *     Spur-Flags 0x000A = MFM, 250 kbps
 *     Ende der letzten Spur == Dateigroesse, Differenz 0
 *     Bitstrom MSB zuerst: 54 Treffer 0x4489 je Spur = 9 Sektoren
 *
 * Die letzte Zeile entscheidet die Bitreihenfolge: LSB zuerst ergibt 2
 * Treffer, also Rauschen. `uft_mfm_decode_track()` erwartet MSB zuerst,
 * die Daten gehen unveraendert hinein.
 *
 * ── Was hier vorher stand ───────────────────────────────────────────────
 *
 * Ein Leser, der auf `"86BX"` probte — ein Magic, das in KEINER echten
 * 86F-Datei steht —, einen 32-Byte-Kopf annahm, eine 12-Byte-Tabelle mit
 * `offset/length/flags/sectors/rpm` erfand und die Bitzellen als rohe
 * Sektorbytes ausgab. Er wies damit jede echte Datei ab und meldete
 * dabei „Read: SUPPORTED".
 *
 * MF-707/708 haben das gemessen und ausdruecklich NICHT einzeilig
 * berichtigt: mit richtigem Magic haette er die Dateien ANGENOMMEN und
 * mit falschem Kopf-Versatz zerlegt — aus „wirkungslos" waere „nimmt an
 * und zerlegt falsch" geworden. Die Neufassung war als eigene Aufgabe
 * benannt. Das ist sie.
 *
 * ── Was dieser Leser NICHT tut ──────────────────────────────────────────
 *
 * Er erfindet keine Geometrie. Die Zylinderzahl kommt aus der Zahl der
 * BELEGTEN Tabelleneintraege, nicht aus einer Typtabelle; die
 * Sektorzahl steht nirgends im Container und wird deshalb erst beim
 * Lesen einer Spur bekannt.
 *
 * Er halbiert die Spurzahl nicht. Die gemessene Datei fuehrt 86
 * Zylinder fuer eine 40-spurige Diskette, und Zylinder 0 und 1 tragen
 * byteidentische Synchronpositionen — die Aufnahme stammt aus einem
 * 80-spurigen Laufwerk. Das ist eine Aussage ueber die Aufnahme und
 * gehoert dem Aufrufer gemeldet, nicht stillschweigend wegnormiert.
 *
 * Er dekodiert nur MFM. Fuer FM, M2FM und GCR gibt es in diesem Baum
 * keinen Bitstrom-Sektordekoder; solche Spuren liefern 0 Sektoren mit
 * benanntem Grund statt geratener Daten.
 *
 * Rotbeweis und Abnahme: `tests/test_86f_spec_conformance.c`.
 */
#include "uft/uft_format_common.h"
#include "uft/flux/uft_mfm_sector_parser.h"

#define F86_MAGIC        "86BF"
#define F86_MAGIC_LEN    4
#define F86_HDR_SIZE     8          /* Magic + Version + Disk-Flags */
#define F86_OFF_ENTRY    4          /* ein Spur-Offset, 32 Bit */

/* Disk-Flags */
#define F86_DF_SURFACE   0x0001u    /* Bit 0  */
#define F86_DF_TWO_SIDES 0x0008u    /* Bit 3  */
#define F86_DF_BITCELL   0x0080u    /* Bit 7  */
#define F86_DF_INTERP    0x1000u    /* Bit 12 */

/* Spur-Flags */
#define F86_TF_ENC_SHIFT 3
#define F86_TF_ENC_MASK  0x03u
#define F86_TF_ENC_FM    0u
#define F86_TF_ENC_MFM   1u
#define F86_TF_ENC_M2FM  2u
#define F86_TF_ENC_GCR   3u

/* Eine Spur fasst hoechstens so viele Sektoren; 2,88M hat 36. */
#define F86_MAX_SECTORS  64

typedef struct {
    uint8_t  *data;
    size_t    size;
    uint16_t  disk_flags;
    uint32_t  entry_count;   /* Eintraege in der Offset-Tabelle */
    uint32_t  used_count;    /* davon belegt (Offset != 0) */
    uint16_t  cylinders;     /* used_count / sides */
    uint8_t   sides;
} f86_pd_t;

/* ── Kopf und Tabelle ──────────────────────────────────────────────── */

/* Wie viele Eintraege fasst die Offset-Tabelle?
 *
 * Die Spezifikation nennt keine feste Zahl. Sie ergibt sich aus dem
 * ERSTEN Spur-Offset: bis dorthin reicht die Tabelle. An der gemessenen
 * Datei geht das auf — (0x808 - 8) / 4 = 512 Eintraege = 256 Spuren x 2
 * Seiten —, und das Ende der letzten Spur trifft die Dateigroesse genau.
 *
 * Ein Offset von 0 heisst „diese Spur ist nicht vorhanden". Der erste
 * Offset ungleich 0 begrenzt die Tabelle; steht er zu frueh oder hinter
 * dem Dateiende, ist der Container unbrauchbar. */
static bool f86_table_extent(const uint8_t *d, size_t size,
                             uint32_t *entries_out)
{
    if (size < F86_HDR_SIZE + F86_OFF_ENTRY) return false;

    uint32_t first = 0;
    for (size_t p = F86_HDR_SIZE; p + F86_OFF_ENTRY <= size;
         p += F86_OFF_ENTRY) {
        const uint32_t v = uft_read_le32(d + p);
        if (v != 0) { first = v; break; }
        /* Eine Tabelle aus lauter Nullen hat keine Spuren. Damit die
         * Schleife nicht durch die ganze Datei laeuft, wird bei der
         * groesstmoeglichen sinnvollen Tabelle abgebrochen. */
        if (p > F86_HDR_SIZE + 4096u * F86_OFF_ENTRY) return false;
    }
    if (first < F86_HDR_SIZE + F86_OFF_ENTRY || first > size) return false;
    if ((first - F86_HDR_SIZE) % F86_OFF_ENTRY != 0) return false;

    *entries_out = (first - F86_HDR_SIZE) / F86_OFF_ENTRY;
    return *entries_out > 0;
}

static bool f86_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)fs;
    if (s < F86_HDR_SIZE + F86_OFF_ENTRY) return false;
    if (memcmp(d, F86_MAGIC, F86_MAGIC_LEN) != 0) return false;

    /* Das Magic allein ist vier Bytes. Konfidenz 80..100 verlangt nach
     * der Skala von MF-729 ein getroffenes MERKMAL — also wird zusaetzlich
     * verlangt, dass die Offset-Tabelle aufgeht. Tut sie das nicht, ist es
     * eine Datei mit passenden ersten vier Bytes, kein 86F. */
    uint32_t entries = 0;
    if (!f86_table_extent(d, s, &entries)) {
        *c = 45;    /* nur die Kennung, keine Struktur */
        return true;
    }
    *c = 95;
    return true;
}

static uft_error_t f86_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    size_t file_size = 0;
    uint8_t *data = uft_read_file(path, &file_size);
    if (!data) return UFT_ERROR_FILE_OPEN;
    if (file_size < F86_HDR_SIZE + F86_OFF_ENTRY) {
        free(data); return UFT_ERROR_FILE_OPEN;
    }
    if (memcmp(data, F86_MAGIC, F86_MAGIC_LEN) != 0) {
        free(data); return UFT_ERROR_FORMAT_INVALID;
    }

    uint32_t entries = 0;
    if (!f86_table_extent(data, file_size, &entries)) {
        free(data); return UFT_ERROR_FORMAT_INVALID;
    }

    f86_pd_t *p = calloc(1, sizeof(f86_pd_t));
    if (!p) { free(data); return UFT_ERROR_NO_MEMORY; }

    p->data        = data;
    p->size        = file_size;
    p->disk_flags  = uft_read_le16(data + 6);
    p->entry_count = entries;
    p->sides       = (p->disk_flags & F86_DF_TWO_SIDES) ? 2 : 1;

    /* Belegte Eintraege zaehlen — daraus, und NUR daraus, kommt die
     * Zylinderzahl. Der Container traegt keine Typangabe, aus der man
     * eine Geometrie ableiten koennte; wer eine erfindet, erfindet Daten. */
    for (uint32_t i = 0; i < entries; i++) {
        const size_t at = F86_HDR_SIZE + (size_t)i * F86_OFF_ENTRY;
        if (at + F86_OFF_ENTRY > file_size) break;
        if (uft_read_le32(data + at) != 0) p->used_count++;
    }
    p->cylinders = (uint16_t)(p->used_count / p->sides);

    disk->plugin_data          = p;
    disk->geometry.cylinders   = p->cylinders;
    disk->geometry.heads       = p->sides;
    /* Sektorzahl und -groesse stehen NICHT im Container. Sie ergeben
     * sich erst beim Dekodieren einer Spur; bis dahin bleiben sie 0.
     * Eine geratene Vorbelegung waere eine Behauptung ueber eine
     * Diskette, die noch niemand gelesen hat. */
    disk->geometry.sectors     = 0;
    disk->geometry.sector_size = 0;
    disk->geometry.total_sectors = 0;
    return UFT_OK;
}

static void f86_close(uft_disk_t *disk) {
    f86_pd_t *p = disk->plugin_data;
    if (p) { free(p->data); free(p); disk->plugin_data = NULL; }
}

/* ── Eine Spur ─────────────────────────────────────────────────────── */

static uft_error_t f86_read_track(uft_disk_t *disk, int cyl, int head,
                                  uft_track_t *track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen gerechnet
     * oder indiziert wird. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    f86_pd_t *p = disk->plugin_data;
    if (!p || !p->data) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    if (head >= p->sides) return UFT_OK;
    const uint32_t idx = (uint32_t)cyl * p->sides + (uint32_t)head;
    if (idx >= p->entry_count) return UFT_OK;

    const size_t at = F86_HDR_SIZE + (size_t)idx * F86_OFF_ENTRY;
    if (at + F86_OFF_ENTRY > p->size) return UFT_OK;
    const uint32_t toff = uft_read_le32(p->data + at);
    if (toff == 0) return UFT_OK;              /* Spur nicht vorhanden */

    /* Spurkopf. Das Zellzahl-Feld gibt es nur, wenn Disk-Flag Bit 7
     * gesetzt ist; Bit 12 deutet die Zahl als GESAMTzahl statt als
     * Zusatz. Ohne Bit 7 gibt es keine Zellzahl im Container. */
    const bool has_count = (p->disk_flags & F86_DF_BITCELL) != 0;
    const size_t hdr_len = has_count ? 0x0Au : 0x06u;
    if (toff + hdr_len > p->size) return UFT_OK;

    const uint16_t tflags = uft_read_le16(p->data + toff);
    uint32_t bitcells = 0;
    if (has_count) {
        const int32_t v = (int32_t)uft_read_le32(p->data + toff + 2);
        /* Ohne Bit 12 ist der Wert ein ZUSATZ und kann negativ sein.
         * Diesen Baum interessiert die Gesamtzahl; ohne Bit 12 fehlt
         * die Bezugsgroesse, also wird nicht geraten. */
        if (!(p->disk_flags & F86_DF_INTERP)) return UFT_OK;
        if (v <= 0) return UFT_OK;
        bitcells = (uint32_t)v;
    } else {
        return UFT_OK;   /* ohne Zellzahl keine Laenge, kein Raten */
    }

    const size_t bytes = ((size_t)bitcells + 7u) / 8u;
    const size_t data_at = toff + hdr_len;
    if (bytes == 0 || data_at + bytes > p->size) return UFT_OK;

    /* Nur MFM. Fuer FM, M2FM und GCR hat dieser Baum keinen
     * Bitstrom-Sektordekoder — eine Spur so zu behandeln, als waere sie
     * MFM, ergaebe Sektoren, die nicht auf der Diskette stehen. */
    const unsigned enc = (tflags >> F86_TF_ENC_SHIFT) & F86_TF_ENC_MASK;
    if (enc != F86_TF_ENC_MFM) return UFT_OK;

    /* Der Bitstrom liegt MSB zuerst — an der Synchronmarke 0x4489
     * gemessen (54 Treffer MSB zuerst, 2 bei LSB). Genau das erwartet
     * `uft_mfm_decode_track()`; es wird nichts umgedreht. */
    uft_mfm_sector_t recs[F86_MAX_SECTORS];
    const size_t pool_size = (size_t)F86_MAX_SECTORS * 1024u;
    uint8_t *pool = (uint8_t *)malloc(pool_size);
    if (!pool) return UFT_ERROR_NO_MEMORY;

    const size_t n = uft_mfm_decode_track(p->data + data_at, bitcells,
                                          pool, pool_size,
                                          recs, F86_MAX_SECTORS, NULL);

    for (size_t i = 0; i < n; i++) {
        const uft_mfm_sector_t *r = &recs[i];
        if (!r->dam_present || r->data_len == 0) continue;
        /* `uft_format_add_sector()` nimmt einen 0-BASIERTEN Laufindex und
         * addiert 1 — so steht es in `uft_format_common.h`. `r->sector`
         * ist aber bereits die Nummer, wie sie auf der Diskette steht
         * (IBM CHRN.R, 1-basiert). Der Unterschied ist genau eins, und
         * ein erster Entwurf hier hat ihn gemacht: die echte Spur kam
         * als R=2..10 statt R=1..9 heraus. Deshalb die Fassung, die die
         * ID nimmt, wie sie ist. */
        uft_format_add_sector_with_id(track, r->sector,
                                      pool + r->data_offset,
                                      (uint16_t)r->data_len,
                                      r->cylinder, r->head);
    }

    free(pool);
    return UFT_OK;
}

static uft_error_t f86_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track) {
    /* MF-529: negative Koordinaten abweisen, BEVOR mit ihnen gerechnet
     * oder indiziert wird. Beim Schreiben wiegt das schwerer als beim
     * Lesen: ein falscher Index bestimmt, WOHIN geschrieben wird. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    f86_pd_t *p = disk->plugin_data;
    if (!p || !p->data) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    /* MF-883: Hier stand eine Speicher-Mutation, die `UFT_OK` meldete.
     *
     * Gemessen: in dieser Datei steht keine einzige Schreiboperation
     * (`fwrite`/`fputc`/`fprintf`/`ftruncate`/`WriteFile`), das Plugin hat
     * kein `.flush`, und `close()` gibt den Puffer frei. Kein Byte hat je
     * die Platte erreicht — der Aufrufer bekam Erfolg gemeldet.
     *
     * MF-961 aendert daran nichts, und das ist Absicht. Ein 86F-Schreiber
     * muesste Sektoren nach MFM ZURUECK-kodieren und in den Bitstrom
     * einpassen; dieser Baum hat mit `uft_mfm_encoder.c` zwar einen
     * IBM-MFM-Kodierer, aber die Einpassung in eine vorhandene Spur mit
     * ihrer Zellzahl und Indexlochlage ist eine eigene Aufgabe mit
     * eigenem Rotbeweis. Sie ohne einen solchen zu bauen waere genau das,
     * was die EINFRIER-REGEL verbietet.
     *
     * `write_track` bleibt GESETZT statt NULL: ein Nullzeiger gaebe dem
     * Aufrufer keine Begruendung. */
    (void)track;
    return UFT_ERROR_NOT_SUPPORTED;
}

static const uft_plugin_feature_t uft_format_plugin_86f_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED,
      "MF-961: Container nach 86Box-Spezifikation, MFM-Spuren ueber "
      "uft_mfm_decode_track(); an einer echten 360K-Datei abgenommen" },
    { "Write", UFT_FEATURE_UNSUPPORTED,
      "MF-883: schreibt nur in den Speicher — kein fwrite in der Datei, kein flush, close() gibt frei" },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED,
      "MF-961: 86F traegt Bitzellen, keinen Fluss. Die frueher gemeldete "
      "Flux-Zusage war unbelegt — Zeitinformation je Wechsel steht nicht "
      "im Container" },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED,
      "MF-961: 86F KANN sie tragen (Disk-Flag Bit 0, Oberflaechen-"
      "beschreibung), dieser Leser wertet sie noch nicht aus" },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_86f = {
    .name = "86F", .description = "86Box/PCem Floppy Image",
    .extensions = "86f", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_VERIFY,
    .probe = f86_probe, .open = f86_open,
    .close = f86_close, .read_track = f86_read_track,
    .write_track = f86_write_track,
    .verify_track = uft_generic_verify_track,
    /* MF-961: nicht mehr rueckentwickelt — die Struktur stammt aus der
     * Spezifikation des URHEBERS (86Box/docs, dev/formats/86f.rst).
     *
     * PARTIAL und nicht FULL, mit Grund: die Beschreibung nennt die
     * Laenge der Spur-Offset-Tabelle nicht. Sie ist hier aus dem ersten
     * Spur-Offset abgeleitet und an einer echten Datei bestaetigt (Ende
     * der letzten Spur == Dateigroesse), aber abgeleitet bleibt
     * abgeleitet. */
    .spec_status = UFT_SPEC_OFFICIAL_PARTIAL,
    .features = uft_format_plugin_86f_features,
    .feature_count = sizeof(uft_format_plugin_86f_features) / sizeof(uft_format_plugin_86f_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(86f)
