/**
 * @file uft_syn.c
 * @brief SYN (Synclavier): eine Gleichung, die nicht aufgeht (MF-1041)
 *
 * ── Befund 1: die beiden Haelften der Gleichung standen in zwei
 *    verschiedenen Funktionen ───────────────────────────────────
 *
 * Hier stand woertlich:
 *
 *     Synclavier floppy: 77 cyl x 2 heads x 16 spt x 256 = 634880 bytes.
 *
 * **77 x 2 x 16 x 256 sind 630 784.** Die Gleichung im eigenen Kopf ging
 * um genau **eine Spur** (4096 Byte) nicht auf — und ihre zwei Haelften
 * landeten in zwei verschiedenen Funktionen: die Sonde nahm die rechte
 * Seite (634 880), `syn_open()` sagte die linke an (77 x 2 x 16 x 256).
 *
 * Gemessen am Vorzustand:
 *
 *     Datei 634 880 Byte : open = 0, Geometrie 77x2x16x256 = 630 784
 *                          -> 4096 Byte unerreichbar, ohne ein Wort
 *     Datei 630 784 Byte : open = 0, dieselbe Geometrie — passt
 *
 * **Dieselbe Zahl steht schon als offener Punkt im Baum:** P3-260 fuehrt
 * zwei der 49 DSK-Varianten mit „`expected_size` widerspricht der eigenen
 * Geometrie um genau eine Spur (634880 statt 630784)". Es ist dieselbe
 * Verwechslung, nur hier in einem eigenstaendigen Plugin.
 *
 * Seit MF-1041 nimmt die Sonde **die Groesse, die die Geometrie
 * erzeugt** — 630 784 —, und `syn_open()` prueft sie ebenfalls. Welche
 * der beiden Zahlen eine echte Synclavier-Diskette hat, ist **nicht
 * belegt** (siehe unten); belegt ist nur, dass die alte Fassung sich
 * selbst widersprach.
 *
 * ── Befund 2: `open` prueft die Dateigroesse gar nicht ────────────
 *
 * Gemessen: eine **100 Byte** grosse Datei wurde geoeffnet und als
 * 77 x 2 x 16 x 256 angesagt — 630 684 Byte, die es nicht gibt. Jetzt
 * verlangt `syn_open()` dieselbe Groesse wie die Sonde.
 *
 * ── Was hier NICHT belegt ist ────────────────────────────
 *
 * **Dieses Plugin hat keine nachpruefbare Referenz.** Der alte Kopf nannte
 * keine Quelle, und im Baum liegt keine; die Geometrie ist nicht gegen
 * eine fremde Hand abgenommen. `syn` steht deshalb weiter auf **T3**, und
 * diese Aenderung hebt es nicht — sie behebt einen Widerspruch, den das
 * Plugin mit sich selbst hatte. Gefuehrt als **P3-340**.
 *
 * ══ MF-1141: die Referenz ist da, und zwei der drei Masse waren falsch ══
 *
 * P3-340 ist fuer die SEKTORTEILUNG beantwortet. Das offizielle
 * Synclavier-Konto veroeffentlicht die kompletten NED-Originalquellen
 * unter **MIT** (Copyright (c) 2017 Synclavier Digital); gefunden im
 * Streifzug SCOUT-90. Kanal *Spec* nach MF-695 — **gelesen, keine Zeile
 * uebernommen**; die Lizenz erlaubte auch einen Port, fuer eine
 * Geometrieangabe genuegt das Lesen.
 *
 * Drei Belegstellen, jede woertlich:
 *
 *   `Able/XPL/DEVUTIL:463`
 *     dcl maxi_config data public (shl("2",8), 8, 77, shl(8,8) or 2);
 *   `Able/-XPL/SYSLITS:317,321`
 *     s#totcyl lit '2'   -> Feld 2 = Zylinder      -> 77
 *     s#spdtrk lit '3'   -> Feld 3, HOHES Byte     -> 8 Sektoren/Spur
 *   `Able/UTILCAT/MODS/DISKFORM:163,164,381,382`
 *     IF cpmflag THEN CALL DEP(1,0);  -- Laengencode 0 = 128 Byte
 *     ELSE CALL DEP(1,2);             -- Laengencode 2 = 512 Byte
 *     IF ((not cpmflag)&(I^=512)) ... -- "NED FORMAT ERROR"
 *
 * Das native NED-Format (XPL) hat also **8 Sektoren zu 512 Byte**; die
 * CP/M-Option hat 26 zu 128 (`DISKFORM:88`, grosse Diskette). UFT fuehrte
 * **16 zu 256** — keine der beiden.
 *
 * **Und die Summe ging auf, weshalb es niemand bemerkte:**
 *
 *     NED : 77 x 2 x  8 x 512 = 630 784, Spur 4096 Byte
 *     UFT : 77 x 2 x 16 x 256 = 630 784, Spur 4096 Byte
 *
 * Dieselbe Dateigroesse, dieselbe Spurlaenge, dieselben Spurversaetze.
 * Zweite Auflage der Lehre aus MF-1026 und MF-1140 — nur ist der Schaden
 * hier ein anderer und geringer: **kein Byte kommt von der falschen
 * Stelle.** Falsch ist die TEILUNG. Gemessen am Vorzustand mit einer
 * Datei, deren jeder 512-Byte-Sektor sich selbst benennt:
 *
 *     gemeldet: 16 Sektoren/Spur, 256 Byte/Sektor, 2464 gesamt
 *       Sektor  0 beginnt mit "NED C00 H0 S0 "
 *       Sektor  1 beginnt mit ""      <- zweite Haelfte desselben Sektors
 *       Sektor  2 beginnt mit "NED C00 H0 S1 "
 *     NED-Sektoren als GANZE Einheit sichtbar : 0 von 1232
 *
 * Jeder echte Sektor erschien als zwei, NED-Sektor N als UFT-Sektor 2N.
 * Ein linearer Abzug der ganzen Diskette war damit byteweise richtig —
 * und **jede Aussage je Sektor** falsch: CRC-Zuordnung, Fehlmarkierung,
 * `uft_format_mark_last_missing()`, der Sektor-Editor und jede
 * Dateisystemschicht sahen 2464 Einheiten, wo die Diskette 1232 hat.
 *
 * ── Was WEITERHIN nicht belegt ist, und das ist praezise ──────────────
 *
 * **Die Seitenzahl.** `SYN_HEADS` bleibt 2 und ist damit unveraendert
 * ANGENOMMEN, nicht gemessen. NED liest sie zur Laufzeit aus dem
 * Laufwerk (`DISKFORM:64-68`):
 *
 *     write(MOT)=DR; ID=read(MOT);        -- Laufwerks-ID lesen
 *     sides=((ID and "2")<>0);            -- Seitenzahl daraus
 *
 * Sie steht also in **keiner Datei** und in keiner Konfigtabelle; keine
 * Lesung dieser Quelle kann sie entscheiden. Dass 77 x 8 x 512 x **2**
 * genau die Groesse ergibt, die dieses Plugin seit je verlangt, ist ein
 * Hinweis und kein Beleg. DISKFORMs eigene Summenformel laesst beides zu:
 * `buf(c#ll) = numt*sect*(sides+1)-1` (`DISKFORM:294`).
 *
 * **Die Anordnung in der Datei.** DISKFORM beschreibt das PHYSISCHE
 * Format einer Diskette, nicht den Aufbau einer `.syn`-Datei. Der lineare
 * Versatz `(cyl * koepfe + head) * spt * ss` bleibt damit ungeprueft — er
 * ist unveraendert und traegt keinen neuen Anspruch.
 *
 * **Die Stufe bleibt T3.** Eine gelesene Beschreibung ist Kanal *Spec*;
 * T1b verlangt ein Abbild von **fremder Hand**, und im Korpus liegt keine
 * einzige `.syn`. Die Quelle dafuer ist benannt (SCOUT-94: dasselbe
 * MIT-Konto veroeffentlicht `NED_Synclavier_Release_L_DD_Floppies.simg`,
 * 5 242 880 Byte) — aber jenes Repo traegt **keine** Lizenzdatei, also
 * ist der Kanal heute Fundus und nicht Daten/Fixture. P3-340 bleibt
 * offen, jetzt mit der Sektorteilung erledigt und zwei benannten offenen
 * Haelften.
 */
