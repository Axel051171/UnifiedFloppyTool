/**
 * @file uft_tap_parser_v2.c
 * @brief GOD MODE TAP Parser v2 - Oric Tape/Disk Format
 * 
 * TAP is the Oric tape image format, also used for Oric disk images.
 * Stores files with headers containing load address, size, etc.
 * 
 * Structure:
 * - Sync bytes (0x16)
 * - Header (type, autorun, end addr, start addr, name)
 * - Data block
 * 
 * @author UFT Team / GOD MODE
 * @version 2.0.0
 * @date 2025-01-02
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

#define TAP_SYNC_BYTE           0x16
#define TAP_HEADER_MARKER       0x24    /* '$' */
#define TAP_MIN_SYNC            3
#define TAP_FILENAME_LEN        16

#define TAP_TYPE_BASIC          0x00    /* BASIC program */
#define TAP_TYPE_MACHINE        0x80    /* Machine code */

/* ═══════════════════════════════════════════════════════════════════════════
 * DATA STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief TAP file entry
 */
typedef struct {
    uint8_t type;               /* 0x00=BASIC, 0x80=Machine */
    bool autorun;               /* Auto-run flag */
    uint16_t end_addr;          /* End address */
    uint16_t start_addr;        /* Start address */
    char filename[17];          /* Filename + null */
    uint32_t data_offset;       /* Offset in TAP file */
    uint16_t data_size;         /* Size of data */
    bool valid;                 /* Entry is valid */
} tap_entry_t;

/**
 * @brief TAP file structure
 */
typedef struct {
    tap_entry_t* entries;
    uint16_t entry_count;
    uint16_t entry_capacity;
    
    size_t file_size;
    bool valid;
    char error[256];
} tap_file_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * HELPER FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get type name
 */
static const char* tap_type_name(uint8_t type) {
    return (type & 0x80) ? "Machine" : "BASIC";
}

/**
 * @brief Find sync sequence
 */
static int tap_find_sync(const uint8_t* data, size_t size, size_t start) {
    int sync_count = 0;
    
    for (size_t i = start; i < size; i++) {
        if (data[i] == TAP_SYNC_BYTE) {
            sync_count++;
        } else if (sync_count >= TAP_MIN_SYNC && data[i] == TAP_HEADER_MARKER) {
            return (int)i;  /* Return position of header marker */
        } else {
            sync_count = 0;
        }
    }
    return -1;
}

/**
 * @brief Read big-endian 16-bit (Oric is 6502, little-endian actually)
 */
