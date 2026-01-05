/**
 * @file uft_ocr.c
 * @brief UFT Disk Label OCR Implementation
 * 
 * C-005: Disk-Label OCR
 * 
 * Note: Full Tesseract integration requires libtesseract.
 * This provides the framework and fallback text extraction.
 */

#include "uft/ocr/uft_ocr.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <regex.h>

/*===========================================================================
 * Internal Structures
 *===========================================================================*/

struct uft_ocr_ctx {
    uft_ocr_config_t config;
    
    /* Engine-specific handles would go here */
    void *engine_handle;
    
    /* Statistics */
    uint32_t total_recognitions;
    double total_time_ms;
};

/*===========================================================================
 * Known Patterns for Metadata Extraction
 *===========================================================================*/

/* Common publisher names (case-insensitive patterns) */
static const char *KNOWN_PUBLISHERS[] = {
    "Accolade", "Activision", "Broderbund", "Cinemaware", "Electronic Arts",
    "Epyx", "Firebird", "Gremlin", "Hewson", "Infocom", "Infogrames",
    "LucasArts", "Lucasfilm", "Mastertronic", "Melbourne House", "Microprose",
    "Ocean", "Origin", "Psygnosis", "Sierra", "SSI", "System 3", "Taito",
    "Thalamus", "U.S. Gold", "Ultimate", "Virgin", NULL
};

/* Year extraction patterns */
static const char *YEAR_PATTERNS[] = {
    "\\b(19[789][0-9])\\b",             /* 1970-1999 */
    "\\b(200[0-9])\\b",                 /* 2000-2009 */
    "\\b(201[0-9])\\b",                 /* 2010-2019 */
    "\\b(202[0-9])\\b",                 /* 2020-2029 */
    "\\(c\\)[[:space:]]*(19[0-9]{2})",  /* (c) 1985 */
    "copyright[[:space:]]*(19[0-9]{2})", /* Copyright 1985 */
    NULL
};

/* Version patterns */
static const char *VERSION_PATTERNS[] = {
    "\\b[Vv]([0-9]+\\.[0-9]+[a-z]?)\\b",    /* V1.0, v2.1a */
    "\\b[Vv]ersion[[:space:]]*([0-9.]+)\\b", /* Version 1.0 */
    "\\b[Rr]evision[[:space:]]*([0-9]+)\\b", /* Revision 2 */
    NULL
};

/*===========================================================================
 * Configuration
 *===========================================================================*/

void uft_ocr_config_init(uft_ocr_config_t *config)
{
    if (!config) return;
    
    memset(config, 0, sizeof(*config));
    config->engine = UFT_OCR_ENGINE_AUTO;
    
    /* Default to English */
    strncpy(config->languages[0], "eng", 7);
    config->language_count = 1;
    
    /* Default preprocessing */
    config->preproc_flags = UFT_PREPROC_DESKEW | UFT_PREPROC_CONTRAST | 
                            UFT_PREPROC_BINARIZE;
    config->target_dpi = UFT_OCR_OPTIMAL_DPI;
    
    /* Recognition settings */
    config->page_seg_mode = 3;  /* Fully automatic page segmentation */
    config->detect_orientation = true;
    config->preserve_interword_spaces = true;
    
    /* Output settings */
    config->extract_metadata = true;
    config->include_positions = false;
    config->include_confidence = true;
}

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

uft_ocr_ctx_t *uft_ocr_create(const uft_ocr_config_t *config)
{
    if (!config) return NULL;
    
    uft_ocr_ctx_t *ctx = calloc(1, sizeof(uft_ocr_ctx_t));
    if (!ctx) return NULL;
    
    memcpy(&ctx->config, config, sizeof(uft_ocr_config_t));
    
    /* Initialize OCR engine */
    /* TODO: Initialize Tesseract or other engine */
    ctx->engine_handle = NULL;
    
    return ctx;
}

void uft_ocr_destroy(uft_ocr_ctx_t *ctx)
{
    if (!ctx) return;
    
    /* Cleanup engine */
    if (ctx->engine_handle) {
        /* TODO: Cleanup Tesseract */
    }
    
    free(ctx);
}

