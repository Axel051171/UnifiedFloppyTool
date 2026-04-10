/**
 * @file uft_provenance.c
 * @brief Cryptographic Provenance Chain for forensic chain-of-custody tracking
 * @version 1.0.0
 *
 * Implements a tamper-evident chain of custody for disk imaging operations.
 * Each entry is cryptographically linked to its predecessor via SHA-256,
 * ensuring any modification to the history is detectable.
 *
 * Uses the same FIPS 180-4 SHA-256 implementation as uft_forensic_report.c.
 */

#include "uft/forensic/uft_provenance.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * SHA-256 Implementation (FIPS 180-4)
 * Same algorithm as in uft_forensic_report.c — duplicated here to keep
 * the provenance module self-contained with no cross-file dependencies.
 * ═══════════════════════════════════════════════════════════════════════════ */

static const uint32_t sha256_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

#define SHA_ROR(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define SHA_CH(x,y,z) (((x)&(y))^(~(x)&(z)))
#define SHA_MAJ(x,y,z) (((x)&(y))^((x)&(z))^((y)&(z)))
#define SHA_EP0(x) (SHA_ROR(x,2)^SHA_ROR(x,13)^SHA_ROR(x,22))
#define SHA_EP1(x) (SHA_ROR(x,6)^SHA_ROR(x,11)^SHA_ROR(x,25))
#define SHA_SIG0(x) (SHA_ROR(x,7)^SHA_ROR(x,18)^((x)>>3))
#define SHA_SIG1(x) (SHA_ROR(x,17)^SHA_ROR(x,19)^((x)>>10))

/**
 * Compute SHA-256 hash of data, output as 32 raw bytes.
 */