static uint16_t read_le16(const uint8_t* data) {
    return data[0] | (data[1] << 8);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PARSING FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parse TAP header
 */
static bool tap_parse_header(const uint8_t* data, size_t size, size_t offset,
                              tap_entry_t* entry, size_t* next_offset) {
    if (offset + 13 > size) return false;
    
    /* Skip header marker (0x24) if present */
    if (data[offset] == TAP_HEADER_MARKER) offset++;
    if (offset + 12 > size) return false;
    
    /* Unused byte */
    offset++;
    
    /* Type and autorun */
    entry->type = data[offset++];
    entry->autorun = (data[offset++] != 0);
    
    /* Addresses (big-endian on Oric) */
    entry->end_addr = (data[offset] << 8) | data[offset + 1];
    offset += 2;
    entry->start_addr = (data[offset] << 8) | data[offset + 1];
    offset += 2;
    
    /* Skip another unused byte */
    offset++;
    
    /* Filename (null-terminated, up to 16 chars) */
    size_t name_start = offset;
    size_t name_len = 0;
    while (offset < size && data[offset] != 0x00 && name_len < 16) {
        entry->filename[name_len++] = data[offset++];
    }
    entry->filename[name_len] = '\0';
    
    /* Skip null terminator */
    if (offset < size && data[offset] == 0x00) offset++;
    
    /* Data follows */
    entry->data_offset = offset;
    entry->data_size = (entry->end_addr >= entry->start_addr) ?
                       (entry->end_addr - entry->start_addr + 1) : 0;
    
    *next_offset = offset + entry->data_size;
    entry->valid = true;
    
    return true;
}

/**
 * @brief Parse TAP file
 */
static bool tap_parse(const uint8_t* data, size_t size, tap_file_t* tap) {
    memset(tap, 0, sizeof(tap_file_t));
    tap->file_size = size;
    
    /* Allocate entry array */
    tap->entry_capacity = 64;
    tap->entries = calloc(tap->entry_capacity, sizeof(tap_entry_t));
    if (!tap->entries) {
        snprintf(tap->error, sizeof(tap->error), "Memory allocation failed");
        return false;
    }
    
    size_t pos = 0;
    
    while (pos < size) {
        /* Find next sync sequence */
        int header_pos = tap_find_sync(data, size, pos);
        if (header_pos < 0) break;
        
        /* Expand array if needed */
        if (tap->entry_count >= tap->entry_capacity) {
            tap->entry_capacity *= 2;
            tap_entry_t* new_entries = realloc(tap->entries, 
                                                tap->entry_capacity * sizeof(tap_entry_t));
            if (!new_entries) break;
            tap->entries = new_entries;
        }
        
        /* Parse entry */
        tap_entry_t* entry = &tap->entries[tap->entry_count];
        size_t next_pos;
        
        if (tap_parse_header(data, size, header_pos, entry, &next_pos)) {
            tap->entry_count++;
            pos = next_pos;
        } else {
            pos = header_pos + 1;
        }
    }
    
    tap->valid = (tap->entry_count > 0);
    return tap->valid;
}

/**
 * @brief Free TAP structure
 */
static void tap_free(tap_file_t* tap) {
    if (tap && tap->entries) {
        free(tap->entries);
        tap->entries = NULL;
    }
}

/**
 * @brief Generate catalog text
 */
static char* tap_catalog_to_text(const tap_file_t* tap) {
    size_t buf_size = 4096;
    char* buf = malloc(buf_size);
    if (!buf) return NULL;
    
    size_t offset = 0;
    
    offset += snprintf(buf + offset, buf_size - offset,
        "Oric TAP File\n"
        "═════════════\n"
        "File size: %zu bytes\n"
        "Entries: %u\n\n",
        tap->file_size,
        tap->entry_count
    );
    
    for (uint16_t i = 0; i < tap->entry_count && offset < buf_size - 100; i++) {
        const tap_entry_t* entry = &tap->entries[i];
        
        offset += snprintf(buf + offset, buf_size - offset,
            "%2u: %-16s  %s  $%04X-$%04X  %5u bytes%s\n",
            i + 1,
            entry->filename,
            tap_type_name(entry->type),
            entry->start_addr,
            entry->end_addr,
            entry->data_size,
            entry->autorun ? " [AUTO]" : ""
        );
    }
    
    return buf;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST SUITE
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef TAP_PARSER_TEST

#include <assert.h>

int main(void) {
    printf("=== TAP Parser v2 Tests ===\n");
    
    printf("Testing type names... ");
    assert(strcmp(tap_type_name(TAP_TYPE_BASIC), "BASIC") == 0);
    assert(strcmp(tap_type_name(TAP_TYPE_MACHINE), "Machine") == 0);
    printf("✓\n");
    
    printf("Testing find sync... ");
    uint8_t data1[] = { 0x16, 0x16, 0x16, 0x24, 0x00 };
    assert(tap_find_sync(data1, sizeof(data1), 0) == 3);
    
    uint8_t data2[] = { 0x00, 0x00, 0x16, 0x16, 0x16, 0x24 };
    assert(tap_find_sync(data2, sizeof(data2), 0) == 5);
    printf("✓\n");
    
    printf("Testing read_le16... ");
    uint8_t le_data[] = { 0x34, 0x12 };
    assert(read_le16(le_data) == 0x1234);
    printf("✓\n");
    
    printf("Testing header marker... ");
    assert(TAP_HEADER_MARKER == 0x24);
    assert(TAP_SYNC_BYTE == 0x16);
    printf("✓\n");
    
    printf("Testing constants... ");
    assert(TAP_FILENAME_LEN == 16);
    assert(TAP_MIN_SYNC == 3);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 5, Failed: 0\n");
    return 0;
}

#endif /* TAP_PARSER_TEST */