bool uft_ocr_engine_available(uft_ocr_engine_t engine)
{
    switch (engine) {
        case UFT_OCR_ENGINE_AUTO:
            return true;  /* Always have fallback */
            
        case UFT_OCR_ENGINE_TESSERACT:
            /* Check for tesseract binary */
            return (system("tesseract --version > /dev/null 2>&1") == 0);
            
        case UFT_OCR_ENGINE_CUSTOM:
            return true;
            
        default:
            return false;
    }
}

uint32_t uft_ocr_get_available_engines(void)
{
    uint32_t engines = (1 << UFT_OCR_ENGINE_AUTO);
    
    if (uft_ocr_engine_available(UFT_OCR_ENGINE_TESSERACT)) {
        engines |= (1 << UFT_OCR_ENGINE_TESSERACT);
    }
    
    return engines;
}

/*===========================================================================
 * Image Handling
 *===========================================================================*/

int uft_ocr_load_image(const char *path, uft_image_t *image)
{
    if (!path || !image) return -1;
    
    memset(image, 0, sizeof(*image));
    
    /* Detect format from extension */
    const char *ext = strrchr(path, '.');
    if (ext) {
        ext++;
        if (strcasecmp(ext, "png") == 0) image->format = UFT_IMG_PNG;
        else if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0)
            image->format = UFT_IMG_JPEG;
        else if (strcasecmp(ext, "tif") == 0 || strcasecmp(ext, "tiff") == 0)
            image->format = UFT_IMG_TIFF;
        else if (strcasecmp(ext, "bmp") == 0) image->format = UFT_IMG_BMP;
    }
    
    /* Load file */
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    image->size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    if (image->size > UFT_OCR_MAX_IMAGE_SIZE) {
        fclose(f);
        return -1;
    }
    
    image->data = malloc(image->size);
    if (!image->data) {
        fclose(f);
        return -1;
    }
    
    if (fread(image->data, 1, image->size, f) != image->size) { /* I/O error */ }
    fclose(f);
    
    /* TODO: Parse image header for dimensions */
    image->dpi = 300;  /* Assume 300 DPI */
    
    return 0;
}

int uft_ocr_load_image_mem(const uint8_t *data, size_t size,
                           uft_img_format_t format, uft_image_t *image)
{
    if (!data || size == 0 || !image) return -1;
    
    memset(image, 0, sizeof(*image));
    
    image->data = malloc(size);
    if (!image->data) return -1;
    
    memcpy(image->data, data, size);
    image->size = size;
    image->format = format;
    image->dpi = 300;
    
    return 0;
}

void uft_ocr_free_image(uft_image_t *image)
{
    if (!image) return;
    free(image->data);
    memset(image, 0, sizeof(*image));
}

int uft_ocr_preprocess(uft_image_t *image, uint32_t flags)
{
    if (!image || !image->data) return -1;
    
    /* TODO: Implement actual preprocessing with image library */
    /* For now, just mark as preprocessed */
    
    (void)flags;
    return 0;
}

float uft_ocr_detect_skew(const uft_image_t *image)
{
    if (!image) return 0.0f;
    
    /* TODO: Implement skew detection */
    /* Could use Hough transform or projection profile */
    return 0.0f;
}

int uft_ocr_rotate(uft_image_t *image, float angle)
{
    if (!image || !image->data) return -1;
    
    /* TODO: Implement rotation */
    (void)angle;
    return 0;
}

uft_label_type_t uft_ocr_detect_label_type(const uft_image_t *image)
{
    if (!image) return UFT_LABEL_UNKNOWN;
    
    /* Heuristics based on aspect ratio */
    if (image->width > 0 && image->height > 0) {
        float ratio = (float)image->width / image->height;
        
        /* 3.5" floppy labels are roughly square */
        if (ratio > 0.8 && ratio < 1.2) {
            return UFT_LABEL_FLOPPY_35;
        }
        /* 5.25" labels are typically wider */
        if (ratio > 1.5 && ratio < 2.5) {
            return UFT_LABEL_FLOPPY_525;
        }
    }
    
    return UFT_LABEL_UNKNOWN;
}

/*===========================================================================
 * OCR Recognition
 *===========================================================================*/

/**
 * @brief Fallback OCR using external tesseract binary
 */
