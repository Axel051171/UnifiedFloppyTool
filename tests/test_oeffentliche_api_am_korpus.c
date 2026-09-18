/**
 * @file test_oeffentliche_api_am_korpus.c
 * @brief A4 Runde 2: 72 Formate mit einem ECHTEN Korpusabbild durch
 *        `uft_disk_open()` (MF-1150)
 *
 * ── Warum eine zweite Runde ──────────────────────────────────────────
 *
 * MF-1144 hat die A4-Achse aufgemacht und dabei einen lebenden Defekt
 * gefunden: von zwoelf kopflosen Formaten erreichte **genau eines**
 * (`northstar`) sein eigenes Abbild ueber `uft_disk_open()`. Die
 * Pruefdateien dort sind hauseigen und synthetisch — das war der
 * schnellste Weg und ist der schwaechere Beleg.
 *
 * Diese Runde fragt dieselbe Frage an dem, was wirklich im Korpus liegt:
 * **je Format das groesste Abbild aus `tests/corpus_free/`**, und die
 * meisten davon sind von fremder Hand (VICE, hxcfe, libdsk, floptool,
 * samdisk, gw, epstool, atrcopy, atrip, xdftool, to_woz2, a2nibblize,
 * adf2dms, cpmtools, fluxfox). Die Auswahl ist ABGELEITET aus
 * `tests/corpus_manifest/manifest.json`, nicht getippt (MF-636);
 * `fat12_fs` (ein Dateisystem) und `none` (eine Gegenprobe ohne
 * Kennung) fallen heraus, weil sie kein Plugin haben.
 *
 * ── Was hier geprueft wird, und was ausdruecklich nicht ──────────────
 *
 * Geprueft wird **eine** Frage, und es ist die A4-Frage:
 *
 *   1. `uft_disk_open()` liefert einen Griff — das Format ist ueber die
 *      oeffentliche API ueberhaupt erreichbar.
 *   2. Der Griff gehoert dem Plugin, dem das Abbild laut Manifest
 *      gehoert — die Erkennung waehlt also nicht nur irgendetwas.
 *
 * **NICHT geprueft wird der INHALT.** Ob die Sektoren an ihrer Stelle
 * stehen, ist Sache des Formattests je Format (`test_<fmt>_gegen_<hand>`);
 * hier geht es allein um die Tuer. Und **nicht die Geometrie gegen die
 * Dateigroesse**: bei einem Behaelterformat mit Kopf (IMD, TD0, EDSK, …)
 * ist die Datei groesser als das Sektorraster, die Rechnung aus MF-1144
 * gilt dort nicht.
 *
 * ── Zusage 2 ist eine Messung, keine Erwartung ───────────────────────
 *
 * Bei gleicher Dateigroesse gewinnt EIN Plugin, und mehrere Formate
 * teilen sich eine Groesse — das ist in diesem Baum vielfach gemessen
 * (MF-729, MF-1144, MF-1146, P3-401, P3-402). Wo ein Abbild deshalb
 * NICHT an sein eigenes Plugin geht, wird die Zeile mit dem gemessenen
 * Sieger und dem Grund beschriftet statt entfernt (MF-699/MF-1077).
 * Eine Ausnahme ohne Grund gibt es nicht.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_core.h"            /* uft_disk_open / uft_disk_close */
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

uft_error_t uft_register_all_formats(void);

