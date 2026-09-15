/**
 * @file uft_ssd_plugin.c
 * @brief SSD/DSD (BBC Micro) Plugin
 *
 * SSD = Single-Sided Disk image, DSD = Double-Sided.
 * Headerless raw 256-byte sectors, 10 sectors/track.
 *
 * Geometry:
 *   SSD: 80 × 1 × 10 × 256 = 204,800 (or 40 tracks = 102,400)
 *   DSD: 80 × 2 × 10 × 256 = 409,600 (or 40 tracks = 204,800)
 *
 * DSD interleave: track 0 side 0, track 0 side 1, track 1 side 0, ...
 *
 * Reference: BeebWiki SSD/DSD format
 */

#include "uft/uft_format_common.h"
#include "uft/uft_log.h"

#define SSD_SECTOR_SIZE     256

/* ── HADFS-Kennung (MF-836) ───────────────────────────────────────────────
 *
 * HADFS (J. G. Harston, BBC/Electron/Master) schreibt beim ANLEGEN einer
 * Diskette einen ECHTEN DFS-Katalog in die Sektoren 0/1 — als
 * Kompatibilitaetsmassnahme. Quelle: HADFS 6.10 Quelltext, `S.HADFS8`
 * §InstDFS:
 *
 *     LDA FSM+31:AND #&41:BNE InstNoDFS   \ Don't overwrite sectors 0/1
 *
 * Nur bei Flag-Bit 0 (NoDFS) oder Bit 6 (Locked) bleiben 0/1 unberuehrt.
 * Im Normalfall stehen dort Titel „HADFS" und die Eintraege `!Boot`, `$`,
 * `HADFSROM` (`S.HADFS9` §BootCode).
 *
 * Die Sonde unten prueft genau diesen Katalog und meldete darauf **85**,
 * also nach der Skala aus MF-729 „Merkmal getroffen". Getroffen wurde
 * aber ein Merkmal, das HADFS ABSICHTLICH hinterlegt hat; das eigentliche
 * Dateisystem beginnt bei Sektor 71 und blieb unsichtbar. Kein Absturz,
 * keine Warnung — ein plausibles falsches Ergebnis.
 *
 * HADFS prueft sich selbst an acht Byte (`S.HADFS9` §ChkJGH gegen
 * §JGHName; Puffer bei &F00, verglichen ab &F10 = Offset 16):
 *
 *     Sektor 70, Byte 16..23 = 00 28 43 29 4A 47 48 00   („\0(C)JGH\0")
 *
 * Dateioffset 70*256 + 16 = 0x4610, innerhalb von
 * UFT_PROBE_BUFFER_SIZE (65536).
 *
 * WAS HIER NICHT PASSIERT: HADFS wird nicht GELESEN. Ein neuer
 * Dateisystem-Leser fiele unter das Moratorium (EINFRIER-REGEL
 * MF-363/498). Diese Aenderung nimmt nur einen falschen Anspruch
 * zurueck — die Groesse stimmt weiter, das Merkmal wird nicht mehr
 * behauptet. */
#define SSD_HADFS_FSM_SECTOR  70u
#define SSD_HADFS_SIG_OFFSET  16u
#define SSD_HADFS_SIG_AT      (SSD_HADFS_FSM_SECTOR * SSD_SECTOR_SIZE \
                               + SSD_HADFS_SIG_OFFSET)   /* 0x4610 */

static const uint8_t SSD_HADFS_SIG[8] = {
    0x00, '(', 'C', ')', 'J', 'G', 'H', 0x00
};

/** true, wenn der Puffer die HADFS-Kennung an ihrer Stelle traegt.
 *
 *  Reicht der Puffer nicht bis dorthin, ist die Frage unbeantwortbar —
 *  und eine unbeantwortbare Frage darf keine Antwort erzwingen: dann
 *  false, also keine Herabsetzung. */
static bool ssd_traegt_hadfs_kennung(const uint8_t *data, size_t size)
{
    if (!data || size < SSD_HADFS_SIG_AT + sizeof(SSD_HADFS_SIG))
        return false;
    return memcmp(data + SSD_HADFS_SIG_AT, SSD_HADFS_SIG,
                  sizeof(SSD_HADFS_SIG)) == 0;
}
#define SSD_SPT             10

typedef struct {
    FILE*       file;
    uint8_t     cylinders;
    uint8_t     heads;
    bool        interleaved;    /* DSD: sides interleaved per track */
} ssd_data_t;

static bool ssd_detect(size_t file_size, uint8_t *cyl, uint8_t *heads)
{
    uint32_t track_size = SSD_SPT * SSD_SECTOR_SIZE;
    if (file_size == 0 || file_size % track_size != 0) return false;

    uint32_t total = (uint32_t)(file_size / track_size);
    if (total == 40)  { *cyl = 40; *heads = 1; return true; }
    if (total == 80)  { *cyl = 80; *heads = 1; return true; }
    if (total == 160) { *cyl = 80; *heads = 2; return true; }
    return false;
}