static int ocr_tesseract_binary(const uft_image_t *image, char *text, size_t max_len)
{
    if (!image || !text || max_len == 0) return -1;
    
    /* Write image to temp file */
    char temp_img[] = "/tmp/uft_ocr_XXXXXX.png";
    int fd = mkstemps(temp_img, 4);
    if (fd < 0) return -1;
    
    FILE *f = fdopen(fd, "wb");
    if (!f) {
        close(fd);
        return -1;
    }
    
    if (fwrite(image->data, 1, image->size, f) != image->size) { /* I/O error */ }
    fclose(f);
    
    /* Run tesseract */
    char temp_out[] = "/tmp/uft_ocr_out_XXXXXX";
    mkstemp(temp_out);
    
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "tesseract \"%s\" \"%s\" -l eng 2>/dev/null",
             temp_img, temp_out);
    
    int ret = system(cmd);
    unlink(temp_img);
    
    if (ret != 0) {
        unlink(temp_out);
        return -1;
    }
    
    /* Read output */
    char out_path[256];
    snprintf(out_path, sizeof(out_path), "%s.txt", temp_out);
    
    f = fopen(out_path, "r");
    if (!f) {
        unlink(temp_out);
        return -1;
    }
    
    size_t read = fread(text, 1, max_len - 1, f);
    text[read] = '\0';
    fclose(f);
    
    unlink(out_path);
    unlink(temp_out);
    
    return 0;
}

int uft_ocr_recognize(uft_ocr_ctx_t *ctx, const uft_image_t *image,
                      uft_ocr_result_t **result)
{
    if (!ctx || !image || !result) return -1;
    
    clock_t start = clock();
    
    /* Allocate result */
    uft_ocr_result_t *res = calloc(1, sizeof(uft_ocr_result_t));
    if (!res) return -1;
    
    res->raw_text = calloc(UFT_OCR_MAX_TEXT, 1);
    if (!res->raw_text) {
        free(res);
        return -1;
    }
    
    /* Preprocess */
    uft_image_t proc_image = *image;
    if (ctx->config.preproc_flags != UFT_PREPROC_NONE) {
        uft_ocr_preprocess(&proc_image, ctx->config.preproc_flags);
    }
    res->preproc_time_ms = (double)(clock() - start) * 1000 / CLOCKS_PER_SEC;
    
    /* Detect label type */
    res->label_type = uft_ocr_detect_label_type(image);
    
    /* Perform OCR */
    clock_t ocr_start = clock();
    int ocr_ret = -1;
    
    switch (ctx->config.engine) {
        case UFT_OCR_ENGINE_TESSERACT:
        case UFT_OCR_ENGINE_AUTO:
            ocr_ret = ocr_tesseract_binary(image, res->raw_text, UFT_OCR_MAX_TEXT);
            break;
            
        case UFT_OCR_ENGINE_CUSTOM:
            if (ctx->config.custom_engine) {
                ocr_ret = ctx->config.custom_engine(image, res->raw_text, UFT_OCR_MAX_TEXT);
            }
            break;
            
        default:
            break;
    }
    
    res->ocr_time_ms = (double)(clock() - ocr_start) * 1000 / CLOCKS_PER_SEC;
    
    if (ocr_ret != 0) {
        uft_ocr_free_result(res);
        return -1;
    }
    
    res->raw_text_len = strlen(res->raw_text);
    
    /* Extract metadata if requested */
    clock_t extract_start = clock();
    if (ctx->config.extract_metadata) {
        uft_ocr_extract_metadata(res->raw_text, res->label_type, &res->metadata);
    }
    res->extract_time_ms = (double)(clock() - extract_start) * 1000 / CLOCKS_PER_SEC;
    
    /* Statistics */
    ctx->total_recognitions++;
    ctx->total_time_ms += res->preproc_time_ms + res->ocr_time_ms + res->extract_time_ms;
    
    *result = res;
    return 0;
}

int uft_ocr_recognize_file(uft_ocr_ctx_t *ctx, const char *path,
                           uft_ocr_result_t **result)
{
    if (!ctx || !path || !result) return -1;
    
    uft_image_t image;
    if (uft_ocr_load_image(path, &image) != 0) {
        return -1;
    }
    
    int ret = uft_ocr_recognize(ctx, &image, result);
    
    uft_ocr_free_image(&image);
    return ret;
}

