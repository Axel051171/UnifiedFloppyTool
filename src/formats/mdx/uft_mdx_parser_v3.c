/**
 * @file uft_md_parser_v3.c
 * @brief GOD MODE MD Parser v3 - Markdown
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
    uint32_t heading_count;
    uint32_t link_count;
    uint32_t code_block_count;
    uint32_t list_count;
    bool has_yaml_frontmatter;
    size_t source_size;
    bool valid;
} md_file_t;

static bool md_parse(const uint8_t* data, size_t size, md_file_t* md) {
    if (!data || !md || size < 1) return false;
    memset(md, 0, sizeof(md_file_t));
    md->source_size = size;
    
    /* Check for YAML front matter */
    if (size >= 4 && memcmp(data, "---\n", 4) == 0) {
        md->has_yaml_frontmatter = true;
    }
    
    bool at_line_start = true;
    for (size_t i = 0; i < size; i++) {
        if (at_line_start && data[i] == '#') md->heading_count++;
        if (at_line_start && (data[i] == '-' || data[i] == '*' || data[i] == '+')) md->list_count++;
        if (data[i] == '[' && i + 2 < size) md->link_count++;
        if (i + 2 < size && data[i] == '`' && data[i+1] == '`' && data[i+2] == '`') {
            md->code_block_count++;
            i += 2;
        }
        at_line_start = (data[i] == '\n');
    }
    md->code_block_count /= 2;  /* Opening and closing */
    
    md->valid = true;
    return true;
}

#ifdef MD_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Markdown Parser v3 Tests ===\n");
    printf("Testing... ");
    const char* md = "# Title\n## Subtitle\n- item1\n- item2\n";
    md_file_t file;
    assert(md_parse((const uint8_t*)md, strlen(md), &file) && file.heading_count == 2);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