extern const uft_format_plugin_t uft_format_plugin_2img;
extern const uft_format_plugin_t uft_format_plugin_86f;
extern const uft_format_plugin_t uft_format_plugin_adf;
extern const uft_format_plugin_t uft_format_plugin_adf_arc;
extern const uft_format_plugin_t uft_format_plugin_adl;
extern const uft_format_plugin_t uft_format_plugin_apridisk;
extern const uft_format_plugin_t uft_format_plugin_atr;
extern const uft_format_plugin_t uft_format_plugin_cas;
extern const uft_format_plugin_t uft_format_plugin_cfi;
extern const uft_format_plugin_t uft_format_plugin_cpm;
extern const uft_format_plugin_t uft_format_plugin_cqm;
extern const uft_format_plugin_t uft_format_plugin_d13;
extern const uft_format_plugin_t uft_format_plugin_d64;
extern const uft_format_plugin_t uft_format_plugin_d67;
extern const uft_format_plugin_t uft_format_plugin_d71;
extern const uft_format_plugin_t uft_format_plugin_d77;
extern const uft_format_plugin_t uft_format_plugin_d80;
extern const uft_format_plugin_t uft_format_plugin_d81;
extern const uft_format_plugin_t uft_format_plugin_d82;
extern const uft_format_plugin_t uft_format_plugin_d88;
extern const uft_format_plugin_t uft_format_plugin_dc42;
extern const uft_format_plugin_t uft_format_plugin_dcm;
extern const uft_format_plugin_t uft_format_plugin_dim;
extern const uft_format_plugin_t uft_format_plugin_dim_atari;
extern const uft_format_plugin_t uft_format_plugin_dmk;
extern const uft_format_plugin_t uft_format_plugin_dms;
extern const uft_format_plugin_t uft_format_plugin_do;
extern const uft_format_plugin_t uft_format_plugin_dsk_cpc;
extern const uft_format_plugin_t uft_format_plugin_edk;
extern const uft_format_plugin_t uft_format_plugin_edsk;
extern const uft_format_plugin_t uft_format_plugin_fds;
extern const uft_format_plugin_t uft_format_plugin_g64;
extern const uft_format_plugin_t uft_format_plugin_g71;
extern const uft_format_plugin_t uft_format_plugin_hfe;
extern const uft_format_plugin_t uft_format_plugin_imd;
extern const uft_format_plugin_t uft_format_plugin_img;
extern const uft_format_plugin_t uft_format_plugin_ipf;
extern const uft_format_plugin_t uft_format_plugin_jv1;
extern const uft_format_plugin_t uft_format_plugin_jv3;
extern const uft_format_plugin_t uft_format_plugin_jvc;
extern const uft_format_plugin_t uft_format_plugin_kfx;
extern const uft_format_plugin_t uft_format_plugin_logical;
extern const uft_format_plugin_t uft_format_plugin_mfi;
extern const uft_format_plugin_t uft_format_plugin_micropolis;
extern const uft_format_plugin_t uft_format_plugin_msa;
extern const uft_format_plugin_t uft_format_plugin_msx_disk;
extern const uft_format_plugin_t uft_format_plugin_myz80;
extern const uft_format_plugin_t uft_format_plugin_nanowasp;
extern const uft_format_plugin_t uft_format_plugin_nib;
extern const uft_format_plugin_t uft_format_plugin_northstar;
extern const uft_format_plugin_t uft_format_plugin_pdp;
extern const uft_format_plugin_t uft_format_plugin_po;
extern const uft_format_plugin_t uft_format_plugin_posix;
extern const uft_format_plugin_t uft_format_plugin_pri;
extern const uft_format_plugin_t uft_format_plugin_qrst;
extern const uft_format_plugin_t uft_format_plugin_sad;
extern const uft_format_plugin_t uft_format_plugin_sam;
extern const uft_format_plugin_t uft_format_plugin_sap_thomson;
extern const uft_format_plugin_t uft_format_plugin_scp;
extern const uft_format_plugin_t uft_format_plugin_ssd;
extern const uft_format_plugin_t uft_format_plugin_st;
extern const uft_format_plugin_t uft_format_plugin_stx;
extern const uft_format_plugin_t uft_format_plugin_t1k;
extern const uft_format_plugin_t uft_format_plugin_tan;
extern const uft_format_plugin_t uft_format_plugin_td0;
extern const uft_format_plugin_t uft_format_plugin_trd;
extern const uft_format_plugin_t uft_format_plugin_v9t9;
extern const uft_format_plugin_t uft_format_plugin_vdk;
extern const uft_format_plugin_t uft_format_plugin_victor9k;
extern const uft_format_plugin_t uft_format_plugin_woz;
extern const uft_format_plugin_t uft_format_plugin_xdm86;
extern const uft_format_plugin_t uft_format_plugin_xfd;

typedef struct {
    const char *sym;                      /* Plugin-Symbol im Manifest   */
    const uft_format_plugin_t *plugin;    /* das Plugin selbst           */
    const char *datei;                    /* unter tests/corpus_free/    */
} korpus_fall_t;

