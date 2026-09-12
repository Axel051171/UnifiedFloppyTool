/**
 * @file uft_cas.c
 * @brief MSX CAS Cassette Image Plugin
 *
 * CAS is the standard cassette tape image format for MSX computers.
 * Files consist of header blocks (8-byte signature) followed by data.
 *
 * MSX CAS header signature: 1F A6 DE BA CC 13 7D 74
 *
 * Block types (identified by 10-byte header after CAS sig):
 *   - Binary:   header D0 D0 D0 D0 D0 D0 D0 D0 D0 D0
 *   - BASIC:    header D3 D3 D3 D3 D3 D3 D3 D3 D3 D3
 *   - ASCII:    header EA EA EA EA EA EA EA EA EA EA
 *   - Custom:   header varies
 *
 * Since CAS is a tape format (not disk), we represent the data as a
 * single-track image with one virtual sector per CAS block.
 *
 * ── Die Referenz ─────────────────────────────────────
 *
 * MAMEs `formats/fmsx_cas.cpp` (BSD-3-Clause, Sean Young) — **nur
 * gelesen**, Kanal *Spec* nach MF-695. Es sagt drei Dinge woertlich:
 *
 *     static const uint8_t CasHeader[8] =
 *         { 0x1F,0xA6,0xDE,0xBA,0xCC,0x13,0x7D,0x74 };
 *
 *     if (caslen < 8) return -1;
 *     if (memcmp (casdata, CasHeader, sizeof (CasHeader))) return -1;
 *
 * Die Datei muss also mit dem Kopf **beginnen**; danach laeuft MAME
 * **byteweise** und beginnt ueberall dort einen neuen Block, wo der Kopf
 * wieder auftaucht. UFTs Leser tut beides genauso — das ist gemessen
 * und stimmt.
 *
 * ── Was NICHT stimmte: dreimal ein stiller Verlust (MF-1040) ─────
 *
 * **(1) Mehr als 256 Bloecke fielen weg.** `CAS_MAX_BLOCKS` ist 256, und
 * der Suchlauf hoerte dort einfach auf. Gemessen an einer Datei mit 300
 * Bloecken: `open` = 0, **256** Sektoren, Summe **10 240 statt 12 352
 * Byte** — 44 Bloecke und 2112 Byte still verloren, ohne ein Wort.
 * Dieselbe Klasse wie die sechs Befunde aus MF-1004: das Orakel bricht
 * ab, UFT kuerzte still.
 *
 * **(2) Ein Block ueber 65 535 Byte wurde gekuerzt und als GUTER Sektor
 * gemeldet.** Gemessen an einem Block mit 70 000 Byte Nutzlast: gemeldet
 * wurden **65 535**, Status `UFT_SECTOR_OK` — **4465 Byte** weg. Die
 * Grenze kommt aus UFTs Sektormodell (die Laenge ist 16 Bit), nicht aus
 * dem Format; MAME kennt sie nicht, es gibt den ganzen Strom aus.
 *
 * **(3) Ein Kopf ohne Daten ergab einen 0-Byte-Sektor mit Status OK —
 * und dahinter eine HEAP-KORRUPTION.**
 * Gemessen an einer Datei aus genau den 8 Kopfbytes: ein Sektor,
 * `data_len` = 0, Status `UFT_SECTOR_OK`. (Der Zeiger ist dabei **nicht**
 * NULL — das war eine Vermutung, die die Messung widerlegt hat.)
 *
 * **Und der Fall ist schwerer als ein Schoenheitsfehler.** An einer
 * Datei mit einem vollen Block und einem Kopf am Dateiende gemessen,
 * gegen den Vorzustand uebersetzt:
 *
 *     open = 0, sector_count = 2
 *       Sektor 0: 200 Byte, Status 0
 *       Sektor 1:   0 Byte, Status 0
 *     Rueckgabe des Prozesses: 0xC0000374
 *
 * `0xC0000374` ist `STATUS_HEAP_CORRUPTION`. Ein Sektor der Laenge
 * null, dessen Zeiger auf das Byte HINTER dem Puffer zeigt, hat den
 * Heap zerstoert — der Prozess starb beim Aufraeumen. **Welche
 * Zuteilung genau es war, ist nicht bestimmt**; gemessen ist die
 * Wirkung, und sie reicht: ein Abbild dieser Gestalt liess UFT
 * abstuerzen.
 *
 * Seit MF-1040 wird in allen drei Faellen **abgesagt oder uebersprungen
 * statt gekuerzt**: zu viele Bloecke und ein zu grosser Block lassen
 * `open` mit einer Begruendung scheitern, und leere Bloecke werden nicht
 * als Sektor ausgegeben. „Kein Bit verloren" heisst auch: lieber keine
 * Antwort als eine gekuerzte.
 *
 * ── Eine Abweichung von MAME, benannt statt verschwiegen ────────
 *
 * MAMEs Bedingung ist `if ((pos + 8) < caslen)` — **streng kleiner**.
 * Ein Kopf, der GENAU am Dateiende endet, ist dort also kein Kopf, und
 * MAME gibt die acht Bytes als Banddaten aus. UFT erkennt ihn (`next + 8
 * <= size`), findet dahinter nichts und hat seit MF-1040 damit **null**
 * Bloecke — die Datei wird abgewiesen. Beides ist vertretbar; UFT
 * entscheidet sich gegen das Ausgeben von acht Bytes, die erkennbar eine
 * Marke sind und keine Daten (Grundsatz „keine erfundenen Daten").
 *
 * ── Und was eine Hausregel ist ───────────────────────────
 *
 * CAS ist ein **Band**, kein Datentraeger mit Spuren und Sektoren. Die
 * Abbildung „eine Spur, je Block ein virtueller Sektor" ist UFTs eigene
 * Konvention — wie die 128 x 512 bei `fds` (MF-1038). Sie steht in
 * keiner Quelle, und sie wird hier als Hausregel benannt statt als
 * Formateigenschaft. `geometry.sector_size` meldet 512, obwohl die
 * Bloecke beliebig lang sind; das ist derselbe Behelf.
 */

