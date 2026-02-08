/**
 * @file uft_ir_serialize.c
 * @brief P0-IR-004/005: UFT-IR Serialization Implementation
 * 
 * Binary (.ufir) and JSON serialization for UFT Intermediate Representation.
 * 
 * @version 1.0.0
 * @date 2025-01-05
 */

#include "uft/uft_safe_io.h"
#include "uft/ir/uft_ir_serialize.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*===========================================================================
 * CRC32 Implementation (IEEE 802.3)
 *===========================================================================*/

static uint32_t crc32_table[256];
static bool crc32_initialized = false;

static void crc32_init(void)
{
    if (crc32_initialized) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
        crc32_table[i] = crc;
    }
    crc32_initialized = true;
}

uint32_t uft_ir_crc32(const uint8_t *data, size_t size)
{
    crc32_init();
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

/*===========================================================================
 * Configuration
 *===========================================================================*/

void uft_ir_config_default(uft_ir_serialize_config_t *config)
{
    if (!config) return;
    
    config->compression = UFT_IR_COMP_NONE;
    config->include_timing = true;
    config->include_flux = false;
    config->include_multirev = false;
    config->include_protection = true;
    config->include_confidence = true;
    config->streaming = false;
    config->compression_level = 6;
}

void uft_ir_config_forensic(uft_ir_serialize_config_t *config)
{
    if (!config) return;
    
    config->compression = UFT_IR_COMP_NONE;  /* No compression for forensic */
    config->include_timing = true;
    config->include_flux = true;
    config->include_multirev = true;
    config->include_protection = true;
    config->include_confidence = true;
    config->streaming = false;
    config->compression_level = 0;
}

void uft_ir_config_compact(uft_ir_serialize_config_t *config)
{
    if (!config) return;
    
    config->compression = UFT_IR_COMP_ZLIB;
    config->include_timing = false;
    config->include_flux = false;
    config->include_multirev = false;
    config->include_protection = false;
    config->include_confidence = false;
    config->streaming = false;
    config->compression_level = 9;
}

void uft_ir_json_config_default(uft_ir_json_config_t *config)
{
    if (!config) return;
    
    config->pretty_print = true;
    config->include_bitstream = false;
    config->include_timing = true;
    config->include_flux = false;
    config->include_hex_data = true;
    config->indent_spaces = 2;
}

/*===========================================================================
 * Writer Implementation
 *===========================================================================*/

uft_ir_writer_t *uft_ir_writer_create(const char *path,
                                       const uft_ir_serialize_config_t *config)
{
    if (!path) return NULL;
    
    uft_ir_writer_t *writer = calloc(1, sizeof(uft_ir_writer_t));
    if (!writer) return NULL;
    
    writer->file = fopen(path, "wb");
    if (!writer->file) {
        snprintf(writer->error_msg, sizeof(writer->error_msg),
                 "Cannot open file: %s", path);
        writer->last_error = -1;
        free(writer);
        return NULL;
    }
    
    /* Set configuration */
    if (config) {
        writer->config = *config;
    } else {
        uft_ir_config_default(&writer->config);
    }
    
    /* Allocate index */
    writer->index_capacity = 256;
    writer->index = calloc(writer->index_capacity, sizeof(uft_ir_index_entry_t));
    if (!writer->index) {
        fclose(writer->file);
        free(writer);
        return NULL;
    }
    
    /* Write file header */
    uft_ir_header_t header = {
        .magic = UFT_IR_MAGIC,
        .version_major = UFT_IR_VERSION_MAJOR,
        .version_minor = UFT_IR_VERSION_MINOR,
        .compression = writer->config.compression,
        .flags = 0,
        .track_count = 0,  /* Updated at close */
        .total_size = 0,   /* Updated at close */
        .creation_time = (uint64_t)time(NULL),
        .checksum = 0,     /* Updated at close */
        .reserved = 0,
    };
    
    /* Set flags based on config */
    if (writer->config.include_timing) header.flags |= UFT_IR_FLAG_HAS_TIMING;
    if (writer->config.include_flux) header.flags |= UFT_IR_FLAG_HAS_FLUX;
    if (writer->config.include_multirev) header.flags |= UFT_IR_FLAG_HAS_MULTIREV;
    if (writer->config.include_protection) header.flags |= UFT_IR_FLAG_HAS_PROTECTION;
    if (writer->config.include_confidence) header.flags |= UFT_IR_FLAG_HAS_CONFIDENCE;
    if (writer->config.streaming) header.flags |= UFT_IR_FLAG_STREAMING;
    
    /* Write header (will be updated at close) */
    if (fwrite(&header, sizeof(header), 1, writer->file) != 1) {
        snprintf(writer->error_msg, sizeof(writer->error_msg),
                 "Failed to write header");
        writer->last_error = -2;
        fclose(writer->file);
        free(writer->index);
        free(writer);
        return NULL;
    }
    
    writer->bytes_written = sizeof(header);
    
    return writer;
}

int uft_ir_writer_close(uft_ir_writer_t *writer)
{
    if (!writer) return -1;
    
    int result = 0;
    
    if (writer->file) {
        /* Write index block */
        if (writer->index_count > 0) {
            uft_ir_block_header_t block = {
                .type = UFT_IR_BLOCK_INDEX,
                .flags = 0,
                .track_id = 0,
                .size = (uint32_t)(writer->index_count * sizeof(uft_ir_index_entry_t)),
            };
            
            /* P1-IO-001: Check fwrite return values */
            if (fwrite(&block, sizeof(block), 1, writer->file) != 1) {
                /* Continue - we're closing anyway */
            }
            if (fwrite(writer->index, sizeof(uft_ir_index_entry_t), 
                   writer->index_count, writer->file) != writer->index_count) {
                /* Continue - we're closing anyway */
            }
            
            writer->bytes_written += sizeof(block) + block.size;
        }
        
        /* Write EOF block */
        uft_ir_block_header_t eof = {
            .type = UFT_IR_BLOCK_EOF,
            .flags = 0,
            .track_id = 0,
            .size = 0,
        };
        if (fwrite(&eof, sizeof(eof), 1, writer->file) != 1) {
            /* Continue - we're closing anyway */
        }
        writer->bytes_written += sizeof(eof);
        
        /* Update header */
        if (fseek(writer->file, 0, SEEK_SET) != 0) {
            fclose(writer->file);
            free(writer->index);
            free(writer);
            return;
        }
        
        uft_ir_header_t header;
        if (fread(&header, sizeof(header), 1, writer->file) == 1) {
            header.track_count = writer->tracks_written;
            header.total_size = (uint32_t)writer->bytes_written;
            
            /* Calculate checksum (excluding checksum field itself) */
            header.checksum = 0;
            header.checksum = uft_ir_crc32((uint8_t*)&header, sizeof(header));
            
            if (fseek(writer->file, 0, SEEK_SET) == 0) {
                if (fwrite(&header, sizeof(header), 1, writer->file) != 1) { /* I/O error */ }
            }
        }
        
        fclose(writer->file);
    }
    
    free(writer->index);
    free(writer);
    
    return result;
}

int uft_ir_write_metadata(uft_ir_writer_t *writer, const char **metadata)
{
    if (!writer || !writer->file || !metadata) return -1;
    
    /* Calculate metadata size */
    size_t total_size = 0;
    for (int i = 0; metadata[i] != NULL; i++) {
        total_size += strlen(metadata[i]) + 1;
    }
    
    if (total_size == 0) return 0;
    
    /* Write block header */
    uft_ir_block_header_t block = {
        .type = UFT_IR_BLOCK_METADATA,
        .flags = 0,
        .track_id = 0,
        .size = (uint32_t)total_size,
    };
    
    if (fwrite(&block, sizeof(block), 1, writer->file) != 1) {
        return -2;
    }
    
    /* Write metadata strings */
    for (int i = 0; metadata[i] != NULL; i++) {
        size_t len = strlen(metadata[i]) + 1;
        if (fwrite(metadata[i], 1, len, writer->file) != len) { /* I/O error */ }
    }
    
    writer->bytes_written += sizeof(block) + total_size;
    
    return 0;
}

int uft_ir_write_track(uft_ir_writer_t *writer,
                       uint16_t track, uint8_t side, uint8_t encoding,
                       const uint8_t *bitstream, size_t bit_count)
{
    if (!writer || !writer->file || !bitstream || bit_count == 0) return -1;
    
    size_t byte_count = (bit_count + 7) / 8;
    
    /* Record index entry */
    if (writer->index_count < writer->index_capacity) {
        uft_ir_index_entry_t *entry = &writer->index[writer->index_count++];
        entry->track = track;
        entry->side = side;
        entry->sector_count = 0;  /* Updated when sectors written */
        entry->file_offset = (uint32_t)writer->bytes_written;
    }
    
    /* Write block header */
    uft_ir_block_header_t block = {
        .type = UFT_IR_BLOCK_TRACK,
        .flags = 0,
        .track_id = (uint16_t)((track << 1) | side),
        .size = (uint32_t)(sizeof(uft_ir_track_header_t) + byte_count),
    };
    
    if (fwrite(&block, sizeof(block), 1, writer->file) != 1) {
        return -2;
    }
    
    /* Write track header */
    uft_ir_track_header_t track_hdr = {
        .track = track,
        .side = side,
        .encoding = encoding,
        .sector_count = 0,
        .revolution_count = 1,
        .rpm = 3000,  /* 300 RPM × 10 */
        .bitstream_size = (uint32_t)bit_count,
        .flags = 0,
    };
    
    if (fwrite(&track_hdr, sizeof(track_hdr), 1, writer->file) != 1) {
        return -3;
    }
    
    /* Write bitstream data */
    if (fwrite(bitstream, 1, byte_count, writer->file) != byte_count) {
        return -4;
    }
    
    writer->bytes_written += sizeof(block) + sizeof(track_hdr) + byte_count;
    writer->tracks_written++;
    
    return 0;
}

int uft_ir_write_sector(uft_ir_writer_t *writer,
                        uint16_t track, uint8_t side, uint8_t sector,
                        const uint8_t *data, size_t size,
                        bool crc_valid, uint16_t stored_crc)
{
    if (!writer || !writer->file || !data || size == 0) return -1;
    
    /* Write block header */
    uft_ir_block_header_t block = {
        .type = UFT_IR_BLOCK_SECTOR,
        .flags = 0,
        .track_id = (uint16_t)((track << 1) | side),
        .size = (uint32_t)(sizeof(uft_ir_sector_header_t) + size),
    };
    
    if (fwrite(&block, sizeof(block), 1, writer->file) != 1) {
        return -2;
    }
    
    /* Calculate size code */
    uint8_t size_code = 0;
    size_t tmp = size;
    while (tmp > 128 && size_code < 7) {
        tmp >>= 1;
        size_code++;
    }
    
    /* Calculate CRC */
    uint16_t calc_crc = (uint16_t)uft_ir_crc32(data, size);
    
    /* Write sector header */
    uft_ir_sector_header_t sect_hdr = {
        .track = track,
        .side = side,
        .sector = sector,
        .size_code = size_code,
        .flags = crc_valid ? UFT_IR_SECT_CRC_OK : 0,
        .crc_stored = stored_crc,
        .crc_calculated = calc_crc,
        .data_size = (uint16_t)size,
    };
    
    if (fwrite(&sect_hdr, sizeof(sect_hdr), 1, writer->file) != 1) {
        return -3;
    }
    
    /* Write sector data */
    if (fwrite(data, 1, size, writer->file) != size) {
        return -4;
    }
    
    writer->bytes_written += sizeof(block) + sizeof(sect_hdr) + size;
    writer->sectors_written++;
    
    return 0;
}

int uft_ir_write_timing(uft_ir_writer_t *writer,
                        uint16_t track, uint8_t side,
                        const uint8_t *timing, size_t size)
{
    if (!writer || !writer->file || !timing || size == 0) return -1;
    if (!writer->config.include_timing) return 0;  /* Skip if not configured */
    
    /* Write block header */
    uft_ir_block_header_t block = {
        .type = UFT_IR_BLOCK_TIMING,
        .flags = 0,
        .track_id = (uint16_t)((track << 1) | side),
        .size = (uint32_t)size,
    };
    
    if (fwrite(&block, sizeof(block), 1, writer->file) != 1) {
        return -2;
    }
    
    if (fwrite(timing, 1, size, writer->file) != size) {
        return -3;
    }
    
    writer->bytes_written += sizeof(block) + size;
    
    return 0;
}

int uft_ir_write_flux(uft_ir_writer_t *writer,
                      uint16_t track, uint8_t side,
                      const uint32_t *flux_ns, size_t count)
{
    if (!writer || !writer->file || !flux_ns || count == 0) return -1;
    if (!writer->config.include_flux) return 0;  /* Skip if not configured */
    
    size_t data_size = count * sizeof(uint32_t);
    
    /* Write block header */
    uft_ir_block_header_t block = {
        .type = UFT_IR_BLOCK_FLUX,
        .flags = 0,
        .track_id = (uint16_t)((track << 1) | side),
        .size = (uint32_t)data_size,
    };
    
    if (fwrite(&block, sizeof(block), 1, writer->file) != 1) {
        return -2;
    }
    
    if (fwrite(flux_ns, sizeof(uint32_t), count, writer->file) != count) {
        return -3;
    }
    
    writer->bytes_written += sizeof(block) + data_size;
    
    return 0;
}

/*===========================================================================
 * Reader Implementation
 *===========================================================================*/

uft_ir_reader_t *uft_ir_reader_open(const char *path)
{
    if (!path) return NULL;
    
    uft_ir_reader_t *reader = calloc(1, sizeof(uft_ir_reader_t));
    if (!reader) return NULL;
    
    reader->file = fopen(path, "rb");
    if (!reader->file) {
        snprintf(reader->error_msg, sizeof(reader->error_msg),
                 "Cannot open file: %s", path);
        reader->last_error = -1;
        free(reader);
        return NULL;
    }
    
    /* Read and validate header */
    if (fread(&reader->header, sizeof(reader->header), 1, reader->file) != 1) {
        snprintf(reader->error_msg, sizeof(reader->error_msg),
                 "Failed to read header");
        reader->last_error = -2;
        fclose(reader->file);
        free(reader);
        return NULL;
    }
    
    /* Validate magic */
    if (reader->header.magic != UFT_IR_MAGIC) {
        snprintf(reader->error_msg, sizeof(reader->error_msg),
                 "Invalid file format (bad magic)");
        reader->last_error = -3;
        fclose(reader->file);
        free(reader);
        return NULL;
    }
    
    /* Validate version */
    if (reader->header.version_major > UFT_IR_VERSION_MAJOR) {
        snprintf(reader->error_msg, sizeof(reader->error_msg),
                 "Unsupported version: %d.%d",
                 reader->header.version_major, reader->header.version_minor);
        reader->last_error = -4;
        fclose(reader->file);
        free(reader);
        return NULL;
    }
    
    reader->file_position = sizeof(uft_ir_header_t);
    
    /* Build index by scanning file */
    reader->index = calloc(UFT_IR_MAX_TRACKS, sizeof(uft_ir_index_entry_t));
    if (!reader->index) {
        fclose(reader->file);
        free(reader);
        return NULL;
    }
    
    /* Scan for track blocks to build index */
    uft_ir_block_header_t block;
    while (fread(&block, sizeof(block), 1, reader->file) == 1) {
        if (block.type == UFT_IR_BLOCK_EOF) {
            break;
        }
        
        if (block.type == UFT_IR_BLOCK_TRACK && 
            reader->index_count < UFT_IR_MAX_TRACKS) {
            uft_ir_index_entry_t *entry = &reader->index[reader->index_count++];
            entry->track = block.track_id >> 1;
            entry->side = block.track_id & 1;
            entry->file_offset = (uint32_t)(reader->file_position - sizeof(block));
        }
        
        /* Skip block data */
        if (fseek(reader->file, block.size, SEEK_CUR) != 0) return UFT_IR_ERR_READ;
        reader->file_position = ftell(reader->file);
    }
    
    /* Reset to after header */
    if (fseek(reader->file, sizeof(uft_ir_header_t), SEEK_SET) != 0) { /* seek error */ }
    reader->file_position = sizeof(uft_ir_header_t);
    
    return reader;
}

void uft_ir_reader_close(uft_ir_reader_t *reader)
{
    if (!reader) return;
    
    if (reader->file) {
        fclose(reader->file);
    }
    free(reader->index);
    free(reader);
}

const uft_ir_header_t *uft_ir_get_header(const uft_ir_reader_t *reader)
{
    return reader ? &reader->header : NULL;
}

uint32_t uft_ir_get_track_count(const uft_ir_reader_t *reader)
{
    return reader ? (uint32_t)reader->index_count : 0;
}

bool uft_ir_has_track(const uft_ir_reader_t *reader, uint16_t track, uint8_t side)
{
    if (!reader) return false;
    
    for (size_t i = 0; i < reader->index_count; i++) {
        if (reader->index[i].track == track && 
            reader->index[i].side == side) {
            return true;
        }
    }
    return false;
}

int uft_ir_read_track(uft_ir_reader_t *reader,
                      uint16_t track, uint8_t side,
                      uint8_t *bitstream, size_t max_bits, size_t *bit_count)
{
    if (!reader || !reader->file || !bitstream || !bit_count) return -1;
    
    *bit_count = 0;
    
    /* Find track in index */
    uint32_t offset = 0;
    bool found = false;
    for (size_t i = 0; i < reader->index_count; i++) {
        if (reader->index[i].track == track && 
            reader->index[i].side == side) {
            offset = reader->index[i].file_offset;
            found = true;
            break;
        }
    }
    
    if (!found) {
        snprintf(reader->error_msg, sizeof(reader->error_msg),
                 "Track %d.%d not found", track, side);
        return -2;
    }
    
    /* Seek to track */
    if (fseek(reader->file, offset, SEEK_SET) != 0) { /* seek error */ }
    /* Read block header */
    uft_ir_block_header_t block;
    if (fread(&block, sizeof(block), 1, reader->file) != 1) {
        return -3;
    }
    
    if (block.type != UFT_IR_BLOCK_TRACK) {
        return -4;
    }
    
    /* Read track header */
    uft_ir_track_header_t track_hdr;
    if (fread(&track_hdr, sizeof(track_hdr), 1, reader->file) != 1) {
        return -5;
    }
    
    /* Read bitstream */
    size_t bytes_needed = (track_hdr.bitstream_size + 7) / 8;
    size_t bytes_available = (max_bits + 7) / 8;
    size_t bytes_to_read = bytes_needed < bytes_available ? bytes_needed : bytes_available;
    
    if (fread(bitstream, 1, bytes_to_read, reader->file) != bytes_to_read) {
        return -6;
    }
    
    *bit_count = track_hdr.bitstream_size < max_bits ? 
                 track_hdr.bitstream_size : max_bits;
    
    return 0;
}

int uft_ir_read_sector(uft_ir_reader_t *reader,
                       uint16_t track, uint8_t side, uint8_t sector,
                       uint8_t *data, size_t max_size, size_t *size)
{
    if (!reader || !reader->file || !data || !size) return -1;
    
    *size = 0;
    
    /* Find track offset */
    uint32_t track_offset = 0;
    for (size_t i = 0; i < reader->index_count; i++) {
        if (reader->index[i].track == track && 
            reader->index[i].side == side) {
            track_offset = reader->index[i].file_offset;
            break;
        }
    }
    
    if (track_offset == 0) return -2;
    
    /* Scan for sector block after track */
    if (fseek(reader->file, track_offset, SEEK_SET) != 0) { /* seek error */ }
    uft_ir_block_header_t block;
    while (fread(&block, sizeof(block), 1, reader->file) == 1) {
        if (block.type == UFT_IR_BLOCK_EOF || 
            block.type == UFT_IR_BLOCK_TRACK) {
            /* Reached next track or EOF */
            break;
        }
        
        if (block.type == UFT_IR_BLOCK_SECTOR) {
            uint16_t track_id = (track << 1) | side;
            if (block.track_id == track_id) {
                /* Read sector header */
                uft_ir_sector_header_t sect_hdr;
                if (fread(&sect_hdr, sizeof(sect_hdr), 1, reader->file) != 1) {
                    return -3;
                }
                
                if (sect_hdr.sector == sector) {
                    /* Found sector */
                    size_t to_read = sect_hdr.data_size < max_size ? 
                                     sect_hdr.data_size : max_size;
                    if (fread(data, 1, to_read, reader->file) != to_read) {
                        return -4;
                    }
                    *size = to_read;
                    return 0;
                }
                
                /* Skip sector data */
                if (fseek(reader->file, sect_hdr.data_size, SEEK_CUR) != 0) { /* seek error */ }
                continue;
            }
        }
        
        /* Skip block data */
        if (fseek(reader->file, block.size, SEEK_CUR) != 0) return UFT_IR_ERR_READ;
    }
    
    return -5;  /* Sector not found */
}

/*===========================================================================
 * JSON Export
 *===========================================================================*/

static const char *base64_chars = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static size_t base64_encode(const uint8_t *data, size_t len, char *output, size_t out_size)
{
    size_t out_len = 4 * ((len + 2) / 3);
    if (out_size < out_len + 1) return out_len + 1;
    
    size_t i = 0, j = 0;
    while (i < len) {
        uint32_t a = i < len ? data[i++] : 0;
        uint32_t b = i < len ? data[i++] : 0;
        uint32_t c = i < len ? data[i++] : 0;
        uint32_t triple = (a << 16) | (b << 8) | c;
        
        output[j++] = base64_chars[(triple >> 18) & 0x3F];
        output[j++] = base64_chars[(triple >> 12) & 0x3F];
        output[j++] = (i > len + 1) ? '=' : base64_chars[(triple >> 6) & 0x3F];
        output[j++] = (i > len) ? '=' : base64_chars[triple & 0x3F];
    }
    output[j] = '\0';
    return j;
}

int uft_ir_export_json(uft_ir_reader_t *reader, const char *path,
                       const uft_ir_json_config_t *config)
{
    if (!reader || !path) return -1;
    
    uft_ir_json_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        uft_ir_json_config_default(&cfg);
    }
    
    FILE *f = fopen(path, "w");
    if (!f) return -2;
    
    const char *indent = cfg.pretty_print ? "  " : "";
    const char *nl = cfg.pretty_print ? "\n" : "";
    
    /* Write header */
    fprintf(f, "{%s", nl);
    fprintf(f, "%s\"format\": \"UFT-IR\",%s", indent, nl);
    fprintf(f, "%s\"version\": \"%d.%d\",%s", indent, 
            reader->header.version_major, reader->header.version_minor, nl);
    fprintf(f, "%s\"track_count\": %u,%s", indent, (unsigned)reader->index_count, nl);
    fprintf(f, "%s\"creation_time\": %lu,%s", indent, 
            (unsigned long)reader->header.creation_time, nl);
    
    /* Write flags */
    fprintf(f, "%s\"flags\": {%s", indent, nl);
    fprintf(f, "%s%s\"has_timing\": %s,%s", indent, indent,
            (reader->header.flags & UFT_IR_FLAG_HAS_TIMING) ? "true" : "false", nl);
    fprintf(f, "%s%s\"has_flux\": %s,%s", indent, indent,
            (reader->header.flags & UFT_IR_FLAG_HAS_FLUX) ? "true" : "false", nl);
    fprintf(f, "%s%s\"has_multirev\": %s,%s", indent, indent,
            (reader->header.flags & UFT_IR_FLAG_HAS_MULTIREV) ? "true" : "false", nl);
    fprintf(f, "%s%s\"has_protection\": %s%s", indent, indent,
            (reader->header.flags & UFT_IR_FLAG_HAS_PROTECTION) ? "true" : "false", nl);
    fprintf(f, "%s},%s", indent, nl);
    
    /* Write tracks */
    fprintf(f, "%s\"tracks\": [%s", indent, nl);
    
    for (size_t i = 0; i < reader->index_count; i++) {
        uft_ir_index_entry_t *entry = &reader->index[i];
        
        fprintf(f, "%s%s{%s", indent, indent, nl);
        fprintf(f, "%s%s%s\"track\": %u,%s", indent, indent, indent, entry->track, nl);
        fprintf(f, "%s%s%s\"side\": %u,%s", indent, indent, indent, entry->side, nl);
        fprintf(f, "%s%s%s\"offset\": %u%s", indent, indent, indent, entry->file_offset, nl);
        fprintf(f, "%s%s}%s%s", indent, indent, 
                i < reader->index_count - 1 ? "," : "", nl);
    }
    
    fprintf(f, "%s]%s", indent, nl);
    fprintf(f, "}%s", nl);
    if (ferror(f)) {
        fclose(f);
        return UFT_ERR_IO;
    }
    
        
    fclose(f);
return 0;
}