static const korpus_fall_t FAELLE[] = {
    { "2img",          &uft_format_plugin_2img,        "floptool_2img_1600.2mg" },
    { "86f",           &uft_format_plugin_86f,         "fluxfox_sector_test_360k.86f" },
    { "adf",           &uft_format_plugin_adf,         "xdftool_dd_ofs.adf" },
    { "adf_arc",       &uft_format_plugin_adf_arc,     "dim_adfs_f.adf" },
    { "adl",           &uft_format_plugin_adl,         "dim_adfs_l.adl" },
    { "apridisk",      &uft_format_plugin_apridisk,    "libdsk_uftk_pc720.apridisk" },
    { "atr",           &uft_format_plugin_atr,         "atrcopy_dos2sd.atr" },
    /* NICHT `cas_msx_grossblock.cas`, obwohl das die groesste `.cas` im
     * Korpus ist: sie traegt einen Block von 70 000 Byte und ist die
     * NEGATIVE Probe aus MF-1040 — `uft_disk_open()` gibt dafuer
     * richtigerweise NULL. Meine erste Auswahl „je Format die groesste
     * Datei" hat das nicht gesehen und den Test rot gemacht, wo der
     * Pruefling recht hatte. Eine abgeleitete Auswahl ist besser als
     * eine gepflegte (MF-636) — aber „gross" ist nicht dasselbe wie
     * „gueltig". */
    { "cas",           &uft_format_plugin_cas,         "cas_msx_drei.cas" },
    { "cfi",           &uft_format_plugin_cfi,         "libdsk_uftk_pc720.cfi" },
    { "cpm",           &uft_format_plugin_cpm,         "cpmtools_cf2dd_720k.cpm" },
    { "cqm",           &uft_format_plugin_cqm,         "libdsk_uftk_pc720.cqm" },
    { "d13",           &uft_format_plugin_d13,         "floptool_d13_455.d13" },
    { "d64",           &uft_format_plugin_d64,         "vice_c1541_35trk.d64" },
    { "d67",           &uft_format_plugin_d67,         "vice_c1541_2040.d67" },
    { "d71",           &uft_format_plugin_d71,         "vice_c1541_70trk.d71" },
    { "d77",           &uft_format_plugin_d77,         "hxcfe_uftk_nec_2d.d77" },
    { "d80",           &uft_format_plugin_d80,         "vice_c1541_8050.d80" },
    { "d81",           &uft_format_plugin_d81,         "vice_c1541_80trk.d81" },
    { "d82",           &uft_format_plugin_d82,         "vice_c1541_8250.d82" },
    { "d88",           &uft_format_plugin_d88,         "hxcfe_pc160.d88" },
    { "dc42",          &uft_format_plugin_dc42,        "libdsk_uftk_pc720.dc42" },
    { "dcm",           &uft_format_plugin_dcm,         "atrip_uftk_sd_alle6.dcm" },
    { "dim",           &uft_format_plugin_dim,         "dim_x68k_m01_1440k.dim" },
    { "dim_atari",     &uft_format_plugin_dim_atari,   "hxcfe_atarist_dd.dim" },
    { "dmk",           &uft_format_plugin_dmk,         "hxcfe_pc160.dmk" },
    { "dms",           &uft_format_plugin_dms,         "adf2dms_uftk_rle_880k.dms" },
    { "do",            &uft_format_plugin_do,          "uftk_dos33_35trk.do" },
    { "dsk_cpc",       &uft_format_plugin_dsk_cpc,     "hxcfe_pc160.dsk" },
    { "edk",           &uft_format_plugin_edk,         "epstool_eps_dd_800k.edk" },
    { "edsk",          &uft_format_plugin_edsk,        "samdisk_edsk.dsk" },
    { "fds",           &uft_format_plugin_fds,         "fds_spec_4seiten.fds" },
    { "g64",           &uft_format_plugin_g64,         "vice_c1541_35trk.g64" },
    { "g71",           &uft_format_plugin_g71,         "vice_c1541_1571.g71" },
    { "hfe",           &uft_format_plugin_hfe,         "gw_amigados.hfe" },
    { "imd",           &uft_format_plugin_imd,         "hxcfe_pc160.imd" },
    { "img",           &uft_format_plugin_img,         "gw_img.img" },
    { "ipf",           &uft_format_plugin_ipf,         "hxcfe_ibmdd.ipf" },
    { "jv1",           &uft_format_plugin_jv1,         "floptool_jv1_80spuren.jv1" },
    { "jv3",           &uft_format_plugin_jv3,         "hxcfe_pc160.jv3" },
    { "jvc",           &uft_format_plugin_jvc,         "gw_jvc.img" },
    { "kfx",           &uft_format_plugin_kfx,         "hxcfe_kfx_t40.0.raw" },
    { "logical",       &uft_format_plugin_logical,     "libdsk_ibm720_outback.logical" },
    { "mfi",           &uft_format_plugin_mfi,         "hxcfe_pc160.mfi" },
    { "micropolis",    &uft_format_plugin_micropolis,  "gw_micropolis.img" },
    { "msa",           &uft_format_plugin_msa,         "hxcfe_msa.msa" },
    { "msx_disk",      &uft_format_plugin_msx_disk,    "gw_msx_2dd.img" },
    { "myz80",         &uft_format_plugin_myz80,       "libdsk_myz80_voll.myz80" },
    { "nanowasp",      &uft_format_plugin_nanowasp,    "nwasp_spec_400k.nanowasp" },
    { "nib",           &uft_format_plugin_nib,         "a2nibblize_uftk_35trk.nib" },
    { "northstar",     &uft_format_plugin_northstar,   "gw_northstar.img" },
    { "pdp",           &uft_format_plugin_pdp,         "gw_pdp.img" },
    { "po",            &uft_format_plugin_po,          "gw_po.img" },
    { "posix",         &uft_format_plugin_posix,       "libdsk_ibm720.rawob" },
    { "pri",           &uft_format_plugin_pri,         "fluxfox_sector_test_360k.pri" },
    { "qrst",          &uft_format_plugin_qrst,        "qrst_spec_160k.qrst" },
    { "sad",           &uft_format_plugin_sad,         "samdisk_sad.sad" },
    { "sam",           &uft_format_plugin_sam,         "gw_sam.img" },
    { "sap_thomson",   &uft_format_plugin_sap_thomson, "sap2_thomson.sap" },
    { "scp",           &uft_format_plugin_scp,         "gw_fm_acorn_3trk.scp" },
    { "ssd",           &uft_format_plugin_ssd,         "gw_ssd.img" },
    { "st",            &uft_format_plugin_st,          "hxcfe_720k.st" },
    { "stx",           &uft_format_plugin_stx,         "hxcfe_pc160.stx" },
    { "t1k",           &uft_format_plugin_t1k,         "gw_t1k.img" },
    { "tan",           &uft_format_plugin_tan,         "floptool_jv1_80spuren.jv1" },
    { "td0",           &uft_format_plugin_td0,         "libdsk_uftk_pc720.td0" },
    { "trd",           &uft_format_plugin_trd,         "gw_trd.img" },
    { "v9t9",          &uft_format_plugin_v9t9,        "hxcfe_ti.v9t9" },
    { "vdk",           &uft_format_plugin_vdk,         "hxcfe_uftk_dragon_ds.vdk" },
    { "victor9k",      &uft_format_plugin_victor9k,    "floptool_victor9k_dsdd.img" },
    { "woz",           &uft_format_plugin_woz,         "to_woz2_uftk_dos33.woz" },
    { "xdm86",         &uft_format_plugin_xdm86,       "hxcfe_uftk_ti99_dssd.v9t9" },
    { "xfd",           &uft_format_plugin_xfd,         "atrcopy_dos2sd.xfd" },
};
#define FAELLE_N ((int)(sizeof FAELLE / sizeof FAELLE[0]))

