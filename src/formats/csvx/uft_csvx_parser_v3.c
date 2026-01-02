/**
 * @file uft_csv_parser_v3.c
 * @brief GOD MODE CSV Parser v3 - Comma Separated Values
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
    uint32_t row_count;
    uint32_t column_count;
    char delimiter;
    bool has_header;
    bool has_quotes;
    size_t source_size;
    bool valid;
} csv_file_t;

static bool csv_parse(const uint8_t* data, size_t size, csv_file_t* csv) {
    if (!data || !csv || size < 1) return false;
    memset(csv, 0, sizeof(csv_file_t));
    csv->source_size = size;
    csv->delimiter = ',';
    
    /* Detect delimiter */
    uint32_t comma_count = 0, semi_count = 0, tab_count = 0;
    for (size_t i = 0; i < size && i < 1000; i++) {
        if (data[i] == ',') comma_count++;
        else if (data[i] == ';') semi_count++;
        else if (data[i] == '\t') tab_count++;
        else if (data[i] == '"') csv->has_quotes = true;
    }
    
    if (semi_count > comma_count) csv->delimiter = ';';
    if (tab_count > comma_count && tab_count > semi_count) csv->delimiter = '\t';
    
    /* Count rows and columns */
    uint32_t cols_first_row = 1;
    bool in_first_row = true;
    for (size_t i = 0; i < size; i++) {
        if (data[i] == '\n') {
            csv->row_count++;
            in_first_row = false;
        } else if (data[i] == csv->delimiter && in_first_row) {
            cols_first_row++;
        }
    }
    if (size > 0 && data[size-1] != '\n') csv->row_count++;
    csv->column_count = cols_first_row;
    
    csv->valid = (csv->row_count > 0);
    return true;
}

#ifdef CSV_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== CSV Parser v3 Tests ===\n");
    printf("Testing... ");
    const char* csv = "name,age,city\nAlice,30,NYC\nBob,25,LA\n";
    csv_file_t file;
    assert(csv_parse((const uint8_t*)csv, strlen(csv), &file) && file.column_count == 3);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
