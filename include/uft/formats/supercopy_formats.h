/**
 * =============================================================================
 * SuperCopy v3.40 SELECT.DAT - CP/M Format-Datenbank
 * =============================================================================
 *
 * 313 CP/M-Diskettenformate aus dem SuperCopy-Kopierprogramm (1991).
 * Enthält physikalische Geometrie-Parameter für die Format-Erkennung.
 *
 * Quelle: SuperCopy v3.40 von Oliver Müller
 * Integration: UFI Preservation Platform / MFM-Detect Modul
 * =============================================================================
 */

#ifndef SUPERCOPY_FORMATS_H
#define SUPERCOPY_FORMATS_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Dichte-Flags */
#define SC_DENS_SD   0x01
#define SC_DENS_DD   0x02
#define SC_DENS_HD   0x04

typedef struct {
    const char *name;           /**< Kurzname (z.B. "KAYPROII") */
    const char *description;    /**< Vollständige Bezeichnung */
    uint16_t sector_size;       /**< Bytes pro Sektor */
    uint8_t  sectors_per_track; /**< Sektoren pro Spur */
    uint8_t  heads;             /**< 1=SS, 2=DS */
    uint16_t cylinders;         /**< 40 (48tpi) oder 80 (96tpi) */
    uint8_t  density;           /**< SC_DENS_SD/DD/HD */
    uint32_t total_bytes;       /**< Gesamtkapazität in Bytes */
} supercopy_format_t;

#define SUPERCOPY_FORMAT_COUNT 313