void uft_ocr_free_result(uft_ocr_result_t *result)
{
    if (!result) return;
    
    free(result->raw_text);
    
    /* Free lines */
    if (result->lines) {
        for (uint16_t i = 0; i < result->line_count; i++) {
            free(result->lines[i].text);
            free(result->lines[i].words);
        }
        free(result->lines);
    }
    
    free(result);
}

int uft_ocr_get_text(uft_ocr_ctx_t *ctx, const uft_image_t *image,
                     char *text, size_t max_len)
{
    if (!ctx || !image || !text || max_len == 0) return -1;
    
    uft_ocr_result_t *result;
    if (uft_ocr_recognize(ctx, image, &result) != 0) {
        return -1;
    }
    
    strncpy(text, result->raw_text, max_len - 1);
    text[max_len - 1] = '\0';
    
    uft_ocr_free_result(result);
    return 0;
}

/*===========================================================================
 * Metadata Extraction
 *===========================================================================*/

/**
 * @brief Match regex pattern and extract first group
 */
static int regex_extract(const char *text, const char *pattern, 
                         char *result, size_t max_len)
{
    regex_t regex;
    regmatch_t matches[2];
    
    if (regcomp(&regex, pattern, REG_EXTENDED | REG_ICASE) != 0) {
        return -1;
    }
    
    if (regexec(&regex, text, 2, matches, 0) == 0) {
        if (matches[1].rm_so >= 0) {
            size_t len = matches[1].rm_eo - matches[1].rm_so;
            if (len > max_len - 1) len = max_len - 1;
            strncpy(result, text + matches[1].rm_so, len);
            result[len] = '\0';
            regfree(&regex);
            return 0;
        }
    }
    
    regfree(&regex);
    return -1;
}

/**
 * @brief Check if text contains string (case-insensitive)
 */
static bool contains_ci(const char *text, const char *search)
{
    if (!text || !search) return false;
    
    size_t text_len = strlen(text);
    size_t search_len = strlen(search);
    
    if (search_len > text_len) return false;
    
    for (size_t i = 0; i <= text_len - search_len; i++) {
        bool match = true;
        for (size_t j = 0; j < search_len && match; j++) {
            if (tolower(text[i + j]) != tolower(search[j])) {
                match = false;
            }
        }
        if (match) return true;
    }
    
    return false;
}

int uft_ocr_extract_metadata(const char *text, uft_label_type_t label_type,
                             uft_label_metadata_t *metadata)
{
    if (!text || !metadata) return -1;
    
    memset(metadata, 0, sizeof(*metadata));
    
    /* Extract year */
    for (int i = 0; YEAR_PATTERNS[i]; i++) {
        if (regex_extract(text, YEAR_PATTERNS[i], 
                          metadata->year, sizeof(metadata->year)) == 0) {
            break;
        }
    }
    
    /* Extract version */
    for (int i = 0; VERSION_PATTERNS[i]; i++) {
        if (regex_extract(text, VERSION_PATTERNS[i],
                          metadata->version, sizeof(metadata->version)) == 0) {
            break;
        }
    }
    
    /* Extract publisher */
    for (int i = 0; KNOWN_PUBLISHERS[i]; i++) {
        if (contains_ci(text, KNOWN_PUBLISHERS[i])) {
            strncpy(metadata->publisher, KNOWN_PUBLISHERS[i],
                    sizeof(metadata->publisher) - 1);
            break;
        }
    }
    
    /* Extract disk number */
    char disk_num[16];
    if (regex_extract(text, "\\b[Dd]isk[[:space:]]*([0-9]+)", 
                      disk_num, sizeof(disk_num)) == 0) {
        strncpy(metadata->disk_number, disk_num, sizeof(metadata->disk_number) - 1);
    }
    
    /* Extract side */
    if (contains_ci(text, "Side A") || contains_ci(text, "Side 1")) {
        strncpy(metadata->side, "A", sizeof(metadata->side) - 1);
    } else if (contains_ci(text, "Side B") || contains_ci(text, "Side 2")) {
        strncpy(metadata->side, "B", sizeof(metadata->side) - 1);
    }
    
    /* Title extraction (first line heuristic) */
    const char *first_newline = strchr(text, '\n');
    if (first_newline) {
        size_t len = first_newline - text;
        if (len > sizeof(metadata->title) - 1) {
            len = sizeof(metadata->title) - 1;
        }
        strncpy(metadata->title, text, len);
        
        /* Clean up title */
        char *p = metadata->title;
        while (*p) {
            if (*p == '\r' || *p == '\t') *p = ' ';
            p++;
        }
        /* Trim trailing spaces */
        p = metadata->title + strlen(metadata->title) - 1;
        while (p >= metadata->title && isspace(*p)) {
            *p-- = '\0';
        }
    }
    
    /* Calculate confidence */
    int fields_found = 0;
    if (metadata->title[0]) fields_found++;
    if (metadata->publisher[0]) fields_found++;
    if (metadata->year[0]) fields_found++;
    if (metadata->version[0]) fields_found++;
    
    metadata->overall_confidence = (float)fields_found / 4.0f;
    metadata->conf_level = uft_ocr_conf_from_value(metadata->overall_confidence);
    
    (void)label_type;  /* Future: use for context-specific extraction */
    
    return 0;
}

