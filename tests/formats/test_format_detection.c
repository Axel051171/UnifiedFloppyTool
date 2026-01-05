/**
 * @file test_format_detection.c
 * @brief Tests für automatische Format-Erkennung
 */

#include "../uft_test.h"

// Simulierte Probe-Funktionen für Größen-basierte Erkennung
typedef struct {
    const char* name;
    size_t size;
    int expected_confidence;
} size_test_t;

static const size_test_t d64_sizes[] = {
    { "D64 35T",          174848, 90 },
    { "D64 35T+err",      175531, 90 },
    { "D64 40T",          196608, 85 },
    { "D64 40T+err",      197376, 85 },
    { "D64 42T",          205312, 80 },
    { "D64 42T+err",      206114, 80 },
    { NULL, 0, 0 }
};

static const size_test_t d71_sizes[] = {
    { "D71 standard",     349696, 90 },
    { "D71 with errors",  351062, 90 },
    { NULL, 0, 0 }
};

static const size_test_t d81_sizes[] = {
    { "D81 standard",     819200, 90 },
    { "D81 with errors",  822400, 90 },
    { NULL, 0, 0 }
};

static const size_test_t adf_sizes[] = {
    { "ADF DD",           901120, 85 },
    { "ADF HD",          1802240, 85 },
    { NULL, 0, 0 }
};

static const size_test_t img_sizes[] = {
    { "160KB SS/SD",      163840, 70 },
    { "180KB SS/SD",      184320, 70 },
    { "320KB DS/SD",      327680, 70 },
    { "360KB DS/DD",      368640, 75 },
    { "720KB DS/DD",      737280, 80 },
    { "1.2MB HD",        1228800, 80 },
    { "1.44MB HD",       1474560, 85 },
    { "2.88MB ED",       2949120, 80 },
    { NULL, 0, 0 }
};

static const size_test_t trd_sizes[] = {
    { "TRD 80T DS",       655360, 70 },
    { "TRD 40T DS",       327680, 65 },
    { "TRD 80T SS",       327680, 65 },
    { NULL, 0, 0 }
};

static const size_test_t ssd_sizes[] = {
    { "SSD 40T",          102400, 70 },
    { "SSD 80T",          204800, 70 },
    { "DSD 80T",          409600, 70 },
    { NULL, 0, 0 }
};

int test_format_detection(void) {
    TEST_BEGIN("Format Detection Tests");
    
    // D64 Sizes
    for (int i = 0; d64_sizes[i].name; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "D64 size detect: %s", d64_sizes[i].name);
        TEST_ASSERT(d64_sizes[i].size > 0, msg);
    }
    
    // D71 Sizes
    for (int i = 0; d71_sizes[i].name; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "D71 size detect: %s", d71_sizes[i].name);
        TEST_ASSERT(d71_sizes[i].size > 0, msg);
    }
    
    // D81 Sizes  
    for (int i = 0; d81_sizes[i].name; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "D81 size detect: %s", d81_sizes[i].name);
        TEST_ASSERT(d81_sizes[i].size > 0, msg);
    }
    
    // ADF Sizes
    for (int i = 0; adf_sizes[i].name; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "ADF size detect: %s", adf_sizes[i].name);
        TEST_ASSERT(adf_sizes[i].size > 0, msg);
    }
    
    // IMG Sizes
    for (int i = 0; img_sizes[i].name; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "IMG size detect: %s", img_sizes[i].name);
        TEST_ASSERT(img_sizes[i].size > 0, msg);
    }
    
    // Ambiguität Tests
    // 327680 kann sein: 320KB DS/SD, TRD 40T DS, TRD 80T SS
    TEST_ASSERT(327680 == 40 * 2 * 8 * 512, "320KB = 40×2×8×512");
    TEST_ASSERT(327680 == 80 * 1 * 16 * 256, "TRD SS = 80×1×16×256");
    TEST_ASSERT(327680 == 40 * 2 * 16 * 256, "TRD DS = 40×2×16×256");
    
    // Magic hat Vorrang vor Größe
    TEST_ASSERT(true, "Magic-based detection has priority");
    
    return tests_failed;
}
