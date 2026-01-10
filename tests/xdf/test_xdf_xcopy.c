/**
 * @file test_xdf_xcopy.c
 * @brief Test XDF ↔ XCopy Integration
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "profiles/uft_profiles_all.h"

int main(void)
{
    printf("=== XDF ↔ XCopy Integration Test ===\n\n");
    
    /* Test 1: XDF Profil-Erkennung */
    printf("1. XDF Profile Detection:\n");
    const uft_platform_profile_t *p;
    
    p = uft_detect_profile_by_size(1915904);
    printf("   1915904 bytes: %s\n", p ? p->name : "NULL");
    assert(p != NULL && strstr(p->name, "XDF") != NULL);
    
    p = uft_detect_profile_by_size(1720320);
    printf("   1720320 bytes: %s\n", p ? p->name : "NULL");
    assert(p != NULL && strstr(p->name, "DMF") != NULL);
    
    printf("   ✓ XDF/DMF profiles detected\n\n");
    
    /* Test 2: Track Copy Requirement */
    printf("2. Track Copy Requirement:\n");
    
    assert(uft_format_requires_track_copy("IBM XDF") == true);
    printf("   XDF: requires Track Copy ✓\n");
    
    assert(uft_format_requires_track_copy("DMF") == true);
    printf("   DMF: requires Track Copy ✓\n");
    
    assert(uft_format_requires_track_copy("Victor") == true);
    printf("   Victor 9000: requires Track Copy ✓\n");
    
    assert(uft_format_requires_track_copy("IBM PC HD") == false);
    printf("   IBM PC HD: Sector Copy OK ✓\n");
    
    assert(uft_format_requires_track_copy("Amiga DD") == false);
    printf("   Amiga DD: Sector Copy OK ✓\n");
    
    printf("   ✓ Track Copy detection works\n\n");
    
    /* Test 3: Copy Mode Recommendation */
    printf("3. XDF Copy Mode Recommendation:\n");
    
    int mode = uft_xdf_recommended_copy_mode(false);
    printf("   Unprotected XDF: Mode %d (Track) ✓\n", mode);
    assert(mode == 2);
    
    mode = uft_xdf_recommended_copy_mode(true);
    printf("   Protected XDF: Mode %d (Flux) ✓\n", mode);
    assert(mode == 3);
    
    printf("   ✓ Copy mode recommendation works\n\n");
    
    /* Test 4: XDF Sector Geometry */
    printf("4. XDF Sector Geometry:\n");
    
    int s0 = uft_xdf_sectors_for_track(0);
    int s1 = uft_xdf_sectors_for_track(1);
    int s79 = uft_xdf_sectors_for_track(79);
    
    printf("   Track 0:  %d sectors (8KB+2KB+1KB+512B)\n", s0);
    printf("   Track 1:  %d sectors (8KB+8KB+2KB+1KB+512B)\n", s1);
    printf("   Track 79: %d sectors\n", s79);
    
    assert(s0 == 4);
    assert(s1 == 5);
    assert(s79 == 5);
    
    printf("   ✓ Variable sector geometry works\n\n");
    
    /* Test 5: Profile Count */
    printf("5. Profile Count:\n");
    int count = uft_get_profile_count();
    printf("   Total profiles: %d (inkl. XDF/XXDF/DMF)\n", count);
    assert(count >= 53);
    printf("   ✓ All profiles available\n\n");
    
    /* Summary */
    printf("════════════════════════════════════════════════════════\n");
    printf("✓ XDF ↔ XCopy Integration: VOLLSTÄNDIG FUNKTIONAL\n");
    printf("════════════════════════════════════════════════════════\n");
    printf("\n");
    printf("XCopy Workflow für XDF:\n");
    printf("  1. Datei öffnen → XDF erkannt (Größe 1.86MB)\n");
    printf("  2. Quick Scan → uft_format_requires_track_copy() = true\n");
    printf("  3. XCopy Panel → Track Copy automatisch aktiviert\n");
    printf("  4. Sector Copy → blockiert (variable Sektoren)\n");
    printf("  5. Kopieren → Track-für-Track, kein Bit verloren!\n");
    printf("\n");
    
    return 0;
}