static const supercopy_format_t supercopy_formats[] = {
    {"ABC-24", "ABC - 24", 512, 9, 2, 40, SC_DENS_DD, 368640},
    {"ACORN", "Acorn B CP/M", 256, 10, 2, 80, SC_DENS_SD, 409600},
    {"ADPS", "ADPS", 512, 8, 1, 40, SC_DENS_DD, 163840},
    {"ADTEC", "ADTEC IDS-7000", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"ADV", "ADV System 1", 512, 10, 2, 80, SC_DENS_DD, 819200},
    {"ALPHAI-6", "Alpha-I Format 6", 256, 18, 2, 40, SC_DENS_DD, 368640},
    {"ALPHAI-8", "Alpha-I Format 8", 512, 10, 2, 80, SC_DENS_DD, 819200},
    {"ALTOS", "Altos", 512, 9, 2, 80, SC_DENS_DD, 737280},
    {"ATARIDS", "Atari 260 ST/520 ST Format 1", 512, 9, 2, 80, SC_DENS_DD, 737280},
    {"ATARIDS2", "Atari 260 ST/520 ST Format 2", 512, 9, 2, 80, SC_DENS_DD, 737280},
    {"ATARISS", "Atari 260ST/520ST Format 3", 512, 9, 1, 80, SC_DENS_DD, 368640},
    {"AVL", "AVL Eagle II", 1024, 5, 1, 80, SC_DENS_DD, 409600},
    {"BASF1", "BASF 7120 Format 1", 1024, 5, 1, 40, SC_DENS_DD, 204800},
    {"BASF2", "BASF 7120 Format 2", 1024, 5, 2, 40, SC_DENS_DD, 409600},
    {"BASISDT", "BASIS 208 (2k-Blockung)", 512, 10, 2, 80, SC_DENS_DD, 819200},
    {"BASISDT2", "BASIS 208 (4k-Blockung)", 512, 10, 2, 80, SC_DENS_DD, 819200},
    {"BASISSD", "BASIS 208 SS/SD", 128, 18, 1, 40, SC_DENS_SD, 92160},
    {"BASISST", "BASIS 208 SS", 512, 9, 1, 40, SC_DENS_DD, 184320},
    {"BASISST2", "BASIS 208 DS", 512, 10, 2, 40, SC_DENS_DD, 409600},
    {"BITSCH", "Bitsch Computertechnik", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"BITSCH2", "Bitsch Computertechnik SS", 256, 16, 1, 80, SC_DENS_DD, 327680},
    {"BONDW-12", "Bondwell-12", 256, 18, 1, 40, SC_DENS_DD, 184320},
    {"BONDW-14", "Bondwell-14", 256, 18, 2, 40, SC_DENS_DD, 368640},
    {"BONDW-2", "Bondwell 2", 256, 18, 1, 80, SC_DENS_DD, 368640},
    {"CADCOM", "Cadcom", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"CANON", "Canon AS-100", 512, 8, 2, 80, SC_DENS_DD, 655360},
    {"CASIO", "Casio FP 1100", 256, 16, 2, 40, SC_DENS_DD, 327680},
    {"CETIS", "Cetis Biomec 1", 512, 10, 2, 80, SC_DENS_DD, 819200},
    {"CHRIST", "Christiani", 256, 16, 2, 40, SC_DENS_DD, 327680},
    {"COLIBRI", "Colibri System II", 512, 9, 2, 80, SC_DENS_DD, 737280},
    {"COMTES", "Comtes", 512, 10, 2, 80, SC_DENS_DD, 819200},
    {"COMULADY", "Comulady Type IV", 512, 15, 2, 80, SC_DENS_DD, 1228800},
    {"CONITEC", "Conitec", 512, 10, 2, 80, SC_DENS_DD, 819200},
    {"CONITEC2", "Conitec Format 2", 1024, 5, 1, 80, SC_DENS_DD, 409600},
    {"COSI", "Cosicomp-81", 512, 10, 2, 80, SC_DENS_DD, 819200},
    {"COSI2", "Cosicomp-81 48tpi", 512, 10, 2, 40, SC_DENS_DD, 409600},
    {"COSTEC", "Costec CP/M 3.0", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"CPC-1", "Schneider CPC 128/464", 512, 9, 1, 40, SC_DENS_DD, 184320},
    {"CPC-2", "Schneider CPC Vortex", 512, 9, 2, 80, SC_DENS_DD, 737280},
    {"CPC-3", "Schneider CPC Format 2", 512, 9, 1, 40, SC_DENS_DD, 184320},
    {"CROMEMC2", "Cromemco CDOS SS", 512, 10, 1, 40, SC_DENS_DD, 204800},
    {"CROMEMCO", "Cromemco CDOS DS", 512, 10, 2, 40, SC_DENS_DD, 409600},
    {"CT-HSP", "C't 68000 HSP", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"DEC", "DEC Rainbow", 512, 10, 1, 80, SC_DENS_DD, 409600},
    {"DIMA-S", "Dima (S) Apple", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"DISYS40", "Disys 48tpi", 1024, 5, 2, 40, SC_DENS_DD, 409600},
    {"DISYS80", "Disys 96tpi", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"DOLCH", "Dolch Colt 300", 256, 16, 2, 40, SC_DENS_DD, 327680},
    {"DOLCH-2", "Dolch Colt Format 2", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"DRS20", "ICL System DRS20", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"DRS300", "ICL System DRS300", 512, 9, 2, 80, SC_DENS_DD, 737280},
    {"DS2069", "DS 2069 DISCO", 256, 16, 1, 40, SC_DENS_DD, 163840},
    {"DTW-1", "DeTeWe IMS 5000 F1", 256, 16, 2, 40, SC_DENS_DD, 327680},
    {"DTW-2", "DeTeWe IMS 5000 F2", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"DTW-3", "DeTeWe IMS 5000 F3", 256, 16, 1, 40, SC_DENS_DD, 163840},
    {"ECMA70", "ECMA 70 MC CP/M", 256, 16, 1, 40, SC_DENS_DD, 163840},
    {"ECMA70-2", "ECMA 70 MC CP/M 96tpi", 256, 16, 1, 80, SC_DENS_DD, 327680},
    {"ELAB-1", "ELAB Format 1", 256, 18, 2, 40, SC_DENS_DD, 368640},
    {"ELAB-2", "ELAB Format 2 CP/M 3", 512, 10, 2, 40, SC_DENS_DD, 409600},
    {"ELSA", "ELSA 3.5\"", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"ELSA2", "ELSA Format 2", 1024, 5, 2, 40, SC_DENS_DD, 409600},
    {"ELSA3", "ELSA Format 3 SD", 128, 18, 2, 40, SC_DENS_SD, 184320},
    {"ELTRON1", "Eltronix Compu Profi F1", 512, 10, 1, 80, SC_DENS_DD, 409600},
    {"ELTRON2", "Eltronix Compu Profi F2", 512, 10, 1, 80, SC_DENS_DD, 409600},
    {"ELZET-80", "Elzet-80", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"ELZET-HD", "Elzet-HD", 1024, 8, 2, 80, SC_DENS_DD, 1310720},
    {"ELZET-SS", "Elzet-80 SS", 1024, 5, 1, 40, SC_DENS_DD, 204800},
    {"ETV250", "Olivetti ETV 250", 256, 16, 1, 80, SC_DENS_DD, 327680},
    {"ETV250-2", "Olivetti ETV 250 F2", 256, 18, 1, 80, SC_DENS_DD, 368640},
    {"ETV300", "Olivetti ETV 300", 256, 18, 1, 40, SC_DENS_DD, 184320},
    {"ETXII-1", "Olympia ETX-II, Philips P-2000", 256, 16, 1, 40, SC_DENS_DD, 163840},
    {"ETXII-2", "Olympia ETX-II DS", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"EUROCOM1", "Eurocom II Format 1", 256, 16, 1, 40, SC_DENS_DD, 163840},
    {"EUROCOM2", "Eurocom II Format 2", 256, 16, 2, 40, SC_DENS_DD, 327680},
    {"EUROCOM3", "Eurocom III CP/M-68k", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"EUROCOM4", "Eurocom TurboDOS", 1024, 8, 2, 80, SC_DENS_DD, 1310720},
    {"EX-100", "Olympia EX-100", 512, 9, 2, 40, SC_DENS_DD, 368640},
    {"EXIDY", "Exidy Sorcerer", 512, 10, 2, 80, SC_DENS_DD, 819200},
    {"EXIDY2", "Exidy Sorcerer CP/M 1.42/3", 256, 16, 1, 40, SC_DENS_DD, 163840},
    {"EXIDY3", "Exidy Sorcerer CP/M 2.2", 512, 10, 2, 40, SC_DENS_DD, 409600},
    {"FELTRON", "Feltron", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"FELTRON2", "Feltron 5080 Format 2", 256, 16, 1, 40, SC_DENS_DD, 163840},
    {"FM7", "Fujitsu Micro 7", 256, 16, 2, 40, SC_DENS_DD, 327680},
    {"FMC", "FMC Turbo-DOS", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"FORMULA", "Formula-80", 256, 18, 2, 40, SC_DENS_DD, 368640},
    {"GEMINI", "Gemini Microcomputers", 512, 10, 2, 40, SC_DENS_DD, 409600},
    {"GEORG", "System Fa. Georg 3.5\"", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"HECKLER", "Heckler & Koch MT4", 1024, 5, 2, 40, SC_DENS_DD, 409600},
    {"HKM-1", "H.K.M. ZDOS", 512, 10, 2, 40, SC_DENS_DD, 409600},
    {"HKM-2", "H.K.M. ZDOS 5.B", 512, 10, 2, 80, SC_DENS_DD, 819200},
    {"HONEYWEL", "Honeywell", 256, 16, 2, 40, SC_DENS_DD, 327680},
    {"HOOKSTAR", "Hookstar 24", 256, 16, 1, 80, SC_DENS_DD, 327680},
    {"HP125", "HP125/HP86/HP87", 256, 16, 2, 40, SC_DENS_DD, 327680},
    {"HP125-SS", "HP125/HP86/HP87 96tpi", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"IBC", "IBC dIMA (S) System", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"IBC-HD", "IBC dIMA(S) HD", 1024, 8, 2, 80, SC_DENS_DD, 1310720},
    {"IBM3740", "8\" Standard SS SD", 128, 26, 1, 80, SC_DENS_SD, 266240},
    {"IBMDS", "IBM PC DS", 512, 8, 2, 40, SC_DENS_DD, 327680},
    {"IBMSS", "IBM PC SS", 512, 8, 1, 40, SC_DENS_DD, 163840},
    {"ICL-1", "ICL RAIR CCP/M 3.2", 512, 10, 2, 80, SC_DENS_DD, 819200},
    {"ICL-2", "ICL Format 2 HD", 1024, 9, 2, 80, SC_DENS_DD, 1474560},
    {"IF800", "BMC IF 800", 512, 10, 2, 40, SC_DENS_DD, 409600},
    {"IMA-77", "IMA-77", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"ITTDT", "ITT 3030 96tpi", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"ITTST", "ITT 3030 48tpi", 256, 16, 2, 40, SC_DENS_DD, 327680},
    {"JAMES", "James 800k", 512, 10, 2, 80, SC_DENS_DD, 819200},
    {"JK82", "Janich & Klaas jk82 fast", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"JK82-2", "Janich & Klaas jk82 F2", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"JK82-3", "Janich & Klaas jk82 F3", 1024, 5, 2, 40, SC_DENS_DD, 409600},
    {"JOYCE", "Schneider Joyce", 512, 9, 1, 40, SC_DENS_DD, 184320},
    {"JOYCE-2", "Schneider Joyce F2", 512, 9, 2, 80, SC_DENS_DD, 737280},
    {"KAYPRO8", "Kaypro Pro 8", 512, 10, 2, 80, SC_DENS_DD, 819200},
    {"KAYPROII", "Kaypro II", 512, 10, 1, 40, SC_DENS_DD, 204800},
    {"KAYPROIV", "Kaypro IV/10", 512, 10, 2, 40, SC_DENS_DD, 409600},
    {"KD-TURBO", "Kneiser & Doering TurboDOS", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"KISS", "KISS 3248", 256, 16, 2, 40, SC_DENS_DD, 327680},
    {"KISS2K", "KISS 3464 2k Block", 256, 16, 2, 40, SC_DENS_DD, 327680},
    {"KOCH", "T. Koch CP/M 3.0", 1024, 9, 2, 80, SC_DENS_DD, 1474560},
    {"KONTRON1", "Kontron SMR", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"KONTRON2", "Kontron CP/M 2.2", 256, 16, 2, 40, SC_DENS_DD, 327680},
    {"KONTRON3", "Kontron PSI 80", 256, 16, 1, 80, SC_DENS_DD, 327680},
    {"KONTRON4", "Kontron PSI 80 DD", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"KRAUSE", "Krause Format 1", 512, 10, 2, 80, SC_DENS_DD, 819200},
    {"KRAUSE2", "Krause Format 2", 512, 10, 1, 40, SC_DENS_DD, 204800},
    {"KRAUSE3", "Krause Format 3 HD", 512, 15, 2, 80, SC_DENS_DD, 1228800},
    {"LANIER", "Lanier", 512, 8, 2, 80, SC_DENS_DD, 655360},
    {"LE-80", "LE-80/3 Langer", 512, 10, 2, 40, SC_DENS_DD, 409600},
    {"LOS-25", "LOS-25 Apothekencomputer", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"M8820", "Monroe 8820", 256, 16, 1, 80, SC_DENS_DD, 327680},
    {"MAYON", "Mayon", 512, 10, 1, 80, SC_DENS_DD, 409600},
    {"MAYON-RS", "Mayon Rueckseite", 512, 10, 1, 80, SC_DENS_DD, 409600},
    {"MAYON2", "Mayon Phoenix 8", 512, 9, 2, 80, SC_DENS_DD, 737280},
    {"MBC2000", "MBC2000", 256, 16, 1, 80, SC_DENS_DD, 327680},
    {"MC-FLO", "MC CP/M Format 1", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"MC-FLO2", "NDR-Kleincomputer", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"MC-FLO3", "MC-Flo 3", 1024, 5, 2, 40, SC_DENS_DD, 409600},
    {"MC-FLO4", "MC CP/M Format 4", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"MC-FLO5", "MC CP/M Format 5 8\"", 1024, 9, 2, 80, SC_DENS_DD, 1474560},
    {"MC-FLO6", "MC CP/M Format 6", 512, 10, 2, 80, SC_DENS_DD, 819200},
    {"MC68000", "MC 68000-Computer", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"MEDACO1", "medacomp System 1", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"MEDACO2", "medacomp System 2", 256, 26, 2, 80, SC_DENS_DD, 1064960},
    {"MEMOTECH", "Memotech", 256, 16, 2, 40, SC_DENS_DD, 327680},
    {"MERCATOR", "Olivetti Mercator-Kasse", 256, 18, 2, 40, SC_DENS_DD, 368640},
    {"MES03", "MES03", 512, 8, 2, 80, SC_DENS_DD, 655360},
    {"MIRO", "Miro 3.5\"", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"MISTRAL", "Mistral-II", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"MODULAR1", "Computer Modular 48tpi", 512, 8, 2, 40, SC_DENS_DD, 327680},
    {"MODULAR2", "Computer Modular 96tpi", 512, 9, 2, 80, SC_DENS_DD, 737280},
    {"MOLECUL", "Molecular USA", 512, 9, 2, 40, SC_DENS_DD, 368640},
    {"MOPPEL", "Moppel CP/M", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"MOPPEL-2", "Moppel CP/M Format 2", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"MOPPEL-3", "Moppel CP/M Format 3", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"MORROW-1", "Morrow Design SS", 1024, 5, 1, 40, SC_DENS_DD, 204800},
    {"MORROW-2", "Morrow Design DS", 1024, 5, 2, 40, SC_DENS_DD, 409600},
    {"MORROW-3", "Morrow Design CP/M 2.2E", 256, 18, 2, 40, SC_DENS_DD, 368640},
    {"MPA", "MPA", 256, 18, 2, 40, SC_DENS_DD, 368640},
    {"MSX", "MSX II CP/M 3.0", 512, 9, 1, 80, SC_DENS_DD, 368640},
    {"MULTI", "Multitron", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"MUPID", "Mupid", 256, 10, 2, 80, SC_DENS_SD, 409600},
    {"NASCOM", "Nascom", 512, 10, 2, 80, SC_DENS_DD, 819200},
    {"NCR1", "NCR Decision Mate V 48tpi", 512, 8, 2, 40, SC_DENS_DD, 327680},
    {"NCR2", "NCR Decision Mate V 96 5x1K", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"NCR3", "NCR Decision Mate V 96 8x512", 512, 8, 2, 80, SC_DENS_DD, 655360},
    {"NEC8000", "NEC-8000", 256, 16, 2, 40, SC_DENS_DD, 327680},
    {"NEC8001A", "NEC 8001 A", 256, 16, 1, 40, SC_DENS_DD, 163840},
    {"NEC8434B", "NEC PC-8434 B 3.5\"", 256, 16, 1, 80, SC_DENS_DD, 327680},
    {"NEC8800", "NEC-8800", 256, 16, 2, 40, SC_DENS_DD, 327680},
    {"NEUHAUS1", "Neuhaus 48tpi", 256, 18, 2, 40, SC_DENS_DD, 368640},
    {"NEUHAUS2", "Neuhaus 96tpi", 256, 18, 2, 80, SC_DENS_DD, 737280},
    {"NEUHAUS3", "Neuhaus Format 3", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"NEUHAUS4", "Neuhaus Format 4", 512, 9, 2, 40, SC_DENS_DD, 368640},
    {"NEVES", "Lear Siegler ADM-3A", 512, 10, 2, 40, SC_DENS_DD, 409600},
    {"NEWBRAIN", "Newbrain, Mayon", 512, 10, 1, 40, SC_DENS_DD, 204800},
    {"NIXDORF", "Nixdorf PC 8810", 512, 10, 2, 80, SC_DENS_DD, 819200},
    {"NOKIA", "Nokia Mikromikko M6", 512, 8, 2, 80, SC_DENS_DD, 655360},
    {"OETTLE-1", "Oettle & Reichler HD", 1024, 9, 2, 80, SC_DENS_DD, 1474560},
    {"OLYMPDT", "Olympia Boss C", 512, 9, 2, 80, SC_DENS_DD, 737280},
    {"OLYMPDT2", "Olympia Boss C F2", 512, 10, 2, 80, SC_DENS_DD, 819200},
    {"OLYMPST1", "Olympia Boss ST F1", 256, 16, 2, 40, SC_DENS_DD, 327680},
    {"OLYMPST2", "Olympia Boss ST F2", 512, 9, 1, 40, SC_DENS_DD, 184320},
    {"OLYMPST3", "Olympia Boss ST F3", 512, 9, 2, 40, SC_DENS_DD, 368640},
    {"OLYMPST4", "Olympia Boss ST F4", 512, 9, 2, 40, SC_DENS_DD, 368640},
    {"OLYTEXT", "Olytest 20", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"ORION", "Orion CSP 5.2", 1024, 5, 2, 40, SC_DENS_DD, 409600},
    {"OSBORNE", "Osborne Executive", 1024, 5, 1, 40, SC_DENS_DD, 204800},
    {"OSBORNE2", "Osborne Single Density", 256, 10, 1, 40, SC_DENS_SD, 102400},
    {"OSBORNE3", "Osborne Executive 96tpi", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"OSBORNE4", "Osborne Mega Disk", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"P2", "Alphatronic P2", 256, 16, 1, 40, SC_DENS_DD, 163840},
    {"P2-2", "Alphatronic P2 96tpi", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"P2-96", "Triumph Adler P2 96tpi", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"P2-RS", "Alphatronic P2 RS", 256, 16, 1, 40, SC_DENS_DD, 163840},
    {"P2-RS2", "Alphatronic P2 RS F2", 256, 16, 1, 40, SC_DENS_DD, 163840},
    {"P2000", "Philips P2000", 256, 16, 1, 40, SC_DENS_DD, 163840},
    {"P2012", "Philips P2012", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"P2500/1", "Philips P2500 300K", 256, 16, 1, 80, SC_DENS_DD, 327680},
    {"P2500/2", "Philips P2500 600K", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"P2L", "Alphatronic P2L", 1024, 5, 2, 40, SC_DENS_DD, 409600},
    {"P3", "Alphatronic P3", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"P3-2", "Alphatronic P3 Texass", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"P3500", "Philips P3000/P3500/P3800", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"P3500-2", "Philips P3500 Format 2", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"P5020", "Philips P5020", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"PC", "Alphatronic PC", 256, 16, 2, 40, SC_DENS_DD, 327680},
    {"PC+2", "PC+ Uni Karlsr. 2k", 512, 10, 2, 80, SC_DENS_DD, 819200},
    {"PC+4", "PC+ Uni Karlsr. 4k", 512, 10, 2, 80, SC_DENS_DD, 819200},
    {"PC-96", "Alphatronic PC 96tpi", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"PC3201-1", "Sharp PC3201", 1024, 5, 2, 40, SC_DENS_DD, 409600},
    {"PC3201-2", "Sharp PC3201 Micro Techn.", 256, 16, 2, 40, SC_DENS_DD, 327680},
    {"PC8000", "Oettle+Reichler PC8000", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"PC8000-2", "Oettle+Reichler PC8000 F2", 512, 9, 2, 80, SC_DENS_DD, 737280},
    {"PEOPLE", "Olympia People", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"PEP1", "PEP Elektronik 48tpi", 256, 16, 2, 40, SC_DENS_DD, 327680},
    {"PEP2", "PEP Elektronik 96tpi", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"PMS", "Siemens PMS T88 D", 512, 9, 1, 80, SC_DENS_DD, 368640},
    {"PROF-4A", "Prof-80 Format 4 Var.", 512, 10, 2, 80, SC_DENS_DD, 819200},
    {"PROF80-2", "Prof-80 Format 2 DS", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"PROF80-3", "Prof-80 Format 3 SS", 1024, 5, 1, 80, SC_DENS_DD, 409600},
    {"PROF80-4", "Prof-80 Format 4", 512, 10, 2, 80, SC_DENS_DD, 819200},
    {"PROF80-5", "Prof-80 Format 5 48tpi", 512, 10, 2, 40, SC_DENS_DD, 409600},
    {"PROF80-6", "Prof-80 Format 6", 512, 10, 2, 80, SC_DENS_DD, 819200},
    {"PROF80-8", "Prof-80 Format 8 HD", 512, 15, 2, 80, SC_DENS_DD, 1228800},
    {"PROTOTEC", "Prototec", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"PX-8", "Epson PX-8", 512, 8, 2, 40, SC_DENS_DD, 327680},
    {"QX10", "Epson QX-10 TF-20", 256, 16, 2, 40, SC_DENS_DD, 327680},
    {"QX10-2", "Epson QX-10 Format 2", 256, 16, 2, 40, SC_DENS_DD, 327680},
    {"QX10-V", "Epson QX-10 Valdocs", 512, 10, 2, 40, SC_DENS_DD, 409600},
    {"QX16", "Epson QX-16", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"QX20", "Epson QX20", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"RC-750", "RC-750", 1024, 8, 2, 80, SC_DENS_DD, 1310720},
    {"RENTIKI", "Rentiki", 512, 10, 1, 40, SC_DENS_DD, 204800},
    {"ROBOT-1", "Robotron Format 1 148k", 256, 16, 1, 40, SC_DENS_DD, 163840},
    {"ROBOT-2", "Robotron Format 2 200k", 1024, 5, 1, 40, SC_DENS_DD, 204800},
    {"ROBOT-3", "Robotron Format 3 800k", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"ROBOT-4", "Robotron SCP 0.5", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"ROBOT-5", "Robotron CP/A 624k", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"RTS-80", "Mico Systems RTS-80", 256, 16, 2, 40, SC_DENS_DD, 327680},
    {"SAGE", "Sage II CP/M-68k", 512, 8, 2, 80, SC_DENS_DD, 655360},
    {"SAN1000", "Sanyo 1000", 256, 16, 2, 40, SC_DENS_DD, 327680},
    {"SAN1160", "Sanyo 1160", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"SANCO1", "Sanco-Ibex 2100", 1024, 5, 2, 40, SC_DENS_DD, 409600},
    {"SANCO2", "Sanco-Ibex 7102/2", 256, 16, 2, 40, SC_DENS_DD, 327680},
    {"SCP-1", "Robotron SCP Format 1", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"SCP-2", "Robotron SCP Format 2", 1024, 5, 1, 80, SC_DENS_DD, 409600},
    {"SCP-3", "Robotron SCP Format 3", 1024, 5, 1, 40, SC_DENS_DD, 204800},
    {"SCP-4", "Robotron SCP Format 4", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"SCP-5", "Robotron SCP Format 5", 256, 16, 1, 80, SC_DENS_DD, 327680},
    {"SCP-6", "Robotron SCP Format 6", 256, 16, 1, 40, SC_DENS_DD, 163840},
    {"SCREENT", "Screentyper", 512, 10, 2, 40, SC_DENS_DD, 409600},
    {"SET", "SET Skiba", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"SHA3541", "Sharp-3541 EOS", 256, 16, 2, 40, SC_DENS_DD, 327680},
    {"SHARP", "Sharp MZ80B", 512, 10, 2, 40, SC_DENS_DD, 409600},
    {"SHARP2", "Sharp EOS", 1024, 5, 2, 40, SC_DENS_DD, 409600},
    {"SHARP3", "Sharp EOS V3Q F1", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"SHARP4", "Sharp EOS V3Q F2", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"SHARP5", "Sharp MZ-800", 512, 8, 2, 40, SC_DENS_DD, 327680},
    {"SIEMENS1", "Siemens PC 16-10", 512, 9, 2, 40, SC_DENS_DD, 368640},
    {"SIEMENS2", "Siemens PMS-E342", 512, 9, 1, 80, SC_DENS_DD, 368640},
    {"SIEMENS3", "Siemens 96tpi DS", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"SIEMENS4", "Siemens PC16-11/PG635", 512, 9, 2, 80, SC_DENS_DD, 737280},
    {"SIEMENS5", "Siemens PMS T85D", 512, 9, 2, 80, SC_DENS_DD, 737280},
    {"SIEMENS6", "Siemens SMP-SYS 900 F2", 512, 9, 2, 80, SC_DENS_DD, 737280},
    {"SIEMENS7", "PMS T85D Format 2", 512, 9, 2, 80, SC_DENS_DD, 737280},
    {"SKSNANO", "SKS Portable CP/M", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"SONY", "Sony Serie 35", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"SPECTRO", "Spectro Test M", 512, 10, 2, 80, SC_DENS_DD, 819200},
    {"SPERRY", "Sperry UTS 30", 512, 9, 2, 80, SC_DENS_DD, 737280},
    {"SUPERBR", "Superbrain", 512, 10, 1, 40, SC_DENS_DD, 204800},
    {"SUPERBR2", "Superbrain F2", 512, 10, 2, 80, SC_DENS_DD, 819200},
    {"SV328", "Spectra Video 328", 256, 17, 1, 40, SC_DENS_DD, 174080},
    {"SV328D", "Spectra Video 328 Super", 256, 17, 2, 40, SC_DENS_DD, 348160},
    {"SVI738", "SVI X'PRESS", 512, 9, 1, 80, SC_DENS_DD, 368640},
    {"SYNELEC", "Synelec", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"SYSTEK1", "Systek SS/SD 8\"", 128, 26, 1, 80, SC_DENS_SD, 266240},
    {"SYSTEK2", "Systek-HD 8\"", 1024, 9, 2, 80, SC_DENS_DD, 1474560},
    {"SYSTEK3", "Systek 3 5.25\" HD", 1024, 9, 2, 80, SC_DENS_DD, 1474560},
    {"SYSTEK4", "Systek-HD 8\" F2", 512, 16, 2, 80, SC_DENS_DD, 1310720},
    {"SYSTEK5", "Systek 5 3.5\" DD", 512, 10, 2, 80, SC_DENS_DD, 819200},
    {"SYSTRON", "Systron S800 TurboDOS", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"TANDY", "Tandy TRS-80 Model IV", 256, 18, 1, 40, SC_DENS_DD, 184320},
    {"TAYLOR-2", "Taylorix F2", 256, 26, 2, 80, SC_DENS_DD, 1064960},
    {"TAYLORIX", "Taylorix", 256, 26, 2, 80, SC_DENS_DD, 1064960},
    {"TE703", "Tekelec Chameleon II", 1024, 5, 1, 80, SC_DENS_DD, 409600},
    {"TEXASS", "Alphatronic P2 TexAss", 256, 16, 1, 40, SC_DENS_DD, 163840},
    {"TOPPER", "Topper Beehive", 512, 10, 2, 40, SC_DENS_DD, 409600},
    {"TOSHIBA", "Toshiba T100", 256, 16, 2, 40, SC_DENS_DD, 327680},
    {"TRS-M3", "TRS-80 MIII / FEC CP/M", 256, 18, 1, 40, SC_DENS_DD, 184320},
    {"TRS-M4.1", "TRS-M4 F1", 256, 18, 1, 40, SC_DENS_DD, 184320},
    {"TRS-M4.2", "TRS-M4 F2", 256, 18, 1, 40, SC_DENS_DD, 184320},
    {"TRS-M4.3", "TRS-M4 F3", 256, 18, 2, 80, SC_DENS_DD, 737280},
    {"TRS-M4.4", "TRS-M4 F4", 256, 18, 2, 80, SC_DENS_DD, 737280},
    {"TRS-M4.5", "TRS-80 M4P", 512, 8, 1, 40, SC_DENS_DD, 163840},
    {"TV1603", "Televideo 1603", 512, 9, 2, 80, SC_DENS_DD, 737280},
    {"TV803", "Televideo 803", 256, 18, 2, 40, SC_DENS_DD, 368640},
    {"TYCOM", "Tycom CP/M86", 512, 9, 2, 80, SC_DENS_DD, 737280},
    {"UNI-SB", "Uni-SB 3", 1024, 8, 2, 80, SC_DENS_DD, 1310720},
    {"UNICOM", "Unicom", 256, 16, 2, 80, SC_DENS_DD, 655360},
    {"VIDEO1", "Video Genie III F1", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"VIDEO2", "Video Genie 3 Vers B", 512, 10, 2, 80, SC_DENS_DD, 819200},
    {"VIDEO3", "Video Genie III", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"VIDEO3A", "Video Genie III Vers. 2", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"VIDEO4", "Video Genie III S", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"VIDEO5", "Video Genie III F5", 256, 18, 2, 40, SC_DENS_DD, 368640},
    {"VIDEO5A", "Genie III 48tpi auf 96tpi", 256, 18, 2, 80, SC_DENS_DD, 737280},
    {"VT180", "DEC VT 180", 512, 9, 1, 40, SC_DENS_DD, 184320},
    {"WEE", "WEE CP/M 3.5\"", 1024, 5, 2, 80, SC_DENS_DD, 819200},
    {"WEMPE", "Wempe", 1024, 8, 2, 80, SC_DENS_DD, 1310720},
    {"XEROX", "XEROX 16/8 820 II", 512, 9, 2, 40, SC_DENS_DD, 368640},
    {"XEROX-2", "XEROX 3700", 256, 17, 2, 40, SC_DENS_DD, 348160},
    {"Z90", "Zenith Z90", 256, 16, 1, 40, SC_DENS_DD, 163840},
    {"ZENITH", "Zenith", 512, 8, 2, 40, SC_DENS_DD, 327680},
    {"ZENITH2", "Zenith Z90 96tpi", 256, 16, 2, 80, SC_DENS_DD, 655360},
};


