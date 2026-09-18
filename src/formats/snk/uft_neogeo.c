/**
 * @file uft_neogeo.c
 * @brief SNK Neo Geo ROM Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/snk/uft_neogeo.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint32_t read_le32(const uint8_t *data)
{
    return data[0] | (data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

/* ============================================================================
 * Detection
 * ============================================================================ */

bool neogeo_is_neo_format(const uint8_t *data, size_t size)
{
    if (!data || size < NEO_HEADER_SIZE) return false;
    return memcmp(data, NEO_MAGIC, NEO_MAGIC_SIZE) == 0;
}

/* MF-1238: Gross-/Kleinwandlung von Hand auf ASCII, statt `toupper()`
 * zu rufen. Der Grund ist nicht Stil: `toupper(name[i])` mit einem
 * blanken `char` ist fuer jedes Byte >= 0x80 UNDEFINIERT, weil `char`
 * hier vorzeichenbehaftet ist und der negative Wert kein gueltiges
 * Argument ist. Ein Dateiname mit Umlaut reichte. Dieselbe Vorsicht
 * uebt `lc()` in `src/util/uft_match.c`. */
static char neo_gross(char c)
{
    return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

/* MF-1238: Traegt der Name an der Stelle @p p die Chipmarke
 * `-<buchstabe><ziffer>`?
 *
 * DAS IST EINE HAUSREGEL, und sie wird als solche benannt (Lehre aus
 * MF-1038, wo eine Hausregel als Formateigenschaft gelesen wurde).
 * Belegt ist sie an den fuenf vorhandenen Testzusagen dieses Baums
 * (`tests/test_neogeo.c:136-140`: `001-p1.bin` bis `001-c1.bin`) und an
 * den Kommentaren der Aufzaehlung (`uft_neogeo.h:44-50`). Eine FREMDE
 * Beschreibung der Namenskonvention liegt NICHT vor. */
static bool neo_marke_hier(const char *p, char buchstabe)
{
    if (p[0] != '-') return false;
    if (neo_gross(p[1]) != buchstabe) return false;
    /* Die Ziffer ist das Entscheidende: sie trennt `001-p1.bin` von
     * `MVS-PACK.ROM`. Ohne sie traf `-P` in jedem Namen, der einen
     * Bindestrich und ein P hintereinander hatte. */
    return p[2] >= '0' && p[2] <= '9';
}

neogeo_rom_type_t neogeo_detect_chip_type(const char *filename)
{
    /* MF-1238: hier stand eine Kette
     *
     *   if (upper[0] == 'P' || strstr(upper, "-P")) return NEO_ROM_P;
     *
     * ueber einen auf 15 Zeichen gekappten, mit `toupper()`
     * grossgeschriebenen Namen — und das trug FUENF Defekte auf
     * zweiundzwanzig Zeilen:
     *
     *  1. `upper[0] == 'P'` entschied nach dem ERSTEN BUCHSTABEN —
     *     `PUZZLE.BIN` war damit ein Programm-ROM, `CHAR.BIN` ein
     *     Character-ROM. Die Regel ist weg; die fuenf vorhandenen
     *     Testzusagen bemerkten sie nie, weil ihre Namen mit `0`
     *     beginnen.
     *  2. `strstr(upper, "-P")` traf UEBERALL: `MVS-PACK.ROM` war ein
     *     Programm-ROM. Zwei Zeichen, irgendwo im Namen gesucht.
     *  3. Der Name wurde bei 15 Zeichen STILL gekappt. Ein laengerer
     *     Name verlor seine Marke und fiel auf die Vorgabe.
     *  4. `toupper(name[i])` mit blankem `char` ist fuer Bytes >= 0x80
     *     undefiniert; siehe `neo_gross()` oben.
     *  5. Die Pruefreihenfolge entschied: ein Name mit zwei Marken
     *     bekam die, die zuerst geprueft wurde.
     *
     * Gesucht wird jetzt die Marke `-<buchstabe><ziffer>` an JEDER
     * Stelle des VOLLEN Namens, und es wird die LETZTE genommen — bei
     * `001-c1-p1.bin` entscheidet damit die Lage im Namen und nicht
     * die Reihenfolge im Quelltext. */
    if (!filename) return NEO_ROM_P;

    /* Letzte Pfadkomponente. Vorher wurde `strrchr(…, '\\')` nur
     * geprueft, WENN `'/'` fehlte — ein Pfad wie `C:/spiele\001-p1.bin`
     * war damit falsch zerlegt. Jetzt gewinnt der spaetere der beiden
     * Trenner. */
    const char *s1 = strrchr(filename, '/');
    const char *s2 = strrchr(filename, '\\');
    const char *name = (s1 > s2) ? s1 : s2;
    name = name ? name + 1 : filename;

    static const struct { char b; neogeo_rom_type_t t; } MARKEN[] = {
        { 'P', NEO_ROM_P }, { 'S', NEO_ROM_S }, { 'M', NEO_ROM_M },
        { 'V', NEO_ROM_V }, { 'C', NEO_ROM_C }
    };

    bool gefunden = false;
    neogeo_rom_type_t typ = NEO_ROM_P;
    for (const char *p = name; p[0] && p[1] && p[2]; p++) {
        for (size_t i = 0; i < sizeof MARKEN / sizeof MARKEN[0]; i++) {
            if (neo_marke_hier(p, MARKEN[i].b)) {
                typ = MARKEN[i].t;
                gefunden = true;
            }
        }
    }
    if (gefunden) return typ;

    /* MF-1238, GESTOPPT (S5): hier stand die Vorgabe mit dem Kommentar
     * „Default to P-ROM" — und sie BEHAUPTET einen Typ, wo keiner
     * erkannt wurde. Sie bleibt trotzdem stehen, weil der TYP es nicht
     * anders kann: `neogeo_rom_type_t` hat keinen UNKNOWN-Wert, und
     * `NEO_ROM_P = 0` ist der erste (`uft_neogeo.h:44-50`). Ein
     * `NEO_ROM_UNKNOWN` waere additiv moeglich (Wert 5, bestehende
     * Werte unveraendert), ist aber eine Aenderung an einem
     * oeffentlichen Header und eine Entscheidung ueber den Vertrag —
     * kein Nebeneffekt einer Teilstring-Korrektur. Benannt statt
     * stillschweigend gelassen. */
    return NEO_ROM_P;
}

const char *neogeo_system_name(neogeo_system_t system)
{
    switch (system) {
        case NEO_SYSTEM_MVS:    return "MVS (Arcade)";
        case NEO_SYSTEM_AES:    return "AES (Home)";
        case NEO_SYSTEM_CD:     return "Neo Geo CD";
        case NEO_SYSTEM_CDZ:    return "Neo Geo CDZ";
        default:                return "Unknown";
    }
}

const char *neogeo_region_name(neogeo_region_t region)
{
    switch (region) {
        case NEO_REGION_JAPAN:  return "Japan";
        case NEO_REGION_USA:    return "USA";
        case NEO_REGION_EUROPE: return "Europe";
        case NEO_REGION_ASIA:   return "Asia";
        default:                return "Unknown";
    }
}

const char *neogeo_rom_type_name(neogeo_rom_type_t type)
{
    switch (type) {
        case NEO_ROM_P: return "P-ROM (Program)";
        case NEO_ROM_S: return "S-ROM (Fix/Text)";
        case NEO_ROM_M: return "M-ROM (Z80 Music)";
        case NEO_ROM_V: return "V-ROM (Voice/ADPCM)";
        case NEO_ROM_C: return "C-ROM (Character/Sprite)";
        default:        return "Unknown";
    }
}

/* ============================================================================
 * ROM Operations
 * ============================================================================ */

int neogeo_open(const uint8_t *data, size_t size, neogeo_rom_t *rom)
{
    if (!data || !rom) return -1;
    
    memset(rom, 0, sizeof(neogeo_rom_t));
    
    rom->is_neo_format = neogeo_is_neo_format(data, size);
    
    if (rom->is_neo_format) {
        if (size < NEO_HEADER_SIZE) return -2;
        
        /* Parse NEO header */
        memcpy(rom->header.magic, data, 4);
        rom->header.p_rom_size = read_le32(data + 4);
        rom->header.s_rom_size = read_le32(data + 8);
        rom->header.m_rom_size = read_le32(data + 12);
        rom->header.v_rom_size = read_le32(data + 16);
        rom->header.c_rom_size = read_le32(data + 20);
        rom->header.year = read_le32(data + 24);
        rom->header.genre = read_le32(data + 28);
        rom->header.screenshot = read_le32(data + 32);
        rom->header.ngh = read_le32(data + 36);
        memcpy(rom->header.name, data + 40, 32);
        rom->header.name[32] = '\0';
        memcpy(rom->header.manufacturer, data + 72, 16);
        rom->header.manufacturer[16] = '\0';
        
        /* Calculate offsets */
        rom->p_offset = NEO_HEADER_SIZE;
        rom->s_offset = rom->p_offset + rom->header.p_rom_size;
        rom->m_offset = rom->s_offset + rom->header.s_rom_size;
        rom->v_offset = rom->m_offset + rom->header.m_rom_size;
        rom->c_offset = rom->v_offset + rom->header.v_rom_size;
        
        /* Validate size */
        size_t expected = NEO_HEADER_SIZE + 
                          rom->header.p_rom_size +
                          rom->header.s_rom_size +
                          rom->header.m_rom_size +
                          rom->header.v_rom_size +
                          rom->header.c_rom_size;
        
        if (size < expected) return -3;
    }
    
    /* Copy data */
    rom->data = malloc(size);
    if (!rom->data) return -4;
    
    memcpy(rom->data, data, size);
    rom->size = size;
    rom->owns_data = true;
    
    return 0;
}

int neogeo_load(const char *filename, neogeo_rom_t *rom)
{
    if (!filename || !rom) return -1;
    
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -2;
    
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(fp);
        return -3;
    }
    
    if (fread(data, 1, size, fp) != size) {
        free(data);
        fclose(fp);
        return -4;
    }
    fclose(fp);
    
    int result = neogeo_open(data, size, rom);
    free(data);
    
    return result;
}