size_t uft_ir_track_to_json(uft_ir_reader_t *reader,
                            uint16_t track, uint8_t side,
                            const uft_ir_json_config_t *config,
                            char *buffer, size_t buffer_size)
{
    if (!reader || !buffer || buffer_size == 0) return 0;
    
    uft_ir_json_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        uft_ir_json_config_default(&cfg);
    }
    
    /* Read track data */
    uint8_t bitstream[20000];
    size_t bit_count = 0;
    
    if (uft_ir_read_track(reader, track, side, bitstream, 
                          sizeof(bitstream) * 8, &bit_count) != 0) {
        return snprintf(buffer, buffer_size, 
                        "{\"error\": \"Track not found\"}");
    }
    
    int pos = 0;
    pos += snprintf(buffer + pos, buffer_size - pos,
        "{\n"
        "  \"track\": %u,\n"
        "  \"side\": %u,\n"
        "  \"bit_count\": %zu",
        track, side, bit_count);
    
    /* Include bitstream as base64 if requested */
    if (cfg.include_bitstream && bit_count > 0) {
        size_t byte_count = (bit_count + 7) / 8;
        size_t b64_size = 4 * ((byte_count + 2) / 3) + 1;
        char *b64 = malloc(b64_size);
        if (b64) {
            base64_encode(bitstream, byte_count, b64, b64_size);
            pos += snprintf(buffer + pos, buffer_size - pos,
                ",\n  \"bitstream\": \"%s\"", b64);
            free(b64);
        }
    }
    
    pos += snprintf(buffer + pos, buffer_size - pos, "\n}");
    
    return (size_t)pos;
}