/**
 * Alle Formate suchen, die zur gegebenen Geometrie passen.
 *
 * @param sector_size    Sektorgröße in Bytes
 * @param spt            Sektoren pro Spur
 * @param heads          Köpfe (1 oder 2)
 * @param cylinders      Zylinder (40 oder 80)
 * @param matches        Ausgabe-Array für Treffer
 * @param max_matches    Maximale Anzahl Treffer
 * @return Anzahl gefundener Formate
 */
static inline uint16_t supercopy_find_by_geometry(
    uint16_t sector_size, uint8_t spt, uint8_t heads, uint16_t cylinders,
    const supercopy_format_t **matches, uint16_t max_matches)
{
    uint16_t count = 0;
    for (uint16_t i = 0; i < SUPERCOPY_FORMAT_COUNT && count < max_matches; i++) {
        const supercopy_format_t *f = &supercopy_formats[i];
        if (f->sector_size == sector_size &&
            f->sectors_per_track == spt &&
            f->heads == heads &&
            f->cylinders == cylinders) {
            matches[count++] = f;
        }
    }
    return count;
}

/**
 * Format per Name suchen.
 */
static inline const supercopy_format_t *supercopy_find_by_name(const char *name)
{
    for (uint16_t i = 0; i < SUPERCOPY_FORMAT_COUNT; i++) {
        if (strcmp(supercopy_formats[i].name, name) == 0)
            return &supercopy_formats[i];
    }
    return NULL;
}

/**
 * Alle Formate mit gleicher Disk-Kapazität suchen.
 */
static inline uint16_t supercopy_find_by_capacity(
    uint32_t total_bytes,
    const supercopy_format_t **matches, uint16_t max_matches)
{
    uint16_t count = 0;
    for (uint16_t i = 0; i < SUPERCOPY_FORMAT_COUNT && count < max_matches; i++) {
        if (supercopy_formats[i].total_bytes == total_bytes)
            matches[count++] = &supercopy_formats[i];
    }
    return count;
}

#endif /* SUPERCOPY_FORMATS_H */