#include "uft/uft_format_common.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define CAS_HEADER_SIZE     8
#define CAS_MAX_BLOCKS      256
#define CAS_MAX_BLOCK_SIZE  65536

static const uint8_t CAS_MSX_MAGIC[8] = {
    0x1F, 0xA6, 0xDE, 0xBA, 0xCC, 0x13, 0x7D, 0x74
};

/* ============================================================================
 * Plugin data
 * ============================================================================ */

typedef struct {
    uint8_t*    data;           /* entire file in memory */
    size_t      data_size;
    uint32_t    block_offsets[CAS_MAX_BLOCKS]; /* offset of each block's data */
    uint32_t    block_sizes[CAS_MAX_BLOCKS];
    uint16_t    block_count;
} cas_data_t;

/* ============================================================================
 * Block scanner — find all CAS header signatures
 * ============================================================================ */

/**
 * @brief Findet die Bloecke — und zaehlt auch die, die nicht mehr
 *        hineinpassen.
 *
 * MF-1040: vorher hielt die Schleife bei `max_blocks` an und der Rest
 * der Datei fiel **still** weg (gemessen: 300 Bloecke ergaben 256
 * Sektoren und 2112 Byte Verlust). Jetzt laeuft sie weiter und meldet
 * ueber `gesamt`, wie viele es wirklich sind; der Aufrufer sagt ab.
 */
static uint16_t cas_scan_blocks(const uint8_t *data, size_t size,
                                uint32_t *offsets, uint32_t *sizes,
                                uint16_t max_blocks, uint32_t *gesamt)
{
    uint16_t count = 0;
    size_t pos = 0;

    if (gesamt) *gesamt = 0;
    while (pos + CAS_HEADER_SIZE <= size) {
        if (memcmp(data + pos, CAS_MSX_MAGIC, CAS_HEADER_SIZE) == 0) {
            size_t block_start = pos + CAS_HEADER_SIZE;

            /* Find next header or end of file */
            size_t next = block_start;
            while (next + CAS_HEADER_SIZE <= size) {
                if (memcmp(data + next, CAS_MSX_MAGIC, CAS_HEADER_SIZE) == 0)
                    break;
                next++;
            }
            if (next + CAS_HEADER_SIZE > size) next = size;

            if (gesamt) (*gesamt)++;
            if (count < max_blocks) {
                offsets[count] = (uint32_t)block_start;
                sizes[count] = (uint32_t)(next - block_start);
                count++;
            }
            pos = next;
        } else {
            pos++;
        }
    }

    return count;
}

/* ============================================================================
 * probe
 * ============================================================================ */

bool cas_probe(const uint8_t *data, size_t size, size_t file_size,
               int *confidence)
{
    /* MF-1040: hier stand `(void)file_size;` — die Falle aus MF-1029.
     * MAME verlangt `if (caslen < 8) return -1`, und das ist eine
     * Aussage ueber die DATEI, nicht ueber einen Sondenpuffer. Und eine
     * Datei, die nur aus den acht Kopfbytes besteht, traegt keinen
     * Block: sie wird abgewiesen, nicht mit 95 angenommen. */
    if (!data || size < CAS_HEADER_SIZE) return false;
    if (file_size <= CAS_HEADER_SIZE) return false;

    if (memcmp(data, CAS_MSX_MAGIC, CAS_HEADER_SIZE) == 0) {
        /* MF-729: acht feste Byte an Versatz 0 sind ein getroffenes
         * Merkmal (80..100). */
        *confidence = 95;
        return true;
    }

    return false;
}

/* ============================================================================
 * open — read entire file, scan blocks
 * ============================================================================ */