static bool uft_ssd_plugin_probe(const uint8_t *data, size_t size, size_t file_size,
               int *confidence)
{
    uint8_t cyl, heads;
    if (!ssd_detect(file_size, &cyl, &heads)) return false;
    /* MF-1153: hier stand `*confidence = 30`, also die Groesse allein.
     * Nach der Doktrin ist die Groesse allein **0** und nie hinreichend;
     * was `ssd_detect()` bestaetigt hat, ist die GEOMETRIE — 40, 80 oder
     * 160 Spursaetze zu 10 x 256 Byte. Das sind 10. */
    *confidence = uft_probe_konfidenz(UFT_BELEG_GEOMETRIE);

    /* BBC Micro DFS catalog: sector 1 bytes 0x100-0x107 contain
     * the disk title (continued from sector 0). Sector 1 byte 0x104
     * holds sector count (low byte), typically 0x20 for 40-track. */
    /* ── MF-1146: EIN Byte reichte fuer das Merkmalsband ──────────────
     *
     * Hier stand:
     *
     *     if (sec_count_lo == 0x90 || == 0x20 || == 0xA0) *confidence = 82;
     *     if ((data[0x106] >> 4) <= 3 && sec_count_lo > 0) *confidence = 85;
     *
     * Die erste Bedingung prueft EIN Byte gegen drei Werte — und einer
     * davon ist **0x20, das Leerzeichen**, das haeufigste Byte in jedem
     * Text. Gefunden hat es Eichung 3 (`test_probe_confidence_on_text`)
     * beim ersten Lauf: auf einem selbstbenennenden Pruefpuffer traegt
     * Versatz 0x107 das achte Byte der Marke `UFT-API C01 H1 S01 `,
     * also ein Leerzeichen, und `ssd` meldete **82** — Band „Merkmal
     * getroffen" (MF-729).
     *
     * Dieselbe Bauform wie bei `img_probe()` in MF-1144: die Stufen
     * wurden ZUGEWIESEN statt gesammelt, und eine Ein-Byte-Pruefung
     * erreichte das Band, das einer Kennung gehoert.
     *
     * **Und es hatte eine Folge, die benannt gehoert:** in MF-1144 hat
     * `ssd` sein eigenes 204 800-Byte-Abbild gegen IMG gewonnen. Die
     * Geometrie dieses Siegs ist richtig (80 x 1 x 10 x 256), der GRUND
     * war es nicht — entschieden hat ein Leerzeichen.
     *
     * Jetzt ist das Tor zum Merkmalsband die Uebereinstimmung ZWEIER
     * unabhaengiger Katalogfelder; ein einzelnes Feld traegt „Struktur
     * gelesen". Eine echte DFS-Diskette verliert nichts: sie hat eine
     * gueltige Sektorzahl UND eine gueltige Bootoption und kommt damit
     * unveraendert auf 85. */
    /* ── MF-1152: das Sektorzahlfeld ist ZEHN Bit breit ───────────────
     *
     * Hier stand `sec_count_lo == 0x90 || == 0x20 || == 0xA0` — also
     * **acht** Bit gegen drei Werte. Die Sektorzahl eines DFS-Katalogs
     * steht aber in den unteren zwei Bit von 0x106 UND den acht von
     * 0x107; das obere Nibble von 0x106 ist die Bootoption:
     *
     *     Sektoren   = ((data[0x106] & 0x03) << 8) | data[0x107]
     *     Bootoption = (data[0x106] >> 4) & 0x03
     *
     * **Und dieses Wissen stand schon im Baum — im Kommentar von
     * `tests/test_ssd_hadfs_nicht_dfs.c` (MF-836):** „400 Sektoren =
     * 0x190 -> High-Bits 0x01 bei 0x106, Low 0x90 bei 0x107", und seine
     * Pruefdatei setzt es genau so. Der Kommentar wusste es, der Code
     * nicht — woertlich die Gestalt von MF-1020 bei `mfi`.
     *
     * **Die Folge war gemessen (A4 Runde 2, MF-1150):** eine
     * TRS-80-Diskette von MAMEs floptool traegt bei 0x105/0x106/0x107
     * die Byte `56 31 20` und erreichte damit **85**, also das Band
     * „Merkmal getroffen". 0x20 ist das Leerzeichen — genau das Byte,
     * das MF-1146 als Ursache benannt hat —, und 0x31 ('1') hat das
     * obere Nibble 3. Mit den zehn Bit gelesen sagt dieser Katalog
     * **288** Sektoren: kein Vielfaches von zehn, und 288 x 256 sind
     * weniger als die 204 800 Byte der Datei.
     *
     * **Und die alten „zwei Felder" waren nicht unabhaengig:** die
     * zweite Bedingung verlangte zusaetzlich `sec_count_lo > 0`, was die
     * erste schon garantierte (0x90, 0x20, 0xA0 sind alle > 0). Neu
     * geprueft wurde allein `data[0x106] <= 0x3F`. Die Formulierung aus
     * MF-1146 traegt damit nicht; das ist eine eigene Angabe, und sie
     * ist hier berichtigt.
     *
     * Die Feldwerte sind an einem ECHTEN Katalog abgenommen:
     * DiscImageManagers `Blank Images/Acorn DFS/` (Gerald Holdsworth,
     * GPL-3, seit MF-693 als DATENQUELLE registriert) fuehrt in
     * Sektor 1 `00 00 00 00 00 00 03 20` — 0 Dateien, 800 Sektoren,
     * Bootoption 0. `tests/test_ssd_katalog_zehn_bit.c` haelt es fest;
     * Rotbeweis zuerst, 6 gruen / 4 rot vor dieser Aenderung. */
    if (size >= 0x108) {
        const unsigned sektoren =
            ((unsigned)(data[0x106] & 0x03) << 8) | (unsigned)data[0x107];
        const unsigned bootopt  = (unsigned)(data[0x106] >> 4);
        const unsigned dateien  = (unsigned)data[0x105];

        /* Zehn Sektoren je Spur — dieselbe Annahme, mit der
         * `ssd_detect()` oben `file_size / 2560` rechnet. Und die Ansage
         * kann nicht KLEINER sein als die Diskette: bei einer
         * doppelseitigen DSD fuehrt jede Seite ihren eigenen Katalog mit
         * ihrer eigenen Sektorzahl, deshalb `* heads`. */
        const bool zahl_ok = (sektoren > 0 && (sektoren % 10u) == 0
                              && (size_t)sektoren * 256u * (size_t)heads
                                 >= file_size);
        /* Ein DFS-Verzeichniseintrag ist ACHT Byte lang; bei 0x105 steht
         * Dateizahl x 8, und es gibt hoechstens 31 Dateien. */
        const bool datei_ok = ((dateien % 8u) == 0 && dateien <= 248u);
        /* Vier Bootoptionen (0..3) im oberen Nibble. */
        const bool boot_ok  = (bootopt <= 3u);

        /* ── Die Konfidenz wird ABGELEITET (MF-1153) ──────────────────
         *
         * Hier stand 85 bzw. 60. Die 85 war ein Handwert, und sie stand
         * im Band „Merkmal getroffen" fuer ein Format, das **keine
         * Kennung hat** — MF-1152 hat das Tor davor verschaerft, aber
         * die Zahl nicht angetastet. Am selben Tag hat MF-1151 `dmk`
         * aus demselben Grund von 100 auf 75 gesenkt: zwei Faelle, zwei
         * Begruendungen. Der Eigentuemer hat die Frage einmal
         * beantwortet (`docs/SONDEN_DOKTRIN.md`).
         *
         * Was diese Sonde WIRKLICH liest, als Belege benannt:
         *
         *   SELBSTKONS. die Sektorzahl aus dem Katalog gegen die
         *               Dateigroesse — `zahl_ok`. Das ist der
         *               Quervergleich, den MF-1152 eingefuehrt hat.
         *   STRUKTUR    der Katalog selbst: Dateizahl ein Vielfaches
         *               von acht UND eine der vier Bootoptionen. Beide
         *               Felder gehoeren demselben Verzeichnis, also
         *               EIN Beleg — nicht zwei.
         *   GEOMETRIE   `ssd_detect()` hat 40, 80 oder 160 Spursaetze
         *               zu 10 x 256 Byte bestaetigt
         *   KENNUNG     hat Acorn DFS nicht. Klemme bei 45.
         *
         * **Und der Nullpuffer bleibt der Pruefstein:** dort sind
         * `datei_ok` (0 % 8) und `boot_ok` (0 <= 3) beide wahr, aber
         * `zahl_ok` ist falsch (Sektorzahl 0), also STRUKTUR|GEOMETRIE
         * = 25 — Band „kein Anspruch". Eichung 1 hatte diese Sonde
         * schon einmal rot gemacht, als eine Vorfassung dafuer 60 gab. */
        {
            unsigned belege = UFT_BELEG_GEOMETRIE;
            if (zahl_ok)             belege |= UFT_BELEG_SELBSTKONSISTENZ;
            if (datei_ok && boot_ok) belege |= UFT_BELEG_STRUKTUR;
            *confidence = uft_probe_konfidenz(belege);
        }
    }

    /* MF-836: Der Katalog kann ECHT und die Diskette trotzdem kein
     * DFS-Volume sein — HADFS legt ihn absichtlich dort ab. Traegt
     * Sektor 70 die HADFS-Kennung, wird der Anspruch auf das gesenkt,
     * was dann noch stimmt: die Groesse.
     *
     * Bewusst KEIN `return false` — die Datei IST ein Acorn-Abbild mit
     * 10 Sektoren zu 256 Byte, und ihr den Zugriff zu verweigern waere
     * schlechter als eine ehrliche niedrige Zahl (MF-830).
     *
     * **MF-1153:** hier stand `*confidence = 30`, die Skalenstufe „nur
     * die Groesse". Auch das war ein Handwert — nach der Doktrin ist die
     * Groesse allein 0, und was bei einer HADFS-Diskette noch stimmt,
     * ist die GEOMETRIE: zehn Sektoren zu 256 Byte auf 40, 80 oder 160
     * Spursaetzen. Das sind **10**, und der Abstand zum DFS-Fall (45)
     * bleibt derselbe Gedanke wie in MF-836, nur abgeleitet. */
    if (ssd_traegt_hadfs_kennung(data, size)) {
        UFT_WARN("SSD: Sektor 70 traegt die HADFS-Kennung \"(C)JGH\" — "
                 "der DFS-Katalog in Sektor 0/1 ist HADFS' "
                 "Kompatibilitaetseintrag, kein eigenstaendiges "
                 "DFS-Volume; das Dateisystem beginnt bei Sektor 71");
        *confidence = uft_probe_konfidenz(UFT_BELEG_GEOMETRIE);
    }
    return true;
}