/* ── Die 21 gemessenen Rennen ────────────────────────────────────────
 *
 * Jede Zeile hier ist ein Abbild, das ueber `uft_disk_open()` NICHT an
 * sein eigenes Plugin geht. Sie steht mit dem gemessenen Sieger und
 * seiner Konfidenz, damit eine Aenderung an irgendeiner Sonde hier
 * auffaellt statt still zu bleiben — das ist der Zweck dieser Tafel:
 * **sie ist eine Schranke, kein Zugestaendnis.**
 *
 * Gemessen 2026-09-15 am ganzen Korpus. Die Spalte `folge` sagt, was
 * der fremde Griff fuer die DATEN bedeutet — und das ist der
 * Unterschied zwischen einer Schoenheitsfrage und einem Defekt:
 *
 *   gleich   die Geometrie ist dieselbe, der Inhalt kommt richtig
 *   ANDERS   die Teilung oder Reihenfolge weicht ab — ein Leser
 *            liefert andere Bytes, als das Format sagt
 */
typedef struct {
    const char *sym;        /* Format, dessen Abbild verdraengt wird  */
    const char *sieger;     /* `name` des Plugins, das gemessen gewinnt */
    const char *folge;      /* "gleich" oder "ANDERS"                  */
    const char *grund;
} rennen_t;

