/**
 * @file uft_css_parser_v3.c
 * @brief GOD MODE CSS Parser v3 - Cascading Style Sheets
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t rule_count;
    uint32_t import_count;
    uint32_t media_query_count;
    bool has_variables;
    bool has_keyframes;
    size_t source_size;
    bool valid;
} css_file_t;

static bool css_parse(const uint8_t* data, size_t size, css_file_t* css) {
    if (!data || !css || size < 1) return false;
    memset(css, 0, sizeof(css_file_t));
    css->source_size = size;
    
    const char* text = (const char*)data;
    
    /* Count rules (open braces) */
    for (size_t i = 0; i < size; i++) {
        if (data[i] == '{') css->rule_count++;
    }
    
    /* Check for features */
    if (strstr(text, "@import") != NULL) {
        const char* p = text;
        while ((p = strstr(p, "@import")) != NULL) {
            css->import_count++;
            p++;
        }
    }
    
    if (strstr(text, "@media") != NULL) css->media_query_count++;
    if (strstr(text, "--") != NULL) css->has_variables = true;
    if (strstr(text, "@keyframes") != NULL) css->has_keyframes = true;
    
    css->valid = (css->rule_count > 0);
    return true;
}

#ifdef CSS_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== CSS Parser v3 Tests ===\n");
    printf("Testing... ");
    const char* css = "body { color: red; } .class { margin: 0; }";
    css_file_t file;
    assert(css_parse((const uint8_t*)css, strlen(css), &file) && file.rule_count == 2);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