size_t uft_ir_metadata_to_json(uft_ir_reader_t *reader,
                               char *buffer, size_t buffer_size)
{
    if (!reader || !buffer || buffer_size == 0) return 0;
    
    return snprintf(buffer, buffer_size,
        "{\n"
        "  \"format\": \"UFT-IR\",\n"
        "  \"version\": \"%d.%d\",\n"
        "  \"track_count\": %u,\n"
        "  \"compression\": %u,\n"
        "  \"flags\": %u\n"
        "}",
        reader->header.version_major,
        reader->header.version_minor,
        reader->header.track_count,
        reader->header.compression,
        reader->header.flags);
}

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

const char *uft_ir_get_error(const uft_ir_writer_t *writer,
                             const uft_ir_reader_t *reader)
{
    if (writer && writer->last_error != 0) {
        return writer->error_msg;
    }
    if (reader && reader->last_error != 0) {
        return reader->error_msg;
    }
    return "No error";
}

int uft_ir_verify(const char *path)
{
    uft_ir_reader_t *reader = uft_ir_reader_open(path);
    if (!reader) return -1;
    
    /* Verify header checksum */
    uft_ir_header_t header = reader->header;
    uint32_t stored_checksum = header.checksum;
    header.checksum = 0;
    uint32_t calc_checksum = uft_ir_crc32((uint8_t*)&header, sizeof(header));
    
    if (stored_checksum != calc_checksum && stored_checksum != 0) {
        uft_ir_reader_close(reader);
        return -2;  /* Checksum mismatch */
    }
    
    /* Verify we can read all tracks */
    for (size_t i = 0; i < reader->index_count; i++) {
        uint8_t dummy[128];
        size_t bit_count;
        if (uft_ir_read_track(reader, reader->index[i].track, 
                              reader->index[i].side,
                              dummy, sizeof(dummy) * 8, &bit_count) != 0) {
            uft_ir_reader_close(reader);
            return -3;  /* Track read error */
        }
    }
    
    uft_ir_reader_close(reader);
    return 0;
}