static uft_error_t cas_open(uft_disk_t *disk, const char *path,
                             bool read_only)
{
    (void)read_only;

    size_t file_size = 0;
    uint8_t *file_data = uft_read_file(path, &file_size);
    if (!file_data) return UFT_ERROR_FILE_OPEN;

    if (file_size < CAS_HEADER_SIZE ||
        memcmp(file_data, CAS_MSX_MAGIC, CAS_HEADER_SIZE) != 0) {
        free(file_data);
        return UFT_ERROR_FORMAT_INVALID;
    }

    cas_data_t *pdata = calloc(1, sizeof(cas_data_t));
    if (!pdata) { free(file_data); return UFT_ERROR_NO_MEMORY; }

    pdata->data = file_data;
    pdata->data_size = file_size;
    {
        uint32_t gesamt = 0;
        uint16_t bi;
        pdata->block_count = cas_scan_blocks(file_data, file_size,
                                              pdata->block_offsets,
                                              pdata->block_sizes,
                                              CAS_MAX_BLOCKS, &gesamt);

        /* MF-1040, Befund 1: lieber keine Antwort als eine gekuerzte.
         * Gemessen fielen bei 300 Bloecken 44 davon und 2112 Byte
         * still weg, und `open` meldete UFT_OK. */
        if (gesamt > CAS_MAX_BLOCKS) {
            free(file_data);
            free(pdata);
            return UFT_ERROR_NOT_SUPPORTED;
        }

        /* MF-1040, Befund 2: ein Block, den ein Sektor nicht fassen
         * kann (die Laenge ist 16 Bit), wurde auf 65 535 gekuerzt und
         * als UFT_SECTOR_OK gemeldet — gemessen 4465 Byte weg. */
        for (bi = 0; bi < pdata->block_count; bi++) {
            if (pdata->block_sizes[bi] > 65535u) {
                free(file_data);
                free(pdata);
                return UFT_ERROR_NOT_SUPPORTED;
            }
        }
    }

    /* MF-1040: nicht nur „kein Block", sondern „kein Block MIT INHALT".
     * Eine Datei aus genau den acht Kopfbytes hat einen Block der Laenge
     * null; sie ging vorher mit UFT_OK und **null Sektoren** auf —
     * Erfolg ohne Tat, die Gestalt von MF-1009 (`apridisk`: 1 Spur, 0
     * Sektoren, UFT_OK). */
    {
        uint16_t bi, mit_inhalt = 0;
        for (bi = 0; bi < pdata->block_count; bi++)
            if (pdata->block_sizes[bi] > 0) mit_inhalt++;
        if (mit_inhalt == 0) {
            free(file_data);
            free(pdata);
            return UFT_ERROR_FORMAT_INVALID;
        }
    }

    disk->plugin_data = pdata;
    disk->geometry.cylinders = 1;
    disk->geometry.heads = 1;
    disk->geometry.sectors = pdata->block_count;
    disk->geometry.sector_size = 512; /* virtual */
    disk->geometry.total_sectors = pdata->block_count;

    return UFT_OK;
}

/* ============================================================================
 * close
 * ============================================================================ */

static void cas_close(uft_disk_t *disk)
{
    cas_data_t *pdata = disk->plugin_data;
    if (pdata) {
        free(pdata->data);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

/* ============================================================================
 * read_track — one track, each sector = one CAS block
 * ============================================================================ */

static uft_error_t cas_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track)
{
    cas_data_t *pdata = disk->plugin_data;
    if (!pdata || cyl != 0 || head != 0) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, 0, 0);

    for (int s = 0; s < pdata->block_count; s++) {
        uint32_t off = pdata->block_offsets[s];
        uint32_t sz = pdata->block_sizes[s];

        if (off + sz > pdata->data_size) break;

        /* MF-1040, Befund 3: ein Kopf ohne Daten dahinter ergab einen
         * Sektor mit `data_len` 0 und Status UFT_SECTOR_OK — gemessen
         * an einer Datei aus genau den acht Kopfbytes. Ein Block ohne
         * Inhalt ist kein Sektor; er wird uebersprungen. */
        if (sz == 0) continue;

        /* Ueber 65 535 kommt hier nichts mehr an: `cas_open` hat solche
         * Dateien bereits abgewiesen, statt sie zu kuerzen. */
        uft_format_add_sector(track, (uint8_t)s,
                              pdata->data + off,
                              (uint16_t)sz,
                              0, 0);
    }

    return UFT_OK;
}

/* ============================================================================
 * Plugin registration
 * NOTE: write_track omitted by design — CAS is a linear tape (cassette)
 * image, not a floppy. It has no track/sector addressing, so writing a
 * uft_track_t here has no meaningful mapping.
 * ============================================================================ */

static const uft_plugin_feature_t uft_format_plugin_cas_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_cas = {
    .name         = "CAS",
    .description  = "MSX Cassette Tape Image",
    .extensions   = "cas",
    .version      = 0x00010000,
    .format       = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_VERIFY,
    .probe        = cas_probe,
    .open         = cas_open,
    .close        = cas_close,
    .read_track   = cas_read_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_DERIVED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_cas_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_cas_features) / sizeof(uft_format_plugin_cas_features[0]),
};

UFT_REGISTER_FORMAT_PLUGIN(cas)