void neogeo_close(neogeo_rom_t *rom)
{
    if (!rom) return;
    
    if (rom->owns_data) {
        free(rom->data);
    }
    
    memset(rom, 0, sizeof(neogeo_rom_t));
}

int neogeo_get_info(const neogeo_rom_t *rom, neogeo_info_t *info)
{
    if (!rom || !info) return -1;
    
    memset(info, 0, sizeof(neogeo_info_t));
    
    info->is_neo_format = rom->is_neo_format;
    info->total_size = rom->size;
    
    if (rom->is_neo_format) {
        strncpy(info->name, rom->header.name, 63);
        strncpy(info->manufacturer, rom->header.manufacturer, 31);
        info->ngh = rom->header.ngh;
        info->year = rom->header.year;
        info->p_size = rom->header.p_rom_size;
        info->s_size = rom->header.s_rom_size;
        info->m_size = rom->header.m_rom_size;
        info->v_size = rom->header.v_rom_size;
        info->c_size = rom->header.c_rom_size;
    }
    
    /* Default to MVS for .neo format */
    info->system = rom->is_neo_format ? NEO_SYSTEM_MVS : NEO_SYSTEM_UNKNOWN;
    
    return 0;
}

/* ============================================================================
 * ROM Access
 * ============================================================================ */