static const rennen_t RENNEN[] = {
    /* BERICHTIGT MF-1152, und wieder hat die Tafel es selbst gemeldet:
     * hier stand `SSD` mit 85, also im Band „Merkmal getroffen". Das
     * war der Befund P3-405 — das DFS-Sektorzahlfeld ist zehn Bit
     * breit, die Sonde las acht, und `0x31 0x20` sah damit aus wie die
     * gueltigen 800 Sektoren statt wie die unmoeglichen 288. Behoben;
     * uebrig ist das Rennen bei 204 800 Byte, und dort melden FUENF
     * Plugins gleichauf 40, waehrend `jv1` selbst nur 35 sagt. */
    { "jv1", "IMG", "gleich",
      "204 800 Byte; IMG 40 und **gleichauf 5**, `jv1` selbst 35. Ein "
      "kopfloser TRS-80-Abzug kann nichts als seine Groesse vorzeigen, "
      "und die teilt er mit IMG (50x1x8x512 gegen 80x1x10x256 — "
      "dieselbe Summe). Die Geometrie des Siegers ist hier zufaellig "
      "folgenlos; die Frage, was bei `gleichauf > 1` geschehen soll, "
      "ist P3-402" },
    { "tan", "IMG", "gleich",
      "dieselbe Datei wie `jv1` (artefaktgleich, P3-365), also "
      "dieselbe Messung" },
    /* BERICHTIGT MF-1182, und die Tafel hat es selbst gemeldet: hier
     * stand `MYZ80` mit dem Grund „meldet 70 gegen cpms 40". Das war
     * P3-406 — MYZ80s ganze Erkennung ist „die ersten 256 Byte sind
     * alle 0xE5", und eine CP/M-Diskette mit unbeschriebener
     * Systemspur ist genau das; gelesen wuerde 64x1x128x1024 statt
     * 80x2x9x512. Seit MF-1182 bilden BEIDE Sonden ihre Zahl ueber
     * `uft_probe_konfidenz()`: MYZ80 faellt auf **25** (Struktur +
     * Geometrie, ohne Kennung geklemmt), `cpm` steigt auf **45**, weil
     * `cpm_waehle()` die Gesamtgroesse EXAKT prueft und der Anspruch
     * damit jedes Byte erklaert. Die Zeile fiel daraufhin mit
     * „gemessen war MYZ80, jetzt MSX" — genau dafuer ist sie eine
     * Schranke und kein Zugestaendnis.
     *
     * **Und `cpm` gewinnt dadurch NICHT.** Es faellt von 40 auf 25,
     * weil auch seine 40 von Hand vergeben waren: kopflos, also keine
     * Kennung und keine Selbstkonsistenz (die Doktrin bindet den Beleg
     * an eine Groessenangabe IN der Datei, und eine Tafelgroesse ist
     * „Groesse allein" = 0). Uebrig bleiben Struktur und Geometrie.
     * Sieger ist jetzt **MSX mit 45 bei gleichauf 1** — MSX-2DD ist
     * ebenfalls 80x2x9x512.
     *
     * Was das Rennen um diese Datei angeht, ist der Befund damit
     * *verschoben*, nicht geschlossen: MYZ80s Ueberanspruch ist weg,
     * und zwischen MYZ80 und `cpm` steht jetzt ein Gleichstand bei 25,
     * den Regel 2 der Doktrin entscheiden wuerde — der engere Anspruch
     * gewinnt, `cpm` erklaert jedes Byte, MYZ80 256 davon. Fuer diese
     * Regel fehlt das Mass im Sondenvertrag: **P3-439**. */
    { "cpm", "MSX", "ANDERS",
      "737 280 Byte. Seit MF-1182 kommen beide fraglichen Zahlen aus der "
      "Leiter: MYZ80 von 70 auf 25 (der Befund aus P3-406) und `cpm` von "
      "40 auf 25 — beide kopflos, also Struktur plus Geometrie und "
      "sonst nichts. Sieger ist MSX mit 45, gleichauf 1; MSX-2DD hat "
      "dieselbe Teilung wie ein PC-720K und wie `pcw-720`. Der "
      "Gleichstand zwischen MYZ80 und `cpm` bei 25 waere Regel 2 der "
      "Doktrin, und ihr fehlt das Mass — P3-439" },
    /* BERICHTIGT MF-1151, und die Tafel hat es selbst gemeldet: hier
     * stand `DMK` mit dem Grund „meldet 65 auf einem flachen Abbild".
     * Das war der Befund, MF-1151 hat ihn behoben (drei Tore aus MAMEs
     * `dmk_dsk.cpp`), und die Zeile fiel daraufhin mit „gemessen war
     * DMK, jetzt MSX" — genau dafuer ist sie eine Schranke und kein
     * Zugestaendnis. **Was bleibt, ist nicht der Defekt, sondern das
     * Rennen:** 737 280 Byte, und MSX-2DD hat dieselbe Teilung wie ein
     * PC-720K — nur nicht dieselbe wie eine Atari-ST-Diskette. */
    { "st", "MSX", "ANDERS",
      "737 280 Byte; MSX 45, `st` selbst darunter. Seit MF-1151 ist DMK "
      "aus diesem Rennen (es meldete 65, weil die ersten Byte `00 01 "
      "55 46 54 2D 4B` — UFTs eigene Marke \"UFT-K\" — als DMK-Kopf mit "
      "tracks=1 lesbar waren und eine UNTERE Groessenschranke dagegen "
      "nichts prueft). Uebrig ist dieselbe Lage wie bei `img`, "
      "`logical` und `posix`: vier Formate teilen sich 737 280 Byte, "
      "und keines kann mehr als die Groesse vorzeigen — P3-401/P3-402" },
    { "edsk", "DSK", "ANDERS",
      "beide melden 95 und liegen GLEICHAUF (tied 2) — den Zuschlag "
      "gibt die Reihenfolge. Eine erweiterte CPC-Datei wuerde mit dem "
      "einfachen DSK-Modell gelesen. Genau der Fall, fuer den P3-402 "
      "die Entscheidung sucht" },
    { "d77", "D88", "gleich",
      "D88 meldet 95, D77 85. D77 IST ein D88-Behaelter mit anderer "
      "Endung; hxcfe hat die Datei ueber sein NEC_D88-Modul "
      "geschrieben. Folgenlos fuer den Inhalt, aber die Stufe von "
      "`d77` ruht auf einem Abbild, das der oeffentliche Pfad als D88 "
      "oeffnet" },
    { "adf_arc", "AkaiS900", "gleich",
      "1 638 400 Byte, und Akai-HD hat dieselbe Geometrie wie Acorn "
      "ADFS F (80x2x10x1024) — gemessen und benannt seit MF-976/P3-278. "
      "Akai 45, ADF_ARC 35" },
    { "adl", "TRD", "gleich",
      "655 360 Byte; TRD 45, gleichauf 2. TR-DOS ist 80x2x16x256, "
      "Acorn ADFS L dasselbe Produkt" },
    { "po", "DO", "ANDERS",
      "143 360 Byte; DO 45, gleichauf 2. DOS-3.3- und ProDOS-Reihenfolge "
      "unterscheiden sich — `src/formats/do/uft_do.c` beschreibt genau "
      "das und nennt zwei aehnliche Konfidenzen fuer dieselben Bytes "
      "ausdruecklich die ehrliche Antwort (P3-401)" },
    { "edk", "D81", "ANDERS",
      "819 200 Byte; D81 45, gleichauf 2. P3-401: D81s 45 ist eine "
      "KONSTANTE, sein Beleg liegt bei Versatz 399 363 und damit hinter "
      "dem 65 536-Byte-Sondenpuffer" },
    { "sam", "D81", "ANDERS", "wie `edk`, dieselbe Groesse, P3-401" },
    { "img", "MSX", "gleich",
      "737 280 Byte; MSX 45 gegen IMG 40. MSX-2DD ist 80x2x9x512, also "
      "dieselbe Teilung" },
    { "logical", "MSX", "ANDERS",
      "737 280 Byte. `logical` sagt bei Mehrdeutigkeit seit MF-1032 "
      "selbst ab (Konfidenz 0) — es KANN das Rennen nicht gewinnen, und "
      "die OUTBACK-Anordnung dieser Datei sieht MSX nicht. Der fehlende "
      "Kanal fuer eine benannte Geometrie ist P3-337" },
    { "posix", "MSX", "ANDERS", "wie `logical`, dieselbe Lage, P3-337" },
    { "v9t9", "MSX", "ANDERS",
      "184 320 Byte; MSX 45. MSX-1DD ist 40x2x9x512, TI-99 DSSD "
      "40x2x9x256 mit RUECKWAERTS laufender Seite 1 (MF-1027) — die "
      "Teilung weicht ab" },
    { "xdm86", "MSX", "ANDERS", "wie `v9t9`, dieselbe Datei-Groesse" },
    /* `ssd` STAND HIER und ist heraus — die Tafel hat es gemeldet
     * („gemessen war JV1, jetzt das eigene Plugin"), und genau dafuer
     * ist sie da: eine Ausnahme, die der Lauf nicht mehr braucht, faellt
     * auf statt stillschweigend weiter zu gelten.
     *
     * Der alte Grund war: „102 400 Byte; JV1 35, SSD nur 30 — und SSD
     * hat recht, denn ein roher gw-Abzug hat kein DFS-Verzeichnis."
     * Der erste Halbsatz stimmte, der zweite nicht ganz. Seit MF-1152
     * liest die Sonde das Sektorzahlfeld als die zehn Bit, die es ist:
     * `gw_ssd.img` sagt dort **480** Sektoren, ein Vielfaches von zehn
     * und gross genug fuer 102 400 Byte.
     *
     * **NACHGEZOGEN MF-1153, und die Zahlen sind kleiner geworden:**
     * seit der Sonden-Doktrin leitet `ssd` seine Konfidenz ab, und ohne
     * Kennung ist die Obergrenze 45. Hier traegt die Datei
     * SELBSTKONSISTENZ (480 Sektoren passen) und GEOMETRIE, aber keine
     * STRUKTUR (die Bootoption faellt aus, 0xC1 >> 4 = 12) — das sind
     * **35**. JV1 meldet ebenfalls 35, also **gleichauf 2**, und `ssd`
     * gewinnt sein Abbild jetzt durch die REIHENFOLGE statt durch
     * Belege. Das ist ehrlicher als vorher und noch nicht gut: Regel 3
     * der Doktrin („bleibt es gleich, gewinnt keiner") ist bewusst noch
     * nicht scharf gestellt, weil sie 26 von 72 Abbildern betrifft und
     * ihre eigene Messung braucht. Die Zeile bleibt deshalb aus der
     * Rennen-Tafel heraus — `ssd` erreicht sein Plugin —, und der
     * Gleichstand steht hier benannt. */
    { "jvc", "IMG", "gleich",
      "161 280 Byte; IMG 40, gleichauf 2. MF-1144-Klasse" },
    { "t1k", "IMG", "gleich",
      "1 474 560 Byte; IMG 40, gleichauf 3. MF-1144-Klasse" },
    { "nanowasp", "IMG", "ANDERS",
      "409 600 Byte; IMG 40, und **gleichauf 3** — der Fall, an dem "
      "MF-1148 den Gleichstand gemessen hat. IMG teilt 50x2x8x512, "
      "NanoWasp 40x2x10x512 mit Skew. Hier standen bis MF-1182 vier "
      "Gleichauf-Liegende; `cpm` ist ausgeschieden, weil seine 40 von "
      "Hand vergeben waren und die Leiter 25 ergibt. P3-402" },
    /* BERICHTIGT MF-1252: der Sieger hat sich GEAENDERT, und das
     * Rennen ist damit nicht gewonnen, sondern neu besetzt. Vorher
     * stand hier `DSK_X820`. Seit die Endung bei Gleichstand verengt,
     * gewinnt unter den drei Gleichauf-Liegenden derjenige, der `.img`
     * beansprucht — und das ist `HardSector` (die Korpusdatei heisst
     * `gw_pdp.img`).
     *
     * „Folgenlos" ist dabei NICHT uebernommen, sondern am neuen Sieger
     * nachgemessen: `src/formats/hardsector/uft_hardsector.c:543`
     * fuehrt `77 x 1 x 26 x 128 = 256 256` — dieselbe Geometrie wie
     * die alte Ausnahme. Waere sie eine andere, stuende hier ein
     * Befund und keine Fussnote. */
    { "pdp", "HardSector", "gleich",
      "256 256 Byte; drei gleichauf bei 40, und `.img` verengt auf "
      "HardSector. Die Geometrie ist identisch (77x1x26x128, gemessen "
      "an uft_hardsector.c:543), also folgenlos — vorher DSK_X820, "
      "gemessen MF-1147/MF-1252" },
};
#define RENNEN_N ((int)(sizeof RENNEN / sizeof RENNEN[0]))