int uft_ocr_extract_title(const char *text, char *title, size_t max_len)
{
    if (!text || !title || max_len == 0) return -1;
    
    const char *first_newline = strchr(text, '\n');
    size_t len = first_newline ? (size_t)(first_newline - text) : strlen(text);
    
    if (len > max_len - 1) len = max_len - 1;
    strncpy(title, text, len);
    title[len] = '\0';
    
    return 0;
}

int uft_ocr_extract_year(const char *text, char *year, size_t max_len)
{
    if (!text || !year || max_len == 0) return -1;
    
    for (int i = 0; YEAR_PATTERNS[i]; i++) {
        if (regex_extract(text, YEAR_PATTERNS[i], year, max_len) == 0) {
            return 0;
        }
    }
    
    return -1;
}

int uft_ocr_extract_publisher(const char *text, char *publisher, size_t max_len)
{
    if (!text || !publisher || max_len == 0) return -1;
    
    for (int i = 0; KNOWN_PUBLISHERS[i]; i++) {
        if (contains_ci(text, KNOWN_PUBLISHERS[i])) {
            strncpy(publisher, KNOWN_PUBLISHERS[i], max_len - 1);
            publisher[max_len - 1] = '\0';
            return 0;
        }
    }
    
    return -1;
}

/*===========================================================================
 * Correction
 *===========================================================================*/

int uft_ocr_correct_field(uft_ocr_result_t *result, const char *field_name,
                          const char *value)
{
    if (!result || !field_name || !value) return -1;
    
    if (strcasecmp(field_name, "title") == 0) {
        strncpy(result->metadata.title, value, sizeof(result->metadata.title) - 1);
    } else if (strcasecmp(field_name, "publisher") == 0) {
        strncpy(result->metadata.publisher, value, sizeof(result->metadata.publisher) - 1);
    } else if (strcasecmp(field_name, "year") == 0) {
        strncpy(result->metadata.year, value, sizeof(result->metadata.year) - 1);
    } else if (strcasecmp(field_name, "version") == 0) {
        strncpy(result->metadata.version, value, sizeof(result->metadata.version) - 1);
    } else {
        /* Add to generic fields */
        if (result->metadata.field_count < UFT_OCR_MAX_FIELDS) {
            uft_ocr_field_t *field = &result->metadata.fields[result->metadata.field_count++];
            strncpy(field->name, field_name, sizeof(field->name) - 1);
            strncpy(field->value, value, sizeof(field->value) - 1);
            field->confidence = 1.0f;
            field->verified = true;
        } else {
            return -1;
        }
    }
    
    return 0;
}

int uft_ocr_verify_field(uft_ocr_result_t *result, const char *field_name)
{
    if (!result || !field_name) return -1;
    
    for (uint8_t i = 0; i < result->metadata.field_count; i++) {
        if (strcasecmp(result->metadata.fields[i].name, field_name) == 0) {
            result->metadata.fields[i].verified = true;
            return 0;
        }
    }
    
    return -1;
}