const uint8_t *neogeo_get_prom(const neogeo_rom_t *rom, size_t *size)
{
    if (!rom || !rom->is_neo_format) return NULL;
    if (size) *size = rom->header.p_rom_size;
    return rom->data + rom->p_offset;
}

const uint8_t *neogeo_get_srom(const neogeo_rom_t *rom, size_t *size)
{
    if (!rom || !rom->is_neo_format) return NULL;
    if (size) *size = rom->header.s_rom_size;
    return rom->data + rom->s_offset;
}

const uint8_t *neogeo_get_mrom(const neogeo_rom_t *rom, size_t *size)
{
    if (!rom || !rom->is_neo_format) return NULL;
    if (size) *size = rom->header.m_rom_size;
    return rom->data + rom->m_offset;
}

const uint8_t *neogeo_get_vrom(const neogeo_rom_t *rom, size_t *size)
{
    if (!rom || !rom->is_neo_format) return NULL;
    if (size) *size = rom->header.v_rom_size;
    return rom->data + rom->v_offset;
}

const uint8_t *neogeo_get_crom(const neogeo_rom_t *rom, size_t *size)
{
    if (!rom || !rom->is_neo_format) return NULL;
    if (size) *size = rom->header.c_rom_size;
    return rom->data + rom->c_offset;
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

void neogeo_print_info(const neogeo_rom_t *rom, FILE *fp)
{
    if (!rom || !fp) return;
    
    neogeo_info_t info;
    neogeo_get_info(rom, &info);
    
    fprintf(fp, "Neo Geo ROM:\n");
    fprintf(fp, "  Format: %s\n", info.is_neo_format ? ".neo container" : "Raw ROM");
    fprintf(fp, "  Total Size: %zu bytes (%.1f MB)\n",
            info.total_size, info.total_size / 1048576.0);
    
    if (info.is_neo_format) {
        fprintf(fp, "  Name: %s\n", info.name);
        fprintf(fp, "  Manufacturer: %s\n", info.manufacturer);
        fprintf(fp, "  NGH: %03u\n", info.ngh);
        fprintf(fp, "  Year: %u\n", info.year);
        fprintf(fp, "  P-ROM: %zu KB\n", info.p_size / 1024);
        fprintf(fp, "  S-ROM: %zu KB\n", info.s_size / 1024);
        fprintf(fp, "  M-ROM: %zu KB\n", info.m_size / 1024);
        fprintf(fp, "  V-ROM: %zu KB\n", info.v_size / 1024);
        fprintf(fp, "  C-ROM: %zu KB\n", info.c_size / 1024);
    }
}
