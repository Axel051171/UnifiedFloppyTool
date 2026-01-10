/**
 * @file test_write_gate.c
 * @brief Write Safety Gate Tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "uft/policy/uft_write_gate.h"
#include "uft/core/uft_sha256.h"
#include "uft/core/uft_snapshot.h"

/* Test data - simulated ADF (Amiga DD) */
#define ADF_SIZE 901120

static uint8_t *create_test_adf(void) {
    uint8_t *data = calloc(1, ADF_SIZE);
    if (!data) return NULL;
    
    /* Add some recognizable pattern */
    data[0] = 'D';
    data[1] = 'O';
    data[2] = 'S';
    
    return data;
}

/* Test data - simulated D64 (C64 1541) */
#define D64_SIZE 174848

static uint8_t *create_test_d64(void) {
    uint8_t *data = calloc(1, D64_SIZE);
    if (!data) return NULL;
    
    /* BAM marker */
    data[0x16500] = 0x12;  /* Track 18 */
    data[0x16501] = 0x01;  /* Sector 1 */
    data[0x16502] = 0x41;  /* 'A' - DOS version */
    
    return data;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * SHA-256 Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_sha256_basic(void) {
    printf("  SHA-256 basic... ");
    
    /* Test vector: empty string */
    uint8_t hash[32];
    uft_sha256("", 0, hash);
    
    /* Expected: e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 */
    assert(hash[0] == 0xe3);
    assert(hash[1] == 0xb0);
    assert(hash[31] == 0x55);
    
    /* Test vector: "abc" */
    uft_sha256("abc", 3, hash);
    /* Expected: ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad */
    assert(hash[0] == 0xba);
    assert(hash[1] == 0x78);
    
    printf("PASS\n");
}

static void test_sha256_hex(void) {
    printf("  SHA-256 hex format... ");
    
    uint8_t hash[32];
    char hex[65];
    
    uft_sha256("test", 4, hash);
    uft_sha256_to_hex(hash, hex);
    
    assert(strlen(hex) == 64);
    assert(hex[64] == '\0');
    
    /* Should be lowercase hex */
    for (int i = 0; i < 64; i++) {
        assert((hex[i] >= '0' && hex[i] <= '9') ||
               (hex[i] >= 'a' && hex[i] <= 'f'));
    }
    
    printf("PASS\n");
}

static void test_sha256_compare(void) {
    printf("  SHA-256 compare... ");
    
    uint8_t hash1[32], hash2[32], hash3[32];
    
    uft_sha256("test", 4, hash1);
    uft_sha256("test", 4, hash2);
    uft_sha256("different", 9, hash3);
    
    assert(uft_sha256_compare(hash1, hash2) == 0);  /* Same */
    assert(uft_sha256_compare(hash1, hash3) != 0);  /* Different */
    
    printf("PASS\n");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Format Detection Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_format_detect_adf(void) {
    printf("  Format detect ADF... ");
    
    uint8_t *data = create_test_adf();
    assert(data != NULL);
    
    uft_format_probe_t probe;
    uft_error_t err = uft_write_gate_probe_format(data, ADF_SIZE, &probe);
    
    assert(err == UFT_SUCCESS);
    assert(strstr(probe.format_name, "ADF") != NULL);
    assert(probe.confidence >= 800);
    assert(probe.capabilities & UFT_FMT_CAP_READ);
    assert(probe.capabilities & UFT_FMT_CAP_WRITE);
    
    free(data);
    printf("PASS (%s, conf=%u)\n", probe.format_name, probe.confidence);
}

static void test_format_detect_d64(void) {
    printf("  Format detect D64... ");
    
    uint8_t *data = create_test_d64();
    assert(data != NULL);
    
    uft_format_probe_t probe;
    uft_error_t err = uft_write_gate_probe_format(data, D64_SIZE, &probe);
    
    assert(err == UFT_SUCCESS);
    assert(strstr(probe.format_name, "D64") != NULL);
    assert(probe.confidence >= 800);
    
    free(data);
    printf("PASS (%s, conf=%u)\n", probe.format_name, probe.confidence);
}

static void test_format_detect_unknown(void) {
    printf("  Format detect unknown... ");
    
    uint8_t data[1000];
    memset(data, 0xAA, sizeof(data));
    
    uft_format_probe_t probe;
    uft_error_t err = uft_write_gate_probe_format(data, sizeof(data), &probe);
    
    /* Should fail - unknown format */
    assert(err == UFT_ERR_FORMAT_DETECT);
    assert(probe.confidence == 0);
    
    printf("PASS (correctly rejected)\n");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Write Gate Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_gate_strict_no_snapshot_dir(void) {
    printf("  Gate: strict, no snapshot dir... ");
    
    uint8_t *data = create_test_adf();
    assert(data != NULL);
    
    uft_write_gate_policy_t policy = UFT_GATE_POLICY_STRICT;
    policy.require_drive_diag = false;  /* No hardware */
    
    uft_write_gate_result_t result;
    uft_gate_status_t status = uft_write_gate_precheck(
        &policy, data, ADF_SIZE,
        NULL, NULL,  /* No snapshot dir */
        &result);
    
    assert(status == UFT_GATE_SNAPSHOT_FAILED);
    assert(result.checks_failed & UFT_CHECK_SNAPSHOT);
    
    free(data);
    printf("PASS (correctly blocked)\n");
}

static void test_gate_image_only_success(void) {
    printf("  Gate: image-only, full success... ");
    
    uint8_t *data = create_test_adf();
    assert(data != NULL);
    
    uft_write_gate_policy_t policy = UFT_GATE_POLICY_IMAGE_ONLY;
    
    uft_write_gate_result_t result;
    uft_gate_status_t status = uft_write_gate_precheck(
        &policy, data, ADF_SIZE,
        "/tmp", "uft_test",
        &result);
    
    assert(status == UFT_GATE_OK);
    assert(result.checks_passed & UFT_CHECK_FORMAT);
    assert(result.checks_passed & UFT_CHECK_SNAPSHOT);
    assert(result.snapshot.size_bytes == ADF_SIZE);
    
    /* Verify snapshot was created */
    assert(result.snapshot.path[0] != '\0');
    
    /* Clean up snapshot */
    uft_snapshot_delete(&result.snapshot);
    
    free(data);
    printf("PASS (snapshot: %s)\n", result.snapshot.path);
}

static void test_gate_relaxed_readonly_format(void) {
    printf("  Gate: relaxed, read-only format... ");
    
    /* NIB format is read-only */
    uint8_t *data = calloc(1, 232960);  /* NIB size */
    assert(data != NULL);
    
    uft_write_gate_policy_t policy = UFT_GATE_POLICY_RELAXED;
    
    uft_write_gate_result_t result;
    uft_gate_status_t status = uft_write_gate_precheck(
        &policy, data, 232960,
        "/tmp", "uft_test_nib",
        &result);
    
    /* Should return FORMAT_READONLY with override option */
    assert(status == UFT_GATE_FORMAT_READONLY);
    assert(result.override_required == true);
    assert(uft_write_gate_can_override(&result) == true);
    
    /* Apply override */
    status = uft_write_gate_apply_override(&result, "Test override");
    assert(status == UFT_GATE_OK);
    assert(result.override_required == false);
    
    /* Clean up */
    if (result.snapshot.path[0]) {
        uft_snapshot_delete(&result.snapshot);
    }
    
    free(data);
    printf("PASS (override works)\n");
}

static void test_gate_status_strings(void) {
    printf("  Gate status strings... ");
    
    assert(strstr(uft_gate_status_str(UFT_GATE_OK), "OK") != NULL);
    assert(strstr(uft_gate_status_str(UFT_GATE_FORMAT_READONLY), "read-only") != NULL);
    assert(strstr(uft_gate_status_str(UFT_GATE_SNAPSHOT_FAILED), "Snapshot") != NULL);
    
    printf("PASS\n");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Snapshot Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_snapshot_create_verify(void) {
    printf("  Snapshot create + verify... ");
    
    const uint8_t test_data[] = "This is test data for snapshot verification.";
    
    uft_snapshot_t snap;
    uft_error_t err = uft_snapshot_create(
        "/tmp", "uft_snap_test",
        test_data, sizeof(test_data),
        NULL, &snap);
    
    assert(err == UFT_SUCCESS);
    assert(snap.size_bytes == sizeof(test_data));
    assert(snap.path[0] != '\0');
    
    /* Verify */
    err = uft_snapshot_verify(&snap);
    assert(err == UFT_SUCCESS);
    
    /* Get hash string */
    char hash_str[65];
    uft_snapshot_get_hash_str(&snap, hash_str);
    assert(strlen(hash_str) == 64);
    
    /* Clean up */
    uft_snapshot_delete(&snap);
    
    printf("PASS (hash: %.16s...)\n", hash_str);
}

static void test_snapshot_restore(void) {
    printf("  Snapshot restore... ");
    
    const char *test_data = "Restore test data - Bei uns geht kein Bit verloren!";
    size_t len = strlen(test_data) + 1;
    
    uft_snapshot_t snap;
    uft_error_t err = uft_snapshot_create(
        "/tmp", "uft_restore_test",
        (const uint8_t *)test_data, len,
        NULL, &snap);
    
    assert(err == UFT_SUCCESS);
    
    /* Restore to buffer */
    uint8_t *restored = malloc(len);
    err = uft_snapshot_restore(&snap, restored, len);
    assert(err == UFT_SUCCESS);
    assert(memcmp(test_data, restored, len) == 0);
    
    free(restored);
    uft_snapshot_delete(&snap);
    
    printf("PASS\n");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("═══════════════════════════════════════════════════════════\n");
    printf(" UFT Write Safety Gate Tests\n");
    printf("═══════════════════════════════════════════════════════════\n\n");
    
    printf("SHA-256:\n");
    test_sha256_basic();
    test_sha256_hex();
    test_sha256_compare();
    
    printf("\nFormat Detection:\n");
    test_format_detect_adf();
    test_format_detect_d64();
    test_format_detect_unknown();
    
    printf("\nWrite Gate Policy:\n");
    test_gate_strict_no_snapshot_dir();
    test_gate_image_only_success();
    test_gate_relaxed_readonly_format();
    test_gate_status_strings();
    
    printf("\nSnapshot System:\n");
    test_snapshot_create_verify();
    test_snapshot_restore();
    
    printf("\n═══════════════════════════════════════════════════════════\n");
    printf(" ✓ All Write Safety Gate tests passed!\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n\"Bei uns geht kein Bit verloren\" - auch nicht beim Schreiben!\n\n");
    
    return 0;
}