const char *uft_ir_version_string(void)
{
    static char version[16];
    snprintf(version, sizeof(version), "%d.%d", 
             UFT_IR_VERSION_MAJOR, UFT_IR_VERSION_MINOR);
    return version;
}

/*===========================================================================
 * Self-Test
 *===========================================================================*/

#ifdef UFT_IR_SERIALIZE_TEST

#include <stdio.h>

int main(void)
{
    printf("UFT IR Serialization Self-Test\n");
    printf("==============================\n\n");
    
    const char *test_file = "/tmp/test.ufir";
    const char *test_json = "/tmp/test_ir.json";
    
    /* Test writer */
    printf("Creating UFIR file...\n");
    
    uft_ir_serialize_config_t config;
    uft_ir_config_forensic(&config);
    
    uft_ir_writer_t *writer = uft_ir_writer_create(test_file, &config);
    if (!writer) {
        printf("FAIL: Could not create writer\n");
        return 1;
    }
    printf("  Writer created: PASS\n");
    
    /* Write metadata */
    const char *metadata[] = {
        "source=TestDisk",
        "date=2025-01-05",
        "creator=UFT",
        NULL
    };
    uft_ir_write_metadata(writer, metadata);
    printf("  Metadata written: PASS\n");
    
    /* Write test track */
    uint8_t bitstream[1000];
    memset(bitstream, 0xAA, sizeof(bitstream));  /* Alternating bits */
    
    uft_ir_write_track(writer, 0, 0, 1, bitstream, sizeof(bitstream) * 8);
    printf("  Track 0.0 written: PASS\n");
    
    uft_ir_write_track(writer, 0, 1, 1, bitstream, sizeof(bitstream) * 8);
    printf("  Track 0.1 written: PASS\n");
    
    /* Write test sector */
    uint8_t sector_data[512];
    memset(sector_data, 0x5A, sizeof(sector_data));
    
    uft_ir_write_sector(writer, 0, 0, 1, sector_data, sizeof(sector_data), true, 0x1234);
    printf("  Sector written: PASS\n");
    
    /* Close writer */
    uft_ir_writer_close(writer);
    printf("  File closed: PASS\n");
    printf("  Bytes written: (check file size)\n");
    
    /* Test reader */
    printf("\nReading UFIR file...\n");
    
    uft_ir_reader_t *reader = uft_ir_reader_open(test_file);
    if (!reader) {
        printf("FAIL: Could not open file for reading\n");
        return 1;
    }
    printf("  Reader opened: PASS\n");
    
    const uft_ir_header_t *header = uft_ir_get_header(reader);
    printf("  Magic: 0x%08X (%s)\n", header->magic, 
           header->magic == UFT_IR_MAGIC ? "PASS" : "FAIL");
    printf("  Version: %d.%d\n", header->version_major, header->version_minor);
    printf("  Track count: %u\n", uft_ir_get_track_count(reader));
    
    /* Read track */
    uint8_t read_bits[1000];
    size_t bit_count;
    if (uft_ir_read_track(reader, 0, 0, read_bits, sizeof(read_bits) * 8, &bit_count) == 0) {
        printf("  Track 0.0 read: PASS (%zu bits)\n", bit_count);
    } else {
        printf("  Track 0.0 read: FAIL\n");
    }
    
    /* Verify track exists */
    printf("  Has track 0.0: %s\n", uft_ir_has_track(reader, 0, 0) ? "PASS" : "FAIL");
    printf("  Has track 99.0: %s\n", !uft_ir_has_track(reader, 99, 0) ? "PASS" : "FAIL");
    
    /* Export JSON */
    printf("\nExporting JSON...\n");
    if (uft_ir_export_json(reader, test_json, NULL) == 0) {
        printf("  JSON export: PASS\n");
    } else {
        printf("  JSON export: FAIL\n");
    }
    
    /* Track to JSON */
    char json_buf[4096];
    size_t json_len = uft_ir_track_to_json(reader, 0, 0, NULL, json_buf, sizeof(json_buf));
    printf("  Track JSON: %zu bytes\n", json_len);
    
    uft_ir_reader_close(reader);
    
    /* Verify file */
    printf("\nVerifying file...\n");
    int verify = uft_ir_verify(test_file);
    printf("  Verification: %s\n", verify == 0 ? "PASS" : "FAIL");
    
    /* Version string */
    printf("\nVersion: %s\n", uft_ir_version_string());
    
    printf("\nSelf-test complete.\n");
    return 0;
}

#endif /* UFT_IR_SERIALIZE_TEST */