int uft_ocr_spell_correct(char *text, size_t max_len, const char *language)
{
    /* TODO: Implement spell correction */
    /* Could use aspell, hunspell, or custom dictionary */
    (void)text;
    (void)max_len;
    (void)language;
    return 0;
}

/*===========================================================================
 * Export
 *===========================================================================*/

int uft_ocr_export_json(const uft_ocr_result_t *result, const char *path)
{
    if (!result || !path) return -1;
    
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    
    fprintf(f, "{\n");
    fprintf(f, "  \"metadata\": {\n");
    fprintf(f, "    \"title\": \"%s\",\n", result->metadata.title);
    fprintf(f, "    \"publisher\": \"%s\",\n", result->metadata.publisher);
    fprintf(f, "    \"year\": \"%s\",\n", result->metadata.year);
    fprintf(f, "    \"version\": \"%s\",\n", result->metadata.version);
    fprintf(f, "    \"serial\": \"%s\",\n", result->metadata.serial);
    fprintf(f, "    \"platform\": \"%s\",\n", result->metadata.platform);
    fprintf(f, "    \"disk_number\": \"%s\",\n", result->metadata.disk_number);
    fprintf(f, "    \"side\": \"%s\",\n", result->metadata.side);
    fprintf(f, "    \"confidence\": %.4f,\n", result->metadata.overall_confidence);
    fprintf(f, "    \"confidence_level\": \"%s\"\n", 
            uft_ocr_conf_name(result->metadata.conf_level));
    fprintf(f, "  },\n");
    
    fprintf(f, "  \"statistics\": {\n");
    fprintf(f, "    \"total_words\": %u,\n", result->total_words);
    fprintf(f, "    \"mean_confidence\": %.4f,\n", result->mean_confidence);
    fprintf(f, "    \"preproc_time_ms\": %.2f,\n", result->preproc_time_ms);
    fprintf(f, "    \"ocr_time_ms\": %.2f,\n", result->ocr_time_ms);
    fprintf(f, "    \"extract_time_ms\": %.2f\n", result->extract_time_ms);
    fprintf(f, "  },\n");
    
    /* Escape raw text for JSON */
    fprintf(f, "  \"raw_text\": \"");
    if (result->raw_text) {
        for (const char *p = result->raw_text; *p; p++) {
            switch (*p) {
                case '"':  fprintf(f, "\\\""); break;
                case '\\': fprintf(f, "\\\\"); break;
                case '\n': fprintf(f, "\\n"); break;
                case '\r': fprintf(f, "\\r"); break;
                case '\t': fprintf(f, "\\t"); break;
                default:
                    if (*p >= 32 && *p < 127) {
                        fputc(*p, f);
                    } else {
                        fprintf(f, "\\u%04x", (unsigned char)*p);
                    }
            }
        }
    }
    fprintf(f, "\"\n");
    
    fprintf(f, "}\n");
    
    fclose(f);
    return 0;
}

int uft_ocr_export_nfo(const uft_label_metadata_t *metadata, const char *path)
{
    if (!metadata || !path) return -1;
    
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    
    fprintf(f, "╔══════════════════════════════════════════════════════════════╗\n");
    fprintf(f, "║                                                              ║\n");
    fprintf(f, "║  %-58s  ║\n", metadata->title);
    fprintf(f, "║                                                              ║\n");
    fprintf(f, "╠══════════════════════════════════════════════════════════════╣\n");
    
    if (metadata->publisher[0]) {
        fprintf(f, "║  Publisher: %-48s  ║\n", metadata->publisher);
    }
    if (metadata->year[0]) {
        fprintf(f, "║  Year:      %-48s  ║\n", metadata->year);
    }
    if (metadata->version[0]) {
        fprintf(f, "║  Version:   %-48s  ║\n", metadata->version);
    }
    if (metadata->platform[0]) {
        fprintf(f, "║  Platform:  %-48s  ║\n", metadata->platform);
    }
    
    fprintf(f, "╠══════════════════════════════════════════════════════════════╣\n");
    fprintf(f, "║  Generated by UFT Disk Label OCR                             ║\n");
    fprintf(f, "╚══════════════════════════════════════════════════════════════╝\n");
    
    fclose(f);
    return 0;
}

