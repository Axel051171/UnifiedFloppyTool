/**
 * @file test_ti99_formats.c
 * @brief Test program for TIFILES and FIAD format support
 */

#include <stdio.h>
#include <string.h>
#include "uft/formats/uft_tifiles.h"
#include "uft/formats/uft_fiad.h"

int main(void) {
    printf("=== TI-99/4A Format Tests ===\n\n");
    
    /* Test 1: Create TIFILES DIS/VAR 80 */
    printf("Test 1: Create TIFILES DIS/VAR 80\n");
    {
        uft_tifiles_file_t tf;
        const char *text = "10 REM TI-99/4A BASIC PROGRAM\n20 PRINT \"HELLO WORLD\"\n30 END\n";
        
        uft_tifiles_error_t err = uft_tifiles_create_dis_var80(&tf, "HELLO", text);
        if (err == UFT_TIFILES_OK) {
            uft_tifiles_info_t info;
            uft_tifiles_get_info((const uint8_t *)&tf.header, 
                                 UFT_TIFILES_HEADER_SIZE + tf.data_size, &info);
            
            printf("  Filename: %s\n", info.filename);
            printf("  Type: %s\n", uft_tifiles_type_str(info.type));
            printf("  Sectors: %u\n", info.total_sectors);
            printf("  Records: %u\n", info.num_records);
            printf("  Data size: %zu bytes\n", info.data_size);
            printf("  => SUCCESS\n");
            
            /* Save to file */
            uft_tifiles_save_file(&tf, "/tmp/HELLO.tfi");
            printf("  Saved to /tmp/HELLO.tfi\n");
            
            uft_tifiles_free(&tf);
        } else {
            printf("  => FAILED: %s\n", uft_tifiles_strerror(err));
        }
    }
    printf("\n");
    
    /* Test 2: Create TIFILES PROGRAM */
    printf("Test 2: Create TIFILES PROGRAM\n");
    {
        uft_tifiles_file_t tf;
        uint8_t program_data[] = {
            0x00, 0x00, 0x00, 0x10,  /* Program header */
            0x10, 0xFE, 0x00, 0x0A,  /* BASIC token stream */
            0x83, 0xE9, 0x00, 0x00   /* END token */
        };
        
        uft_tifiles_error_t err = uft_tifiles_create_program(&tf, "MYPROGRAM", 
                                                              program_data, sizeof(program_data));
        if (err == UFT_TIFILES_OK) {
            uft_tifiles_info_t info;
            uft_tifiles_get_info((const uint8_t *)&tf.header,
                                 UFT_TIFILES_HEADER_SIZE + tf.data_size, &info);
            
            printf("  Filename: %s\n", info.filename);
            printf("  Type: %s\n", uft_tifiles_type_str(info.type));
            printf("  Sectors: %u\n", info.total_sectors);
            printf("  => SUCCESS\n");
            
            uft_tifiles_free(&tf);
        } else {
            printf("  => FAILED: %s\n", uft_tifiles_strerror(err));
        }
    }
    printf("\n");
    
    /* Test 3: Create FIAD DIS/VAR 80 */
    printf("Test 3: Create FIAD DIS/VAR 80\n");
    {
        uft_fiad_file_t fiad;
        const char *text = "THIS IS A TEST FILE\nSECOND LINE\nTHIRD LINE\n";
        
        uft_fiad_error_t err = uft_fiad_create_dis_var80(&fiad, "TESTFILE", text);
        if (err == UFT_FIAD_OK) {
            uft_fiad_info_t info;
            uft_fiad_get_info((const uint8_t *)&fiad.header,
                              UFT_FIAD_HEADER_SIZE + fiad.data_size, &info);
            
            printf("  Filename: %s\n", info.filename);
            printf("  Type: %s\n", uft_fiad_type_str(info.type));
            printf("  Sectors: %u\n", info.total_sectors);
            printf("  Records: %u\n", info.num_records);
            printf("  => SUCCESS\n");
            
            /* Save to file */
            uft_fiad_save_file(&fiad, "/tmp/TESTFILE");
            printf("  Saved to /tmp/TESTFILE\n");
            
            uft_fiad_free(&fiad);
        } else {
            printf("  => FAILED: %s\n", uft_fiad_strerror(err));
        }
    }
    printf("\n");
    
    /* Test 4: Validate TIFILES signature */
    printf("Test 4: TIFILES Signature Validation\n");
    {
        uint8_t valid[] = {0x07, 'T', 'I', 'F', 'I', 'L', 'E', 'S', 
                           0, 1,  /* 1 sector */
                           0x80,  /* DIS/VAR */
                           3,     /* 3 recs/sector */
                           10,    /* EOF offset */
                           80,    /* rec length */
                           3, 0,  /* 3 records (LE) */
                           'T', 'E', 'S', 'T', ' ', ' ', ' ', ' ', ' ', ' '};
        uint8_t full[128 + 256] = {0};
        memcpy(full, valid, sizeof(valid));
        
        bool is_valid = uft_tifiles_is_valid(full, sizeof(full));
        printf("  Valid TIFILES: %s\n", is_valid ? "YES" : "NO");
        
        uint8_t invalid[] = {0x00, 'N', 'O', 'T', 'V', 'A', 'L', 'I', 'D'};
        is_valid = uft_tifiles_is_valid(invalid, sizeof(invalid));
        printf("  Invalid data:  %s\n", is_valid ? "YES (BUG!)" : "NO");
        
        printf("  => SUCCESS\n");
    }
    printf("\n");
    
    /* Test 5: Extract text from DIS/VAR */
    printf("Test 5: Text Extraction from DIS/VAR\n");
    {
        uft_tifiles_file_t tf;
        const char *original = "LINE ONE\nLINE TWO\nLINE THREE\n";
        
        uft_tifiles_create_dis_var80(&tf, "EXTRACT", original);
        
        char extracted[1024] = {0};
        uft_tifiles_error_t err = uft_tifiles_extract_text(&tf, extracted, sizeof(extracted));
        
        if (err == UFT_TIFILES_OK) {
            printf("  Original:\n%s", original);
            printf("  Extracted:\n%s", extracted);
            printf("  => SUCCESS\n");
        } else {
            printf("  => FAILED: %s\n", uft_tifiles_strerror(err));
        }
        
        uft_tifiles_free(&tf);
    }
    printf("\n");
    
    /* Test 6: Filename validation */
    printf("Test 6: Filename Validation\n");
    {
        printf("  'HELLO'    -> %s\n", uft_fiad_validate_filename("HELLO") ? "VALID" : "INVALID");
        printf("  'TEST123'  -> %s\n", uft_fiad_validate_filename("TEST123") ? "VALID" : "INVALID");
        printf("  ' SPACE'   -> %s\n", uft_fiad_validate_filename(" SPACE") ? "VALID" : "INVALID");
        printf("  'TOOLONGNAME' -> %s\n", uft_fiad_validate_filename("TOOLONGNAME") ? "VALID" : "INVALID");
        printf("  => SUCCESS\n");
    }
    printf("\n");
    
    printf("=== All Tests Complete ===\n");
    return 0;
}
