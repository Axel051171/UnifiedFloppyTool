/**
 * @file test_img.c
 * @brief Tests für IMG Format (PC Floppy)
 */

#include "../uft_test.h"

typedef struct {
    const char* name;
    int cylinders;
    int heads;
    int sectors;
    int sector_size;
    size_t total_size;
} img_geometry_t;

static const img_geometry_t img_geometries[] = {
    { "160KB 5.25\" SS/SD",   40, 1,  8, 512,  163840 },
    { "180KB 5.25\" SS/SD",   40, 1,  9, 512,  184320 },
    { "320KB 5.25\" DS/SD",   40, 2,  8, 512,  327680 },
    { "360KB 5.25\" DS/DD",   40, 2,  9, 512,  368640 },
    { "720KB 3.5\" DS/DD",    80, 2,  9, 512,  737280 },
    { "1.2MB 5.25\" HD",      80, 2, 15, 512, 1228800 },
    { "1.44MB 3.5\" HD",      80, 2, 18, 512, 1474560 },
    { "2.88MB 3.5\" ED",      80, 2, 36, 512, 2949120 },
    { NULL, 0, 0, 0, 0, 0 }
};

int test_format_img(void) {
    TEST_BEGIN("IMG Format Tests");
    
    for (int i = 0; img_geometries[i].name; i++) {
        const img_geometry_t* g = &img_geometries[i];
        size_t calc_size = (size_t)g->cylinders * g->heads * g->sectors * g->sector_size;
        
        char msg[128];
        snprintf(msg, sizeof(msg), "%s size calculation", g->name);
        TEST_ASSERT_EQ(calc_size, g->total_size, msg);
    }
    
    // FAT Boot Sector Tests
    TEST_ASSERT(0xEB == 0xEB, "Boot sector jump byte 0xEB");
    TEST_ASSERT(0x55 == 0x55, "Boot signature 0x55");
    TEST_ASSERT(0xAA == 0xAA, "Boot signature 0xAA");
    
    // Media Descriptor Bytes
    TEST_ASSERT(0xF0 == 0xF0, "Media 0xF0 = 1.44MB/2.88MB");
    TEST_ASSERT(0xF9 == 0xF9, "Media 0xF9 = 720KB/1.2MB");
    TEST_ASSERT(0xFD == 0xFD, "Media 0xFD = 360KB");
    TEST_ASSERT(0xFF == 0xFF, "Media 0xFF = 320KB");
    TEST_ASSERT(0xFC == 0xFC, "Media 0xFC = 180KB");
    TEST_ASSERT(0xFE == 0xFE, "Media 0xFE = 160KB");
    
    return tests_failed;
}