static void prov_sha256_raw(const uint8_t *data, size_t size, uint8_t *out) {
    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    /* Pad message: append 0x80, zeros, then 64-bit length in bits (big-endian) */
    size_t padded_len = ((size + 9 + 63) / 64) * 64;
    uint8_t *padded = (uint8_t *)calloc(1, padded_len);
    if (!padded) { memset(out, 0, 32); return; }
    memcpy(padded, data, size);
    padded[size] = 0x80;
    uint64_t bit_len = (uint64_t)size * 8;
    for (int i = 0; i < 8; i++)
        padded[padded_len - 1 - i] = (uint8_t)(bit_len >> (i * 8));

    /* Process 64-byte blocks */
    for (size_t block = 0; block < padded_len; block += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++)
            w[i] = ((uint32_t)padded[block+i*4]<<24) | ((uint32_t)padded[block+i*4+1]<<16) |
                    ((uint32_t)padded[block+i*4+2]<<8) | padded[block+i*4+3];
        for (int i = 16; i < 64; i++)
            w[i] = SHA_SIG1(w[i-2]) + w[i-7] + SHA_SIG0(w[i-15]) + w[i-16];

        uint32_t a=h[0], b=h[1], c=h[2], d=h[3], e=h[4], f=h[5], g=h[6], hh=h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t t1 = hh + SHA_EP1(e) + SHA_CH(e,f,g) + sha256_k[i] + w[i];
            uint32_t t2 = SHA_EP0(a) + SHA_MAJ(a,b,c);
            hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }
    free(padded);

    /* Output as raw bytes (big-endian) */
    for (int i = 0; i < 8; i++) {
        out[i*4+0] = (uint8_t)(h[i] >> 24);
        out[i*4+1] = (uint8_t)(h[i] >> 16);
        out[i*4+2] = (uint8_t)(h[i] >> 8);
        out[i*4+3] = (uint8_t)(h[i]);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Hex Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Convert 32 raw bytes to 64-char hex string (+ null terminator).
 */
static void hash_to_hex(const uint8_t *hash, char *hex_out) {
    for (int i = 0; i < UFT_PROV_HASH_SIZE; i++) {
        snprintf(hex_out + i * 2, 3, "%02x", hash[i]);
    }
    hex_out[UFT_PROV_HASH_SIZE * 2] = '\0';
}

/**
 * Convert 64-char hex string to 32 raw bytes.
 * Returns false on invalid input.
 */
static bool hex_to_hash(const char *hex, uint8_t *hash_out) {
    if (!hex) return false;
    size_t len = strlen(hex);
    if (len != UFT_PROV_HASH_SIZE * 2) return false;

    for (int i = 0; i < UFT_PROV_HASH_SIZE; i++) {
        unsigned int byte_val;
        if (sscanf(hex + i * 2, "%02x", &byte_val) != 1)
            return false;
        hash_out[i] = (uint8_t)byte_val;
    }
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Entry Serialization (for chain hashing)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Serialize the hashable fields of an entry into a byte buffer.
 * This is what gets fed into SHA-256 for chain linking.
 * Returns the number of bytes written.
 */
static size_t serialize_entry(const uft_prov_entry_t *entry, uint8_t *buf, size_t buf_size) {
    size_t offset = 0;

    /* action (4 bytes, little-endian) */
    if (offset + 4 > buf_size) return 0;
    uint32_t action = (uint32_t)entry->action;
    buf[offset++] = (uint8_t)(action);
    buf[offset++] = (uint8_t)(action >> 8);
    buf[offset++] = (uint8_t)(action >> 16);
    buf[offset++] = (uint8_t)(action >> 24);

    /* timestamp (8 bytes, little-endian) */
    if (offset + 8 > buf_size) return 0;
    uint64_t ts = (uint64_t)entry->timestamp;
    for (int i = 0; i < 8; i++)
        buf[offset++] = (uint8_t)(ts >> (i * 8));

    /* input_hash (32 bytes) */
    if (offset + UFT_PROV_HASH_SIZE > buf_size) return 0;
    memcpy(buf + offset, entry->input_hash, UFT_PROV_HASH_SIZE);
    offset += UFT_PROV_HASH_SIZE;

    /* output_hash (32 bytes) */
    if (offset + UFT_PROV_HASH_SIZE > buf_size) return 0;
    memcpy(buf + offset, entry->output_hash, UFT_PROV_HASH_SIZE);
    offset += UFT_PROV_HASH_SIZE;

    /* tool_version (32 bytes) */
    if (offset + 32 > buf_size) return 0;
    memcpy(buf + offset, entry->tool_version, 32);
    offset += 32;

    /* description (128 bytes) */
    if (offset + 128 > buf_size) return 0;
    memcpy(buf + offset, entry->description, 128);
    offset += 128;

    /* operator_id (64 bytes) */
    if (offset + 64 > buf_size) return 0;
    memcpy(buf + offset, entry->operator_id, 64);
    offset += 64;

    /* controller (32 bytes) */
    if (offset + 32 > buf_size) return 0;
    memcpy(buf + offset, entry->controller, 32);
    offset += 32;

    /* data_size (4 bytes, little-endian) */
    if (offset + 4 > buf_size) return 0;
    buf[offset++] = (uint8_t)(entry->data_size);
    buf[offset++] = (uint8_t)(entry->data_size >> 8);
    buf[offset++] = (uint8_t)(entry->data_size >> 16);
    buf[offset++] = (uint8_t)(entry->data_size >> 24);

    return offset;
}

/* Maximum serialized entry size:
 * 4 + 8 + 32 + 32 + 32 + 128 + 64 + 32 + 4 = 336 bytes */
#define ENTRY_SERIAL_MAX 336

/* ═══════════════════════════════════════════════════════════════════════════
 * Action Name Table
 * ═══════════════════════════════════════════════════════════════════════════ */

static const char *action_names[] = {
    "CAPTURE",
    "DECODE",
    "ANALYZE",
    "CONVERT",
    "EXPORT",
    "MODIFY",
    "VERIFY",
    "IMPORT"
};

#define ACTION_COUNT (sizeof(action_names) / sizeof(action_names[0]))

const char *uft_prov_action_name(uft_prov_action_t action) {
    if ((unsigned)action < ACTION_COUNT)
        return action_names[(unsigned)action];
    return "UNKNOWN";
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Provenance Chain API
 * ═══════════════════════════════════════════════════════════════════════════ */

uft_provenance_chain_t *uft_prov_create(void) {
    uft_provenance_chain_t *chain = (uft_provenance_chain_t *)calloc(1, sizeof(uft_provenance_chain_t));
    if (!chain) return NULL;

    chain->count = 0;
    chain->valid = true;
    memset(chain->root_hash, 0, UFT_PROV_HASH_SIZE);
    chain->media_fingerprint[0] = '\0';

    return chain;
}

void uft_prov_free(uft_provenance_chain_t *chain) {
    free(chain);
}

int uft_prov_add(uft_provenance_chain_t *chain,
                  uft_prov_action_t action,
                  const uint8_t *data, size_t data_size,
                  const char *description,
                  const char *operator_id)
{
    if (!chain) return -1;
    if (chain->count >= UFT_PROV_MAX_ENTRIES) return -2;

    uft_prov_entry_t *entry = &chain->entries[chain->count];
    memset(entry, 0, sizeof(uft_prov_entry_t));

    /* Set basic fields */
    entry->action = action;
    entry->timestamp = time(NULL);
    entry->data_size = (uint32_t)(data_size > 0xFFFFFFFF ? 0xFFFFFFFF : data_size);
    snprintf(entry->tool_version, sizeof(entry->tool_version), "UFT 4.1.0");

    if (description)
        snprintf(entry->description, sizeof(entry->description), "%s", description);
    if (operator_id)
        snprintf(entry->operator_id, sizeof(entry->operator_id), "%s", operator_id);

    /* Step 1: If this is not the first entry, the previous entry's output_hash
     * becomes this entry's input_hash */
    if (chain->count > 0) {
        memcpy(entry->input_hash,
               chain->entries[chain->count - 1].output_hash,
               UFT_PROV_HASH_SIZE);
    }
    /* For first entry, input_hash stays all zeros (no predecessor) */

    /* Step 2: Compute SHA-256 of the data -> output_hash */
    if (data && data_size > 0) {
        prov_sha256_raw(data, data_size, entry->output_hash);
    }
    /* If no data, output_hash stays all zeros */

    /* Step 3: Compute chain_hash to link this entry to the previous one */
    {
        uint8_t entry_bytes[ENTRY_SERIAL_MAX];
        size_t entry_len = serialize_entry(entry, entry_bytes, sizeof(entry_bytes));

        if (chain->count == 0) {
            /* First entry: chain_hash = SHA-256(entry_bytes) */
            prov_sha256_raw(entry_bytes, entry_len, entry->chain_hash);
        } else {
            /* Subsequent: chain_hash = SHA-256(prev_chain_hash || entry_bytes) */
            size_t combined_len = UFT_PROV_HASH_SIZE + entry_len;
            uint8_t *combined = (uint8_t *)malloc(combined_len);
            if (!combined) return -3;

            memcpy(combined,
                   chain->entries[chain->count - 1].chain_hash,
                   UFT_PROV_HASH_SIZE);
            memcpy(combined + UFT_PROV_HASH_SIZE, entry_bytes, entry_len);

            prov_sha256_raw(combined, combined_len, entry->chain_hash);
            free(combined);
        }
    }

    /* Step 4: Update root hash on first entry */
    if (chain->count == 0) {
        memcpy(chain->root_hash, entry->chain_hash, UFT_PROV_HASH_SIZE);
    }

    /* Step 5: Increment count */
    chain->count++;
    chain->valid = true;

    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Chain Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

bool uft_prov_verify(const uft_provenance_chain_t *chain) {
    if (!chain) return false;
    if (chain->count == 0) return true; /* Empty chain is trivially valid */

    for (uint32_t i = 0; i < chain->count; i++) {
        const uft_prov_entry_t *entry = &chain->entries[i];
        uint8_t computed_hash[UFT_PROV_HASH_SIZE];

        /* Serialize the entry (without chain_hash, which is computed from the rest) */
        uint8_t entry_bytes[ENTRY_SERIAL_MAX];
        size_t entry_len = serialize_entry(entry, entry_bytes, sizeof(entry_bytes));

        if (i == 0) {
            /* First entry: chain_hash = SHA-256(entry_bytes) */
            prov_sha256_raw(entry_bytes, entry_len, computed_hash);
        } else {
            /* Subsequent: chain_hash = SHA-256(prev_chain_hash || entry_bytes) */
            size_t combined_len = UFT_PROV_HASH_SIZE + entry_len;
            uint8_t *combined = (uint8_t *)malloc(combined_len);
            if (!combined) return false;

            memcpy(combined,
                   chain->entries[i - 1].chain_hash,
                   UFT_PROV_HASH_SIZE);
            memcpy(combined + UFT_PROV_HASH_SIZE, entry_bytes, entry_len);

            prov_sha256_raw(combined, combined_len, computed_hash);
            free(combined);
        }

        /* Compare computed chain_hash with stored one */
        if (memcmp(computed_hash, entry->chain_hash, UFT_PROV_HASH_SIZE) != 0)
            return false;
    }

    /* Verify root_hash matches first entry's chain_hash */
    if (memcmp(chain->root_hash, chain->entries[0].chain_hash, UFT_PROV_HASH_SIZE) != 0)
        return false;

    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * JSON Export
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Write a JSON-escaped string. Escapes backslash, double-quote, and control chars.
 */
static void write_json_string(FILE *fp, const char *s) {
    fputc('"', fp);
    if (s) {
        for (; *s; s++) {
            switch (*s) {
                case '"':  fputs("\\\"", fp); break;
                case '\\': fputs("\\\\", fp); break;
                case '\n': fputs("\\n", fp);  break;
                case '\r': fputs("\\r", fp);  break;
                case '\t': fputs("\\t", fp);  break;
                default:
                    if ((unsigned char)*s < 0x20)
                        fprintf(fp, "\\u%04x", (unsigned char)*s);
                    else
                        fputc(*s, fp);
                    break;
            }
        }
    }
    fputc('"', fp);
}

/**
 * Format a time_t as ISO 8601 UTC string.
 */
static void format_iso8601(time_t t, char *buf, size_t buf_size) {
    struct tm *utc = gmtime(&t);
    if (utc) {
        strftime(buf, buf_size, "%Y-%m-%dT%H:%M:%SZ", utc);
    } else {
        snprintf(buf, buf_size, "1970-01-01T00:00:00Z");
    }
}

int uft_prov_export_json(const uft_provenance_chain_t *chain,
                          const char *output_path)
{
    if (!chain || !output_path) return -1;

    FILE *fp = fopen(output_path, "w");
    if (!fp) return -2;

    char hex_buf[65];

    fprintf(fp, "{\n");
    fprintf(fp, "  \"provenance_chain\": {\n");
    fprintf(fp, "    \"version\": \"1.0\",\n");

    hash_to_hex(chain->root_hash, hex_buf);
    fprintf(fp, "    \"root_hash\": \"%s\",\n", hex_buf);

    fprintf(fp, "    \"media_fingerprint\": ");
    write_json_string(fp, chain->media_fingerprint);
    fprintf(fp, ",\n");

    fprintf(fp, "    \"entry_count\": %u,\n", chain->count);
    fprintf(fp, "    \"valid\": %s,\n", chain->valid ? "true" : "false");

    fprintf(fp, "    \"entries\": [\n");

    for (uint32_t i = 0; i < chain->count; i++) {
        const uft_prov_entry_t *entry = &chain->entries[i];
        char time_buf[32];

        fprintf(fp, "      {\n");

        fprintf(fp, "        \"action\": \"%s\",\n", uft_prov_action_name(entry->action));

        format_iso8601(entry->timestamp, time_buf, sizeof(time_buf));
        fprintf(fp, "        \"timestamp\": \"%s\",\n", time_buf);

        hash_to_hex(entry->input_hash, hex_buf);
        fprintf(fp, "        \"input_hash\": \"%s\",\n", hex_buf);

        hash_to_hex(entry->output_hash, hex_buf);
        fprintf(fp, "        \"output_hash\": \"%s\",\n", hex_buf);

        hash_to_hex(entry->chain_hash, hex_buf);
        fprintf(fp, "        \"chain_hash\": \"%s\",\n", hex_buf);

        fprintf(fp, "        \"tool_version\": ");
        write_json_string(fp, entry->tool_version);
        fprintf(fp, ",\n");

        fprintf(fp, "        \"description\": ");
        write_json_string(fp, entry->description);
        fprintf(fp, ",\n");

        fprintf(fp, "        \"operator\": ");
        write_json_string(fp, entry->operator_id);
        fprintf(fp, ",\n");

        fprintf(fp, "        \"controller\": ");
        write_json_string(fp, entry->controller);
        fprintf(fp, ",\n");

        fprintf(fp, "        \"data_size\": %u\n", entry->data_size);

        if (i < chain->count - 1)
            fprintf(fp, "      },\n");
        else
            fprintf(fp, "      }\n");
    }

    fprintf(fp, "    ]\n");
    fprintf(fp, "  }\n");
    fprintf(fp, "}\n");

    fclose(fp);
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * JSON Import
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Minimal JSON parser helpers.
 * This is a simple line-oriented parser that extracts known fields.
 * For a forensic tool, this avoids external JSON library dependencies.
 */

/** Skip whitespace and return pointer to first non-space char */
static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

/** Extract a JSON string value after the given key.
 *  Looks for "key": "value" and copies value into dst. */
static bool extract_json_string(const char *line, const char *key, char *dst, size_t dst_size) {
    const char *p = strstr(line, key);
    if (!p) return false;
    p += strlen(key);
    p = skip_ws(p);
    if (*p != ':') return false;
    p++;
    p = skip_ws(p);
    if (*p != '"') return false;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i < dst_size - 1) {
        if (*p == '\\' && *(p+1)) {
            p++;
            switch (*p) {
                case '"':  dst[i++] = '"'; break;
                case '\\': dst[i++] = '\\'; break;
                case 'n':  dst[i++] = '\n'; break;
                case 'r':  dst[i++] = '\r'; break;
                case 't':  dst[i++] = '\t'; break;
                default:   dst[i++] = *p; break;
            }
        } else {
            dst[i++] = *p;
        }
        p++;
    }
    dst[i] = '\0';
    return true;
}

/** Extract unsigned integer value for given key */
static bool extract_json_uint(const char *line, const char *key, uint32_t *val) {
    const char *p = strstr(line, key);
    if (!p) return false;
    p += strlen(key);
    p = skip_ws(p);
    if (*p != ':') return false;
    p++;
    p = skip_ws(p);
    *val = (uint32_t)strtoul(p, NULL, 10);
    return true;
}

/** Parse ISO 8601 timestamp (YYYY-MM-DDTHH:MM:SSZ) to time_t */
static time_t parse_iso8601(const char *s) {
    struct tm tm_val;
    memset(&tm_val, 0, sizeof(tm_val));

    if (sscanf(s, "%d-%d-%dT%d:%d:%d",
               &tm_val.tm_year, &tm_val.tm_mon, &tm_val.tm_mday,
               &tm_val.tm_hour, &tm_val.tm_min, &tm_val.tm_sec) != 6)
        return 0;

    tm_val.tm_year -= 1900;
    tm_val.tm_mon -= 1;

#ifdef _WIN32
    return _mkgmtime(&tm_val);
#else
    return timegm(&tm_val);
#endif
}

/** Map action name string to enum */
static uft_prov_action_t parse_action_name(const char *name) {
    for (unsigned i = 0; i < ACTION_COUNT; i++) {
        if (strcmp(name, action_names[i]) == 0)
            return (uft_prov_action_t)i;
    }
    return UFT_PROV_CAPTURE; /* fallback */
}

uft_provenance_chain_t *uft_prov_import_json(const char *path) {
    if (!path) return NULL;

    FILE *fp = fopen(path, "r");
    if (!fp) return NULL;

    /* Read entire file */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    if (file_size <= 0 || file_size > 10 * 1024 * 1024) { /* 10 MB limit */
        fclose(fp);
        return NULL;
    }
    fseek(fp, 0, SEEK_SET);

    char *json = (char *)malloc((size_t)file_size + 1);
    if (!json) { fclose(fp); return NULL; }

    size_t read_len = fread(json, 1, (size_t)file_size, fp);
    fclose(fp);
    json[read_len] = '\0';

    uft_provenance_chain_t *chain = uft_prov_create();
    if (!chain) { free(json); return NULL; }

    /* Parse root-level fields */
    char temp[128];
    if (extract_json_string(json, "\"root_hash\"", temp, sizeof(temp)))
        hex_to_hash(temp, chain->root_hash);
    if (extract_json_string(json, "\"media_fingerprint\"", temp, sizeof(temp)))
        snprintf(chain->media_fingerprint, sizeof(chain->media_fingerprint), "%s", temp);

    /* Parse entries — find each entry block delimited by { } within "entries" */
    const char *entries_start = strstr(json, "\"entries\"");
    if (!entries_start) { free(json); return chain; }

    /* Find the opening [ */
    const char *arr_start = strchr(entries_start, '[');
    if (!arr_start) { free(json); return chain; }

    const char *p = arr_start + 1;
    while (*p && chain->count < UFT_PROV_MAX_ENTRIES) {
        /* Find next { */
        while (*p && *p != '{') {
            if (*p == ']') goto done_parsing;
            p++;
        }
        if (!*p) break;

        /* Find matching } */
        const char *entry_start = p;
        int depth = 1;
        p++;
        while (*p && depth > 0) {
            if (*p == '{') depth++;
            else if (*p == '}') depth--;
            p++;
        }
        if (depth != 0) break;

        /* p now points past the closing } */
        size_t entry_block_len = (size_t)(p - entry_start);
        char *entry_block = (char *)malloc(entry_block_len + 1);
        if (!entry_block) break;
        memcpy(entry_block, entry_start, entry_block_len);
        entry_block[entry_block_len] = '\0';

        uft_prov_entry_t *entry = &chain->entries[chain->count];
        memset(entry, 0, sizeof(uft_prov_entry_t));

        /* Parse entry fields */
        if (extract_json_string(entry_block, "\"action\"", temp, sizeof(temp)))
            entry->action = parse_action_name(temp);

        if (extract_json_string(entry_block, "\"timestamp\"", temp, sizeof(temp)))
            entry->timestamp = parse_iso8601(temp);

        if (extract_json_string(entry_block, "\"input_hash\"", temp, sizeof(temp)))
            hex_to_hash(temp, entry->input_hash);

        if (extract_json_string(entry_block, "\"output_hash\"", temp, sizeof(temp)))
            hex_to_hash(temp, entry->output_hash);

        if (extract_json_string(entry_block, "\"chain_hash\"", temp, sizeof(temp)))
            hex_to_hash(temp, entry->chain_hash);

        extract_json_string(entry_block, "\"tool_version\"", entry->tool_version,
                           sizeof(entry->tool_version));

        extract_json_string(entry_block, "\"description\"", entry->description,
                           sizeof(entry->description));

        extract_json_string(entry_block, "\"operator\"", entry->operator_id,
                           sizeof(entry->operator_id));

        extract_json_string(entry_block, "\"controller\"", entry->controller,
                           sizeof(entry->controller));

        extract_json_uint(entry_block, "\"data_size\"", &entry->data_size);

        chain->count++;
        free(entry_block);
    }

done_parsing:
    free(json);

    /* Verify the imported chain */
    chain->valid = uft_prov_verify(chain);

    return chain;
}