int uft_ocr_export_hocr(const uft_ocr_result_t *result, const char *path)
{
    if (!result || !path) return -1;
    
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    
    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(f, "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\"\n");
    fprintf(f, "    \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n");
    fprintf(f, "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n");
    fprintf(f, "<head>\n");
    fprintf(f, "  <meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n");
    fprintf(f, "  <title>%s</title>\n", result->metadata.title);
    fprintf(f, "</head>\n");
    fprintf(f, "<body>\n");
    fprintf(f, "  <div class=\"ocr_page\">\n");
    
    /* Output as single paragraph for simple case */
    fprintf(f, "    <p class=\"ocr_par\">\n");
    if (result->raw_text) {
        /* HTML escape */
        for (const char *p = result->raw_text; *p; p++) {
            switch (*p) {
                case '<':  fprintf(f, "&lt;"); break;
                case '>':  fprintf(f, "&gt;"); break;
                case '&':  fprintf(f, "&amp;"); break;
                case '\n': fprintf(f, "<br/>\n"); break;
                default:   fputc(*p, f); break;
            }
        }
    }
    fprintf(f, "    </p>\n");
    
    fprintf(f, "  </div>\n");
    fprintf(f, "</body>\n");
    fprintf(f, "</html>\n");
    
    fclose(f);
    return 0;
}

int uft_ocr_export_pdf(const uft_image_t *image, const uft_ocr_result_t *result,
                       const char *path)
{
    /* TODO: Implement PDF generation */
    /* Would require a PDF library like libharu, poppler, or calling ghostscript */
    (void)image;
    (void)result;
    (void)path;
    return -1;
}

/*===========================================================================
 * Utilities
 *===========================================================================*/

const char *uft_ocr_engine_name(uft_ocr_engine_t engine)
{
    switch (engine) {
        case UFT_OCR_ENGINE_AUTO:      return "Auto";
        case UFT_OCR_ENGINE_TESSERACT: return "Tesseract";
        case UFT_OCR_ENGINE_CUNEIFORM: return "Cuneiform";
        case UFT_OCR_ENGINE_GOCR:      return "GOCR";
        case UFT_OCR_ENGINE_CUSTOM:    return "Custom";
        default:                       return "Unknown";
    }
}

const char *uft_img_format_name(uft_img_format_t format)
{
    switch (format) {
        case UFT_IMG_PNG:     return "PNG";
        case UFT_IMG_JPEG:    return "JPEG";
        case UFT_IMG_TIFF:    return "TIFF";
        case UFT_IMG_BMP:     return "BMP";
        case UFT_IMG_RAW:     return "RAW";
        default:              return "Unknown";
    }
}

const char *uft_label_type_name(uft_label_type_t type)
{
    switch (type) {
        case UFT_LABEL_FLOPPY_525: return "5.25\" Floppy";
        case UFT_LABEL_FLOPPY_35:  return "3.5\" Floppy";
        case UFT_LABEL_SLEEVE:     return "Sleeve";
        case UFT_LABEL_MANUAL:     return "Manual";
        case UFT_LABEL_BOX:        return "Box";
        case UFT_LABEL_DISK_SCAN:  return "Disk Scan";
        default:                   return "Unknown";
    }
}

const char *uft_ocr_conf_name(uft_ocr_conf_level_t level)
{
    switch (level) {
        case UFT_OCR_CONF_LOW:      return "Low";
        case UFT_OCR_CONF_MEDIUM:   return "Medium";
        case UFT_OCR_CONF_HIGH:     return "High";
        case UFT_OCR_CONF_VERIFIED: return "Verified";
        default:                    return "Unknown";
    }
}

uft_ocr_conf_level_t uft_ocr_conf_from_value(float confidence)
{
    if (confidence >= 0.95f) return UFT_OCR_CONF_VERIFIED;
    if (confidence >= 0.80f) return UFT_OCR_CONF_HIGH;
    if (confidence >= 0.60f) return UFT_OCR_CONF_MEDIUM;
    if (confidence > 0.0f)   return UFT_OCR_CONF_LOW;
    return UFT_OCR_CONF_UNKNOWN;
}
