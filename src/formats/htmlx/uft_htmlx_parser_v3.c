/**
 * @file uft_html_parser_v3.c
 * @brief GOD MODE HTML Parser v3 - HTML Document
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

/* strcasestr is POSIX but not available on Windows/MinGW */
#if defined(_WIN32) || defined(__MINGW32__) || defined(__MINGW64__)
static const char *uft_strcasestr(const char *haystack, const char *needle)
{
    if (!needle[0]) return haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack, *n = needle;
        while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
            h++; n++;
        }
        if (!*n) return haystack;
    }
    return NULL;
}
#define strcasestr uft_strcasestr
#endif

typedef struct {
    bool has_doctype;
    bool has_html_tag;
    bool has_head;
    bool has_body;
    bool is_html5;
    bool is_xhtml;
    size_t source_size;
    bool valid;
} html_file_t;

static bool html_parse(const uint8_t* data, size_t size, html_file_t* html) {
    if (!data || !html || size < 10) return false;
    memset(html, 0, sizeof(html_file_t));
    html->source_size = size;
    
    const char* text = (const char*)data;
    
    if (strncasecmp(text, "<!DOCTYPE", 9) == 0) {
        html->has_doctype = true;
        if (strstr(text, "<!DOCTYPE html>") != NULL) html->is_html5 = true;
    }
    
    if (strcasestr(text, "<html") != NULL) html->has_html_tag = true;
    if (strcasestr(text, "<head") != NULL) html->has_head = true;
    if (strcasestr(text, "<body") != NULL) html->has_body = true;
    if (strstr(text, "xmlns") != NULL) html->is_xhtml = true;
    
    html->valid = html->has_html_tag || html->has_doctype;
    return true;
}

#ifdef HTML_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== HTML Parser v3 Tests ===\n");
    printf("Testing... ");
    const char* html = "<!DOCTYPE html><html><head></head><body></body></html>";
    html_file_t file;
    assert(html_parse((const uint8_t*)html, strlen(html), &file) && file.is_html5);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
