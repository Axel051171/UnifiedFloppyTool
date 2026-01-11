/**
 * @file uft_xml_parser_v3.c
 * @brief GOD MODE XML Parser v3 - XML Configuration
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
    bool has_declaration;
    bool has_doctype;
    char root_element[64];
    size_t source_size;
    bool valid;
} xml_file_t;

static bool xml_parse(const uint8_t* data, size_t size, xml_file_t* xml) {
    if (!data || !xml || size < 5) return false;
    memset(xml, 0, sizeof(xml_file_t));
    xml->source_size = size;
    
    const char* text = (const char*)data;
    
    /* Check for XML declaration */
    xml->has_declaration = (strstr(text, "<?xml") != NULL);
    xml->has_doctype = (strstr(text, "<!DOCTYPE") != NULL);
    
    /* Find root element */
    const char* root = strchr(text, '<');
    while (root && (root[1] == '?' || root[1] == '!')) {
        root = strchr(root + 1, '<');
    }
    if (root && root[1] != '/') {
        const char* end = strpbrk(root + 1, " \t\n\r>");
        if (end) {
            size_t len = end - root - 1;
            if (len < 63) {
                memcpy(xml->root_element, root + 1, len);
                xml->root_element[len] = '\0';
            }
        }
    }
    
    xml->valid = (xml->has_declaration || strchr(text, '<') != NULL);
    return true;
}

#ifdef XML_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== XML Parser v3 Tests ===\n");
    printf("Testing... ");
    const char* xml = "<?xml version=\"1.0\"?><root><item/></root>";
    xml_file_t file;
    assert(xml_parse((const uint8_t*)xml, strlen(xml), &file) && file.has_declaration);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