static const rennen_t *rennen_fuer(const char *sym)
{
    int i;
    for (i = 0; i < RENNEN_N; i++)
        if (strcmp(RENNEN[i].sym, sym) == 0) return &RENNEN[i];
    return NULL;
}

static int gruen = 0, rot = 0, uebersprungen = 0;

static void zusage(const char *was, int ok)
{
    if (ok) { printf("  [OK]   %s\n", was); gruen++; }
    else    { printf("  [ROT]  %s\n", was); rot++; }
}

static long dateigroesse(const char *pfad)
{
    FILE *f = fopen(pfad, "rb");
    long n;
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    n = ftell(f);
    fclose(f);
    return n;
}

int main(void)
{
    int erreicht = 0, eigenes = 0, fremdes = 0;
    /* MF-1252: wie viele Abbilder NUR mit genanntem Format
     * hineinkommen — der eigentliche Befund dieses Laufs. */
    int nur_mit_zwang = 0;
    /* Wie viele benannte Rennen die ENDUNG gewonnen hat — also
     * Abbilder, die vorher an ein fremdes Plugin gingen. */
    int eigenes_durch_endung = 0;
    int i;

    printf("\nA4 Runde 2: der Korpus durch uft_disk_open() (MF-1150)\n\n");

    /* MF-447: ohne dies ist die Registry LEER und `uft_disk_open()`
     * gibt fuer jede Datei NULL — der ganze Test waere aus dem falschen
     * Grund rot. */
    zusage("uft_register_all_formats() meldet Erfolg",
           uft_register_all_formats() == UFT_OK);
    if (rot) { printf("\n%d gruen, %d rot\n", gruen, rot); return 1; }

    for (i = 0; i < FAELLE_N; i++) {
        const korpus_fall_t *f = &FAELLE[i];
        char pfad[600], txt[400];
        long groesse;
        uft_disk_t *d;

        snprintf(pfad, sizeof pfad, "%s/%s", UFT_CORPUS_DIR, f->datei);
        groesse = dateigroesse(pfad);
        if (groesse < 0) {
            printf("  [SKIP] %s: %s fehlt im Korpus\n", f->sym, f->datei);
            uebersprungen++;
            continue;
        }

        /* MF-1251/MF-1252: erreichbar heisst jetzt „ohne Zwang, ODER
         * mit genanntem Format". Vorher war die Zusage gruen, weil
         * IRGENDWER das Rennen gewann — gemessen an 18 von 129
         * Abbildern, darunter eine von VICE erzeugte `.d81`.
         *
         * Welcher der beiden Faelle eintritt, wird GEZAEHLT. Die Zahl
         * ist der Befund: so viele Abbilder sind ueber Groesse UND
         * Endung nicht bestimmbar, und was ihren Gleichstand bricht,
         * ist Inhalt. */
        uft_probe_ranking_t vorab;
        d = uft_disk_open_ranked(pfad, true, &vorab);
        int mit_zwang = 0;
        if (!d && vorab.tied > 1) {
            d = uft_disk_open_as(pfad, true, f->plugin);
            mit_zwang = (d != NULL);
        }
        snprintf(txt, sizeof txt,
                 "%-13s erreichbar%s (%s, %ld Byte%s)",
                 f->sym, mit_zwang ? " NUR mit genanntem Format" : "",
                 f->datei, groesse,
                 (vorab.tied > 1) ? ", mehrdeutig" : "");
        zusage(txt, d != NULL);
        if (!d) continue;
        if (mit_zwang) nur_mit_zwang++;
        erreicht++;

        {
            const char *sieger = (d->plugin && d->plugin->name)
                               ? d->plugin->name : "?";
            const int selbst = (d->plugin == f->plugin);
            const rennen_t *rn = rennen_fuer(f->sym);
            uft_probe_ranking_t r;
            memset(&r, 0, sizeof r);
            uft_probe_file_ranked(pfad, &r);
            if (selbst) eigenes++; else fremdes++;

            if (!rn) {
                snprintf(txt, sizeof txt,
                         "%-13s der Griff gehoert dem eigenen Plugin "
                         "(Sieger %s, Konfidenz %d, gleichauf %zu, "
                         "Naechster %s %d, Anspruch %zu)",
                         f->sym, sieger, r.confidence, r.tied,
                         (r.runner_up && r.runner_up->name)
                             ? r.runner_up->name : "-",
                         r.runner_up_confidence, r.claimants);
                zusage(txt, selbst);
            } else {
                /* Ein benanntes Rennen: geprueft wird, dass es NOCH
                 * GENAU SO ausgeht. Wird es gewonnen, faellt die Zeile
                 * ebenso — dann ist die Ausnahme ueberholt und gehoert
                 * aus der Tafel, nicht stillschweigend weiter geduldet. */
                /* MF-1252: ein benanntes Rennen beschreibt, was die
                 * SONDE entscheidet — nicht, was nach einem Zwang im
                 * Handle steht. Wurde mit genanntem Format geoeffnet,
                 * gewinnt zwangslaeufig das eigene Plugin, und der
                 * alte Vergleich waere eine Aussage ueber meinen
                 * eigenen Aufruf.
                 *
                 * Gemessen sind ALLE gelisteten Rennen Gleichstaende
                 * (`edsk` sogar bei Konfidenz 95 mit zwei Bewerbern).
                 * Damit sind sie ueberholt: kein fremdes Plugin
                 * gewinnt mehr, weil keines mehr gewinnt. Geprueft
                 * wird deshalb, dass die Sonde fuer genau diese Faelle
                 * wirklich ABSAGT — das ist eine Aussage ueber die
                 * Doktrin und keine Tautologie. */
                if (mit_zwang) {
                    const int sagt_ab =
                        (uft_probe_file_entschieden(pfad, NULL) == NULL);
                    snprintf(txt, sizeof txt,
                             "%-13s benanntes Rennen UEBERHOLT: die Sonde "
                             "sagt ab, statt es an %s zu geben "
                             "(gleichauf %zu bei %d%%)",
                             f->sym, rn->sieger, r.tied, r.confidence);
                    zusage(txt, sagt_ab);
                    printf("         -> Grund der alten Ausnahme: %s\n",
                           rn->grund);
                    uft_disk_close(d);
                    d = NULL;
                    continue;
                }
                /* MF-1252, zweiter Fall: die ENDUNG hat verengt, und
                 * zwar auf das eigene Plugin. Dann ist das Rennen
                 * nicht nur ueberholt, es ist GEWONNEN — genau der
                 * Ausgang, den der Kommentar oben fordert. Geprueft
                 * wird, dass es ohne Zwang und mit dem eigenen Plugin
                 * aufgeht UND dass die Messung den Gleichstand
                 * weiterhin ausweist (sonst waere die Verengung als
                 * Evidenz getarnt). */
                if (selbst) {
                    snprintf(txt, sizeof txt,
                             "%-13s benanntes Rennen GEWONNEN: die Endung "
                             "verengt auf das eigene Plugin, statt es an "
                             "%s zu geben (gleichauf %zu bei %d%%)",
                             f->sym, rn->sieger, r.tied, r.confidence);
                    zusage(txt, r.tied > 1);
                    printf("         -> Grund der alten Ausnahme: %s\n",
                           rn->grund);
                    eigenes_durch_endung++;
                    uft_disk_close(d);
                    d = NULL;
                    continue;
                }
                const int wie_gemessen =
                    (!selbst && strcmp(sieger, rn->sieger) == 0);
                snprintf(txt, sizeof txt,
                         "%-13s benanntes Rennen, geht noch an %-9s "
                         "[%s] (Konfidenz %d, gleichauf %zu, Naechster "
                         "%s %d, Anspruch %zu)",
                         f->sym, rn->sieger, rn->folge, r.confidence,
                         r.tied, (r.runner_up && r.runner_up->name)
                                     ? r.runner_up->name : "-",
                         r.runner_up_confidence, r.claimants);
                zusage(txt, wie_gemessen);
                if (!wie_gemessen)
                    printf("         -> gemessen war %s, jetzt %s. Grund "
                           "der Ausnahme: %s\n",
                           rn->sieger, selbst ? "das eigene Plugin"
                                              : sieger, rn->grund);
            }
        }
        uft_disk_close(d);
    }

    printf("\n  %d von %d Abbildern erreichen ein Plugin, davon %d das "
           "EIGENE und %d ein fremdes; %d uebersprungen\n",
           erreicht, FAELLE_N, eigenes, fremdes, uebersprungen);

    /* Die Bilanz als eigene Zusage, in BEIDE Richtungen. Sie steht hier,
     * weil eine Tafel mit benannten Ausnahmen sonst wachsen koennte,
     * ohne dass es jemand merkt: eine NEUE Verdraengung faellt oben an
     * ihrer Zeile auf, aber eine neue ZEILE in FAELLE mit einer neuen
     * Ausnahme faellt nur hier auf.
     *
     * Gemessen 2026-09-15 zuerst 72 / 51 / 21. Seither zwei Korrekturen,
     * und die Zahl ist ihre FOLGE (MF-1077):
     *
     *   MF-1151  `st` ging an DMK (65 auf einem flachen Abbild) und geht
     *            jetzt an MSX — ein Rennen weniger falsch begruendet,
     *            aber dieselbe Bilanz.
     *   MF-1152  `ssd` bekommt sein eigenes Abbild (60 gegen JV1s 35),
     *            weil das Sektorzahlfeld jetzt mit zehn Bit gelesen
     *            wird: **52 eigenes, 20 fremdes**.
     *   MF-1252  Die Sonde gehorcht ihrer Doktrin: bei Gleichstand
     *            gewinnt niemand, und die ENDUNG verengt als „engerer
     *            Anspruch". Gemessen gehen damit **acht** Abbilder
     *            nicht mehr an ein fremdes Plugin — aus 52/20 wird
     *            **60 eigenes, 12 fremdes**. Die Zahl ist die FOLGE
     *            der Messung und nicht ihr Ziel (MF-1077): sie bewegt
     *            sich, weil eine Regel jetzt gilt, die vorher nur
     *            aufgeschrieben war. */
    {
        char txt[240];
        snprintf(txt, sizeof txt,
                 "die Bilanz ist unveraendert: %d erreicht, %d eigenes, "
                 "%d fremdes (gemessen 72 / 60 / 12), davon %d Rennen "
                 "durch die Endung gewonnen und %d nur mit Zwang "
                 "erreichbar",
                 erreicht, eigenes, fremdes,
                 eigenes_durch_endung, nur_mit_zwang);
        zusage(txt, erreicht == 72 && eigenes == 60 && fremdes == 12);
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