static uft_error_t ssd_open(uft_disk_t *disk, const char *path, bool ro)
{
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long fs = ftell(f);
    if (fs < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    uint8_t cyl, heads;
    if (!ssd_detect((size_t)fs, &cyl, &heads)) { fclose(f); return UFT_ERROR_FORMAT_INVALID; }

    ssd_data_t *p = calloc(1, sizeof(ssd_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f; p->cylinders = cyl; p->heads = heads;
    p->interleaved = (heads == 2);

    disk->plugin_data = p;
    disk->geometry.cylinders = cyl;
    disk->geometry.heads = heads;
    disk->geometry.sectors = SSD_SPT;
    disk->geometry.sector_size = SSD_SECTOR_SIZE;
    disk->geometry.total_sectors = (uint32_t)cyl * heads * SSD_SPT;
    return UFT_OK;
}

static void ssd_close(uft_disk_t *disk)
{
    ssd_data_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t ssd_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track)
{
    ssd_data_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    long offset;
    if (p->interleaved) {
        /* DSD: cyl0/side0, cyl0/side1, cyl1/side0, ... */
        offset = (long)((cyl * 2 + head) * SSD_SPT * SSD_SECTOR_SIZE);
    } else {
        offset = (long)(cyl * SSD_SPT * SSD_SECTOR_SIZE);
    }

    if (fseek(p->file, offset, SEEK_SET) != 0) return UFT_ERROR_IO;

    uint8_t buf[SSD_SECTOR_SIZE];
    for (int s = 0; s < SSD_SPT; s++) {
        if (fread(buf, 1, SSD_SECTOR_SIZE, p->file) != SSD_SECTOR_SIZE)
            return UFT_ERROR_IO;
        uft_format_add_sector(track, (uint8_t)s, buf, SSD_SECTOR_SIZE,
                              (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

static uft_error_t ssd_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track) {
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

    ssd_data_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    long offset = p->interleaved ?
        (long)((cyl * 2 + head) * SSD_SPT * SSD_SECTOR_SIZE) :
        (long)(cyl * SSD_SPT * SSD_SECTOR_SIZE);
    for (size_t s = 0; s < track->sector_count && (int)s < SSD_SPT; s++) {
        if (fseek(p->file, offset + (long)s * SSD_SECTOR_SIZE, SEEK_SET) != 0)
            return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[SSD_SECTOR_SIZE];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, SSD_SECTOR_SIZE); data = pad;
        }
        if (fwrite(data, 1, SSD_SECTOR_SIZE, p->file) != SSD_SECTOR_SIZE)
            return UFT_ERROR_IO;
    }
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_ssd_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_ssd = {
    .name = "SSD", .description = "BBC Micro SSD/DSD",
    .extensions = "ssd;dsd", .format = UFT_FORMAT_SSD,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = uft_ssd_plugin_probe, .open = ssd_open, .close = ssd_close,
    .read_track = ssd_read_track, .write_track = ssd_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_DERIVED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_ssd_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_ssd_features) / sizeof(uft_format_plugin_ssd_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(ssd)