#include "uft/uft_format_common.h"
typedef struct { FILE* file; } syn_data_t;
/* MF-1041: die Groesse, die die eigene Geometrie erzeugt. Vorher stand
 * hier 634880 — eine Spur mehr, als 77 x 2 x 16 x 256 ergibt. */
#define SYN_CYL   77
/* MF-1141: NICHT belegt — NED liest die Seitenzahl aus der Laufwerks-ID
 * (DISKFORM:64-68), sie steht in keiner Datei. Bleibt eine Annahme. */
#define SYN_HEADS  2
/* MF-1141: 8 statt 16, 512 statt 256 — belegt an DEVUTIL:463 (hohes Byte
 * von s#spdtrk) und DISKFORM:164/381 (Laengencode 2 = 512). Das PRODUKT
 * ist unveraendert 4096 Byte je Spur, die Dateigroesse damit ebenfalls. */
#define SYN_SPT    8
#define SYN_SS   512
#define SYN_SIZE ((size_t)SYN_CYL * SYN_HEADS * SYN_SPT * SYN_SS)
/* Spurlaenge in Byte — die Groesse, die sich NICHT geaendert hat. */
#define SYN_SPUR ((long)SYN_SPT * SYN_SS)

bool syn_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)d; (void)s;
    /* MF-729: erkannt ist allein die Dateigroesse — Band 30..49. */
    if (fs == SYN_SIZE) { *c = 35; return true; } return false;
}
static uft_error_t syn_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    long fs;
    syn_data_t *p;
    if (!f) return UFT_ERROR_FILE_OPEN;
    /* MF-1041, Befund 2: hier wurde die Dateigroesse gar nicht geprueft.
     * Gemessen ging eine 100-Byte-Datei auf und bekam eine volle
     * Diskette angesagt. */
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    fs = ftell(f);
    if (fs < 0) { fclose(f); return UFT_ERROR_IO; }
    if ((size_t)fs != SYN_SIZE) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }
    p = calloc(1, sizeof(syn_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f; disk->plugin_data = p;
    disk->geometry.cylinders = SYN_CYL; disk->geometry.heads = SYN_HEADS;
    disk->geometry.sectors = SYN_SPT;
    disk->geometry.sector_size = SYN_SS;
    disk->geometry.total_sectors = SYN_CYL * SYN_HEADS * SYN_SPT;
    return UFT_OK;
}
static void syn_close(uft_disk_t *d) {
    syn_data_t *p = d->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); d->plugin_data = NULL; }
}
static uft_error_t syn_read_track(uft_disk_t *d, int cyl, int head, uft_track_t *t) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    syn_data_t *p = d->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    /* MF-1141: obere Schranke, damit nicht hinter das Dateiende gelesen
     * wird — `open` verlangt genau SYN_SIZE, also sind die Grenzen
     * bekannt. Vorher gab es nur die untere. */
    if (cyl >= SYN_CYL || head >= SYN_HEADS) return UFT_ERROR_INVALID_STATE;

    uft_track_init(t, cyl, head);
    /* MF-1141: dieselben Versaetze wie vorher — 8 x 512 ist wie 16 x 256
     * genau SYN_SPUR. Geaendert hat sich die TEILUNG, nicht die Lage. */
    long off = (long)((uint32_t)cyl * SYN_HEADS + (uint32_t)head) * SYN_SPUR;
    if (fseek(p->file, off, SEEK_SET) != 0) return UFT_ERROR_IO;
    uint8_t buf[SYN_SS];
    for (int s = 0; s < SYN_SPT; s++) {
        if (fread(buf, 1, SYN_SS, p->file) != SYN_SS) return UFT_ERROR_IO;
        uft_format_add_sector(t, (uint8_t)s, buf, SYN_SS,
                              (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}
static uft_error_t syn_write_track(uft_disk_t *d, int cyl, int head,
                                    const uft_track_t *t) {
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

    syn_data_t *p = d->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (d->read_only) return UFT_ERROR_NOT_SUPPORTED;
    /* MF-1141: obere Schranke auch hier — beim Schreiben bestimmt der
     * Index, WOHIN geschrieben wird (MF-529). */
    if (cyl >= SYN_CYL || head >= SYN_HEADS) return UFT_ERROR_INVALID_STATE;

    long off = (long)((uint32_t)cyl * SYN_HEADS + (uint32_t)head) * SYN_SPUR;
    for (size_t s = 0; s < t->sector_count && (int)s < SYN_SPT; s++) {
        if (fseek(p->file, off + (long)s * SYN_SS, SEEK_SET) != 0)
            return UFT_ERROR_IO;
        const uint8_t *data = t->sectors[s].data;
        uint8_t pad[SYN_SS];
        if (!data || t->sectors[s].data_len == 0) {
            memset(pad, 0xE5, SYN_SS); data = pad;
        } else if (t->sectors[s].data_len < SYN_SS) {
            /* MF-1141: ein kurzer Sektor wurde vorher VOLL geschrieben —
             * `data` zeigte auf `data_len` Byte und `fwrite` nahm 512.
             * Mit der Vergroesserung von 256 auf 512 waere das ein Lesen
             * hinter dem Puffer geworden. Jetzt gepolstert. */
            memset(pad, 0xE5, SYN_SS);
            memcpy(pad, data, t->sectors[s].data_len);
            data = pad;
        }
        if (fwrite(data, 1, SYN_SS, p->file) != SYN_SS) return UFT_ERROR_IO;
    }
    return UFT_OK;
}
static const uft_plugin_feature_t uft_format_plugin_syn_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_syn = {
    .name = "SYN", .description = "Synclavier Disk",
    .extensions = "syn", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = syn_probe, .open = syn_open, .close = syn_close,
    .read_track = syn_read_track, .write_track = syn_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_syn_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_syn_features) / sizeof(uft_format_plugin_syn_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(syn)
