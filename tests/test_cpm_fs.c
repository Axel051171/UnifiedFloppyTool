/**
 * =============================================================================
 * CP/M Dateisystem CLI-Tool & Tests
 * =============================================================================
 */

#include "cpm_fs.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static int tests_run = 0, tests_passed = 0, tests_failed = 0;

#define TEST(name) do { printf("  %-50s ", name); tests_run++; } while(0)
#define PASS()     do { printf("✓\n"); tests_passed++; } while(0)
#define FAIL(m)    do { printf("✗ (%s)\n", m); tests_failed++; return; } while(0)


/* =============================================================================
 * Test-Infrastruktur: In-Memory Disk Image
 * ========================================================================== */

typedef struct {
    uint8_t  *data;
    uint32_t  size;
    uint16_t  sector_size;
    uint8_t   sectors_per_track;
    uint8_t   heads;
    uint8_t   first_sector;
} mem_disk_t;

static cpm_error_t mem_read(void *ctx, uint16_t cyl, uint8_t head,
                              uint8_t sector, uint8_t *buf, uint16_t *br)
{
    mem_disk_t *d = (mem_disk_t *)ctx;
    uint32_t sec_idx = sector - d->first_sector;
    uint32_t abs = (uint32_t)cyl * d->heads * d->sectors_per_track +
                   (uint32_t)head * d->sectors_per_track + sec_idx;
    uint32_t offset = abs * d->sector_size;

    if (offset + d->sector_size > d->size) return CPM_ERR_READ;

    memcpy(buf, d->data + offset, d->sector_size);
    *br = d->sector_size;
    return CPM_OK;
}

static cpm_error_t mem_write(void *ctx, uint16_t cyl, uint8_t head,
                               uint8_t sector, const uint8_t *buf, uint16_t size)
{
    mem_disk_t *d = (mem_disk_t *)ctx;
    uint32_t sec_idx = sector - d->first_sector;
    uint32_t abs = (uint32_t)cyl * d->heads * d->sectors_per_track +
                   (uint32_t)head * d->sectors_per_track + sec_idx;
    uint32_t offset = abs * d->sector_size;

    if (offset + size > d->size) return CPM_ERR_WRITE;

    memcpy(d->data + offset, buf, size);
    return CPM_OK;
}

/**
 * Kaypro II Disk erstellen (SS/DD, 40T, 10×512, 1K Blöcke, 64 Dir)
 */
static mem_disk_t *create_kaypro_disk(void) {
    mem_disk_t *d = calloc(1, sizeof(mem_disk_t));
    d->sector_size = 512;
    d->sectors_per_track = 10;
    d->heads = 1;
    d->first_sector = 0;
    d->size = 40 * 1 * 10 * 512;  /* 200K */
    d->data = calloc(1, d->size);
    memset(d->data, 0xE5, d->size);  /* Leer formatiert */
    return d;
}

static void free_mem_disk(mem_disk_t *d) {
    if (d) { free(d->data); free(d); }
}


/* =============================================================================
 * Tests
 * ========================================================================== */

static void test_dpb_calc(void) {
    TEST("DPB Berechnung (Kaypro II)");

    cpm_geometry_t geom = {
        .sector_size = 512, .sectors_per_track = 10,
        .heads = 1, .cylinders = 40, .first_sector = 0
    };

    cpm_dpb_t dpb;
    cpm_error_t err = cpm_calc_dpb(&dpb, 1024, 64, 1, &geom);
    if (err != CPM_OK) FAIL("calc failed");
    if (dpb.spt != 40) FAIL("SPT != 40");
    if (dpb.bsh != 3) FAIL("BSH != 3");
    if (dpb.blm != 7) FAIL("BLM != 7");
    if (dpb.off != 1) FAIL("OFF != 1");
    if (dpb.drm != 63) FAIL("DRM != 63");
    if (dpb.block_size != 1024) FAIL("block_size");
    if (dpb.use_16bit) FAIL("soll 8-Bit sein");

    PASS();
}

static void test_name_parse(void) {
    TEST("Dateiname-Parsing");

    char name[9], ext[4];

    cpm_error_t err = cpm_parse_name("TEST.COM", name, ext);
    if (err != CPM_OK) FAIL("parse 1");
    if (memcmp(name, "TEST    ", 8) != 0) FAIL("name 1");
    if (memcmp(ext, "COM", 3) != 0) FAIL("ext 1");

    err = cpm_parse_name("hello.txt", name, ext);
    if (err != CPM_OK) FAIL("parse 2");
    if (memcmp(name, "HELLO   ", 8) != 0) FAIL("name 2");
    if (memcmp(ext, "TXT", 3) != 0) FAIL("ext 2");

    err = cpm_parse_name("LONGNAME.X", name, ext);
    if (err != CPM_OK) FAIL("parse 3");
    if (memcmp(name, "LONGNAME", 8) != 0) FAIL("name 3");
    if (memcmp(ext, "X  ", 3) != 0) FAIL("ext 3");

    err = cpm_parse_name(".COM", name, ext);
    if (err == CPM_OK) FAIL("soll fehlschlagen");

    PASS();
}

static void test_name_format(void) {
    TEST("Dateiname-Formatierung");

    char output[CPM_FULLNAME_MAX];
    uint8_t name[] = "TEST    ";
    uint8_t ext[] = "COM";
    cpm_format_name(name, ext, output);
    if (strcmp(output, "TEST.COM") != 0) FAIL(output);

    uint8_t name2[] = "X       ";
    uint8_t ext2[] = "   ";
    cpm_format_name(name2, ext2, output);
    if (strcmp(output, "X") != 0) FAIL(output);

    PASS();
}

static void test_open_close(void) {
    TEST("Disk Open/Close");

    mem_disk_t *md = create_kaypro_disk();
    cpm_geometry_t geom = {
        .sector_size = 512, .sectors_per_track = 10,
        .heads = 1, .cylinders = 40, .first_sector = 0
    };
    cpm_dpb_t dpb;
    cpm_calc_dpb(&dpb, 1024, 64, 1, &geom);

    cpm_disk_t *disk = cpm_open(&geom, &dpb, mem_read, mem_write, md);
    if (!disk) { free_mem_disk(md); FAIL("open"); }

    cpm_error_t err = cpm_close(disk);
    free_mem_disk(md);
    if (err != CPM_OK) FAIL("close");

    PASS();
}

static void test_format_and_read(void) {
    TEST("Format + Read Directory");

    mem_disk_t *md = create_kaypro_disk();
    cpm_geometry_t geom = {
        .sector_size = 512, .sectors_per_track = 10,
        .heads = 1, .cylinders = 40, .first_sector = 0
    };
    cpm_dpb_t dpb;
    cpm_calc_dpb(&dpb, 1024, 64, 1, &geom);

    cpm_disk_t *disk = cpm_open(&geom, &dpb, mem_read, mem_write, md);
    if (!disk) { free_mem_disk(md); FAIL("open"); }

    cpm_error_t err = cpm_format(disk);
    if (err != CPM_OK) { cpm_close(disk); free_mem_disk(md); FAIL("format"); }

    err = cpm_read_directory(disk);
    if (err != CPM_OK) { cpm_close(disk); free_mem_disk(md); FAIL("readdir"); }

    if (disk->num_files != 0) { cpm_close(disk); free_mem_disk(md); FAIL("nicht leer"); }
    if (disk->free_blocks == 0) { cpm_close(disk); free_mem_disk(md); FAIL("kein Platz"); }

    cpm_close(disk);
    free_mem_disk(md);
    PASS();
}

static void test_write_read_file(void) {
    TEST("Datei schreiben + lesen");

    mem_disk_t *md = create_kaypro_disk();
    cpm_geometry_t geom = {
        .sector_size = 512, .sectors_per_track = 10,
        .heads = 1, .cylinders = 40, .first_sector = 0
    };
    cpm_dpb_t dpb;
    cpm_calc_dpb(&dpb, 1024, 64, 1, &geom);

    cpm_disk_t *disk = cpm_open(&geom, &dpb, mem_read, mem_write, md);
    cpm_format(disk);
    cpm_read_directory(disk);

    /* Testdaten: "Hello, CP/M World!\r\n" × 10 */
    char test_data[512];
    int len = 0;
    for (int i = 0; i < 10; i++) {
        len += snprintf(test_data + len, sizeof(test_data) - len,
                        "Hello, CP/M World! Line %d\r\n", i + 1);
    }

    cpm_error_t err = cpm_write_file(disk, "HELLO.TXT", 0,
                                       (uint8_t *)test_data, len);
    if (err != CPM_OK) {
        cpm_close(disk); free_mem_disk(md);
        FAIL(cpm_error_str(err));
    }

    /* Prüfe ob Datei gefunden wird */
    const cpm_file_info_t *fi = cpm_find_file(disk, "HELLO.TXT", 0);
    if (!fi) { cpm_close(disk); free_mem_disk(md); FAIL("nicht gefunden"); }

    /* Datei lesen */
    uint8_t read_buf[4096];
    uint32_t br = 0;
    err = cpm_read_file(disk, fi, read_buf, sizeof(read_buf), &br);
    if (err != CPM_OK) { cpm_close(disk); free_mem_disk(md); FAIL("read"); }
    if (br == 0) { cpm_close(disk); free_mem_disk(md); FAIL("0 Bytes gelesen"); }

    /* Inhalt vergleichen */
    if (memcmp(read_buf, test_data, len) != 0) {
        cpm_close(disk); free_mem_disk(md);
        FAIL("Inhalt unterschiedlich");
    }

    cpm_close(disk);
    free_mem_disk(md);
    PASS();
}

static void test_multiple_files(void) {
    TEST("Mehrere Dateien");

    mem_disk_t *md = create_kaypro_disk();
    cpm_geometry_t geom = {
        .sector_size = 512, .sectors_per_track = 10,
        .heads = 1, .cylinders = 40, .first_sector = 0
    };
    cpm_dpb_t dpb;
    cpm_calc_dpb(&dpb, 1024, 64, 1, &geom);

    cpm_disk_t *disk = cpm_open(&geom, &dpb, mem_read, mem_write, md);
    cpm_format(disk);
    cpm_read_directory(disk);

    const char *names[] = {"FILE1.COM", "FILE2.TXT", "FILE3.BAS",
                            "DATA.DAT", "README.DOC"};
    const int num = 5;

    for (int i = 0; i < num; i++) {
        char data[128];
        int len = snprintf(data, sizeof(data), "Content of %s", names[i]);
        cpm_error_t err = cpm_write_file(disk, names[i], 0,
                                           (uint8_t *)data, len);
        if (err != CPM_OK) {
            cpm_close(disk); free_mem_disk(md);
            FAIL(names[i]);
        }
    }

    if (cpm_file_count(disk) != num) {
        cpm_close(disk); free_mem_disk(md);
        FAIL("falsche Anzahl");
    }

    /* Alle einzeln prüfen */
    for (int i = 0; i < num; i++) {
        const cpm_file_info_t *fi = cpm_find_file(disk, names[i], 0);
        if (!fi) { cpm_close(disk); free_mem_disk(md); FAIL(names[i]); }
    }

    cpm_close(disk);
    free_mem_disk(md);
    PASS();
}

static void test_delete_file(void) {
    TEST("Datei löschen");

    mem_disk_t *md = create_kaypro_disk();
    cpm_geometry_t geom = {
        .sector_size = 512, .sectors_per_track = 10,
        .heads = 1, .cylinders = 40, .first_sector = 0
    };
    cpm_dpb_t dpb;
    cpm_calc_dpb(&dpb, 1024, 64, 1, &geom);

    cpm_disk_t *disk = cpm_open(&geom, &dpb, mem_read, mem_write, md);
    cpm_format(disk);
    cpm_read_directory(disk);

    uint8_t data[] = "Test data for deletion";
    cpm_write_file(disk, "DELETE.ME", 0, data, sizeof(data));

    uint32_t free_before = disk->free_blocks;

    cpm_error_t err = cpm_delete_file(disk, "DELETE.ME", 0);
    if (err != CPM_OK) { cpm_close(disk); free_mem_disk(md); FAIL("delete"); }

    if (cpm_find_file(disk, "DELETE.ME", 0)) {
        cpm_close(disk); free_mem_disk(md);
        FAIL("noch vorhanden");
    }

    if (disk->free_blocks <= free_before) {
        cpm_close(disk); free_mem_disk(md);
        FAIL("Blöcke nicht freigegeben");
    }

    cpm_close(disk);
    free_mem_disk(md);
    PASS();
}

static void test_rename_file(void) {
    TEST("Datei umbenennen");

    mem_disk_t *md = create_kaypro_disk();
    cpm_geometry_t geom = {
        .sector_size = 512, .sectors_per_track = 10,
        .heads = 1, .cylinders = 40, .first_sector = 0
    };
    cpm_dpb_t dpb;
    cpm_calc_dpb(&dpb, 1024, 64, 1, &geom);

    cpm_disk_t *disk = cpm_open(&geom, &dpb, mem_read, mem_write, md);
    cpm_format(disk);
    cpm_read_directory(disk);

    uint8_t data[] = "Rename test data";
    cpm_write_file(disk, "OLD.TXT", 0, data, sizeof(data));

    cpm_error_t err = cpm_rename_file(disk, "OLD.TXT", "NEW.TXT", 0);
    if (err != CPM_OK) { cpm_close(disk); free_mem_disk(md); FAIL("rename"); }

    if (cpm_find_file(disk, "OLD.TXT", 0)) {
        cpm_close(disk); free_mem_disk(md); FAIL("alter Name noch da"); }
    if (!cpm_find_file(disk, "NEW.TXT", 0)) {
        cpm_close(disk); free_mem_disk(md); FAIL("neuer Name fehlt"); }

    /* Inhalt prüfen */
    const cpm_file_info_t *fi = cpm_find_file(disk, "NEW.TXT", 0);
    uint8_t rbuf[256];
    uint32_t br;
    cpm_read_file(disk, fi, rbuf, sizeof(rbuf), &br);
    if (memcmp(rbuf, data, sizeof(data)) != 0) {
        cpm_close(disk); free_mem_disk(md); FAIL("Inhalt geändert"); }

    cpm_close(disk);
    free_mem_disk(md);
    PASS();
}

static void test_user_numbers(void) {
    TEST("User-Nummern");

    mem_disk_t *md = create_kaypro_disk();
    cpm_geometry_t geom = {
        .sector_size = 512, .sectors_per_track = 10,
        .heads = 1, .cylinders = 40, .first_sector = 0
    };
    cpm_dpb_t dpb;
    cpm_calc_dpb(&dpb, 1024, 64, 1, &geom);

    cpm_disk_t *disk = cpm_open(&geom, &dpb, mem_read, mem_write, md);
    cpm_format(disk);
    cpm_read_directory(disk);

    uint8_t data[] = "User test";
    cpm_write_file(disk, "TEST.COM", 0, data, sizeof(data));
    cpm_write_file(disk, "TEST.COM", 1, data, sizeof(data));
    cpm_write_file(disk, "OTHER.TXT", 1, data, sizeof(data));

    if (cpm_file_count(disk) != 3) {
        cpm_close(disk); free_mem_disk(md); FAIL("Anzahl"); }

    /* User 0: nur TEST.COM */
    if (!cpm_find_file(disk, "TEST.COM", 0)) {
        cpm_close(disk); free_mem_disk(md); FAIL("U0 TEST.COM"); }
    if (cpm_find_file(disk, "OTHER.TXT", 0)) {
        cpm_close(disk); free_mem_disk(md); FAIL("U0 hat OTHER"); }

    /* User 1: TEST.COM und OTHER.TXT */
    if (!cpm_find_file(disk, "TEST.COM", 1)) {
        cpm_close(disk); free_mem_disk(md); FAIL("U1 TEST.COM"); }
    if (!cpm_find_file(disk, "OTHER.TXT", 1)) {
        cpm_close(disk); free_mem_disk(md); FAIL("U1 OTHER.TXT"); }

    /* Wildcard-User (0xFF) → findet erste */
    if (!cpm_find_file(disk, "TEST.COM", 0xFF)) {
        cpm_close(disk); free_mem_disk(md); FAIL("wildcard"); }

    cpm_close(disk);
    free_mem_disk(md);
    PASS();
}

static void test_attributes(void) {
    TEST("Datei-Attribute");

    mem_disk_t *md = create_kaypro_disk();
    cpm_geometry_t geom = {
        .sector_size = 512, .sectors_per_track = 10,
        .heads = 1, .cylinders = 40, .first_sector = 0
    };
    cpm_dpb_t dpb;
    cpm_calc_dpb(&dpb, 1024, 64, 1, &geom);

    cpm_disk_t *disk = cpm_open(&geom, &dpb, mem_read, mem_write, md);
    cpm_format(disk);
    cpm_read_directory(disk);

    uint8_t data[] = "Attr test";
    cpm_write_file(disk, "ATTR.TST", 0, data, sizeof(data));

    cpm_error_t err = cpm_set_attributes(disk, "ATTR.TST", 0, true, true, false);
    if (err != CPM_OK) { cpm_close(disk); free_mem_disk(md); FAIL("set attr"); }

    /* Directory neu lesen und prüfen */
    cpm_read_directory(disk);
    const cpm_file_info_t *fi = cpm_find_file(disk, "ATTR.TST", 0);
    if (!fi) { cpm_close(disk); free_mem_disk(md); FAIL("nicht gefunden"); }
    if (!fi->read_only) { cpm_close(disk); free_mem_disk(md); FAIL("R fehlt"); }
    if (!fi->system) { cpm_close(disk); free_mem_disk(md); FAIL("S fehlt"); }

    cpm_close(disk);
    free_mem_disk(md);
    PASS();
}

static void test_free_space(void) {
    TEST("Freier Speicher");

    mem_disk_t *md = create_kaypro_disk();
    cpm_geometry_t geom = {
        .sector_size = 512, .sectors_per_track = 10,
        .heads = 1, .cylinders = 40, .first_sector = 0
    };
    cpm_dpb_t dpb;
    cpm_calc_dpb(&dpb, 1024, 64, 1, &geom);

    cpm_disk_t *disk = cpm_open(&geom, &dpb, mem_read, mem_write, md);
    cpm_format(disk);
    cpm_read_directory(disk);

    uint32_t free1, total;
    cpm_free_space(disk, &free1, &total);
    if (total == 0) { cpm_close(disk); free_mem_disk(md); FAIL("total=0"); }
    if (free1 == 0) { cpm_close(disk); free_mem_disk(md); FAIL("free=0"); }

    /* Datei schreiben → weniger Platz */
    uint8_t data[2048];
    memset(data, 'A', sizeof(data));
    cpm_write_file(disk, "BIG.DAT", 0, data, sizeof(data));

    uint32_t free2;
    cpm_free_space(disk, &free2, NULL);
    if (free2 >= free1) { cpm_close(disk); free_mem_disk(md); FAIL("Platz nicht reduziert"); }

    cpm_close(disk);
    free_mem_disk(md);
    PASS();
}

static void test_timestamps(void) {
    TEST("Timestamp-Konvertierung");

    cpm_timestamp_t ts;
    char buf[20];

    /* 1. Januar 1978 = Tag 1 */
    cpm_make_timestamp(&ts, 1978, 1, 1, 12, 30);
    if (ts.days != 1) FAIL("Tag 1");
    cpm_format_timestamp(&ts, buf, sizeof(buf));
    if (strcmp(buf, "1978-01-01 12:30") != 0) FAIL(buf);

    /* 1. Januar 1979 = Tag 366 */
    cpm_make_timestamp(&ts, 1979, 1, 1, 0, 0);
    if (ts.days != 366) FAIL("Tag 366");

    /* Ungültiger Timestamp */
    ts.valid = false;
    ts.days = 0;
    cpm_format_timestamp(&ts, buf, sizeof(buf));
    if (strcmp(buf, "---") != 0) FAIL("ungültig");

    PASS();
}

static void test_listing(void) {
    TEST("Datei-Listing (Smoke)");

    mem_disk_t *md = create_kaypro_disk();
    cpm_geometry_t geom = {
        .sector_size = 512, .sectors_per_track = 10,
        .heads = 1, .cylinders = 40, .first_sector = 0
    };
    cpm_dpb_t dpb;
    cpm_calc_dpb(&dpb, 1024, 64, 1, &geom);

    cpm_disk_t *disk = cpm_open(&geom, &dpb, mem_read, mem_write, md);
    cpm_format(disk);
    cpm_read_directory(disk);

    uint8_t data[] = "Test";
    cpm_write_file(disk, "A.COM", 0, data, sizeof(data));
    cpm_write_file(disk, "B.TXT", 0, data, sizeof(data));

    FILE *null_fp = fopen("/dev/null", "w");
    if (null_fp) {
        cpm_list_files(disk, null_fp, 0xFF, true);
        cpm_print_info(disk, null_fp);
        cpm_print_allocation(disk, null_fp);
        fclose(null_fp);
    }

    cpm_close(disk);
    free_mem_disk(md);
    PASS();
}

static void test_persist(void) {
    TEST("Persistenz (Write + Reopen)");

    mem_disk_t *md = create_kaypro_disk();
    cpm_geometry_t geom = {
        .sector_size = 512, .sectors_per_track = 10,
        .heads = 1, .cylinders = 40, .first_sector = 0
    };
    cpm_dpb_t dpb;
    cpm_calc_dpb(&dpb, 1024, 64, 1, &geom);

    /* Phase 1: Dateien schreiben */
    {
        cpm_disk_t *disk = cpm_open(&geom, &dpb, mem_read, mem_write, md);
        cpm_format(disk);
        cpm_read_directory(disk);

        uint8_t data1[] = "Persistent data file 1";
        uint8_t data2[] = "Second persistent file";
        cpm_write_file(disk, "PERS1.TXT", 0, data1, sizeof(data1));
        cpm_write_file(disk, "PERS2.DAT", 0, data2, sizeof(data2));

        cpm_close(disk);  /* Sync + Close */
    }

    /* Phase 2: Disk erneut öffnen und lesen */
    {
        cpm_disk_t *disk = cpm_open(&geom, &dpb, mem_read, NULL, md);
        cpm_error_t err = cpm_read_directory(disk);
        if (err != CPM_OK) {
            cpm_close(disk); free_mem_disk(md);
            FAIL("readdir nach reopen");
        }

        if (cpm_file_count(disk) != 2) {
            cpm_close(disk); free_mem_disk(md);
            FAIL("falsche Dateianzahl");
        }

        const cpm_file_info_t *fi = cpm_find_file(disk, "PERS1.TXT", 0);
        if (!fi) { cpm_close(disk); free_mem_disk(md); FAIL("PERS1 fehlt"); }

        uint8_t rbuf[256];
        uint32_t br;
        cpm_read_file(disk, fi, rbuf, sizeof(rbuf), &br);
        if (memcmp(rbuf, "Persistent data file 1", 22) != 0) {
            cpm_close(disk); free_mem_disk(md);
            FAIL("Inhalt falsch");
        }

        cpm_close(disk);
    }

    free_mem_disk(md);
    PASS();
}

static void test_error_strings(void) {
    TEST("Fehler-Strings");

    if (strlen(cpm_error_str(CPM_OK)) == 0) FAIL("OK");
    if (strlen(cpm_error_str(CPM_ERR_NOT_FOUND)) == 0) FAIL("NOT_FOUND");
    if (strlen(cpm_error_str(CPM_ERR_DISK_FULL)) == 0) FAIL("DISK_FULL");

    PASS();
}


/* =============================================================================
 * Main
 * ========================================================================== */

static void run_tests(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║        CP/M DATEISYSTEM - TEST SUITE                ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    printf("── Grundlagen ─────────────────────────────────────────\n");
    test_dpb_calc();
    test_name_parse();
    test_name_format();
    test_error_strings();
    test_timestamps();

    printf("\n── Disk-Operationen ───────────────────────────────────\n");
    test_open_close();
    test_format_and_read();
    test_free_space();

    printf("\n── Datei-Operationen ──────────────────────────────────\n");
    test_write_read_file();
    test_multiple_files();
    test_delete_file();
    test_rename_file();
    test_user_numbers();
    test_attributes();

    printf("\n── Integration ────────────────────────────────────────\n");
    test_listing();
    test_persist();

    printf("\n══════════════════════════════════════════════════════\n");
    printf("  Ergebnis: %d/%d Tests bestanden", tests_passed, tests_run);
    if (tests_failed > 0) printf(", %d FEHLGESCHLAGEN", tests_failed);
    printf("\n══════════════════════════════════════════════════════\n\n");
}

static void print_usage(const char *prog) {
    printf("Verwendung: %s [Befehl] [Argumente]\n\n", prog);
    printf("Befehle:\n");
    printf("  test                        Tests ausführen\n");
    printf("  info <image> <format>       Disk-Info anzeigen\n");
    printf("  dir <image> <format>        Directory auflisten\n");
    printf("  extract <image> <format> <file> [dest]  Datei extrahieren\n");
    printf("  alloc <image> <format>      Allokations-Map anzeigen\n");
    printf("\nFormate: kaypro2, kaypro4, amstrad, osborne1, ibm8ss\n");
}

/**
 * Vordefinierte Formate
 */
typedef struct {
    const char *name;
    cpm_geometry_t geom;
    uint16_t block_size;
    uint16_t dir_entries;
    uint16_t off;
} preset_format_t;

static const preset_format_t presets[] = {
    {"kaypro2",  {512, 10, 1, 40, 0, 0, NULL, 0}, 1024, 64, 1},
    {"kaypro4",  {512, 10, 2, 40, 0, 0, NULL, 0}, 2048, 64, 1},
    {"amstrad",  {512,  9, 1, 40, 0x41, 0, NULL, 0}, 1024, 64, 2},
    {"osborne1", {1024, 5, 1, 40, 1, 0, NULL, 0}, 1024, 64, 3},
    {"ibm8ss",   {128, 26, 1, 77, 1, 6, NULL, 0}, 1024, 64, 2},
    {"pcw720",   {512,  9, 2, 80, 1, 0, NULL, 0}, 2048, 128, 1},
    {"c128",     {512, 10, 2, 80, 0, 0, NULL, 0}, 2048, 128, 2},
    {NULL, {0}, 0, 0, 0}
};

static const preset_format_t *find_preset(const char *name) {
    for (int i = 0; presets[i].name; i++) {
        if (strcasecmp(presets[i].name, name) == 0)
            return &presets[i];
    }
    return NULL;
}

/** Image-Datei öffnen als CP/M-Disk */
typedef struct {
    FILE *fp;
    uint16_t sector_size;
    uint8_t sectors_per_track;
    uint8_t heads;
    uint8_t first_sector;
} file_disk_t;

static cpm_error_t file_read(void *ctx, uint16_t cyl, uint8_t head,
                               uint8_t sector, uint8_t *buf, uint16_t *br)
{
    file_disk_t *fd = (file_disk_t *)ctx;
    uint32_t sec_idx = sector - fd->first_sector;
    uint32_t abs = (uint32_t)cyl * fd->heads * fd->sectors_per_track +
                   (uint32_t)head * fd->sectors_per_track + sec_idx;
    uint32_t offset = abs * fd->sector_size;

    if (fseek(fd->fp, offset, SEEK_SET) != 0) return CPM_ERR_READ;
    size_t r = fread(buf, 1, fd->sector_size, fd->fp);
    if (r != fd->sector_size) return CPM_ERR_READ;
    *br = fd->sector_size;
    return CPM_OK;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        run_tests();
        return tests_failed > 0 ? 1 : 0;
    }

    if (strcmp(argv[1], "test") == 0) {
        run_tests();
        return tests_failed > 0 ? 1 : 0;
    }

    /* Befehle die Image + Format benötigen */
    if (argc < 4) {
        print_usage(argv[0]);
        return 1;
    }

    const char *image_path = argv[2];
    const char *format_name = argv[3];

    const preset_format_t *preset = find_preset(format_name);
    if (!preset) {
        fprintf(stderr, "Unbekanntes Format: %s\n", format_name);
        fprintf(stderr, "Verfügbar: ");
        for (int i = 0; presets[i].name; i++)
            fprintf(stderr, "%s ", presets[i].name);
        fprintf(stderr, "\n");
        return 1;
    }

    file_disk_t fd = {0};
    fd.fp = fopen(image_path, "rb");
    if (!fd.fp) {
        fprintf(stderr, "Kann %s nicht öffnen\n", image_path);
        return 1;
    }
    fd.sector_size = preset->geom.sector_size;
    fd.sectors_per_track = preset->geom.sectors_per_track;
    fd.heads = preset->geom.heads;
    fd.first_sector = preset->geom.first_sector;

    cpm_dpb_t dpb;
    cpm_calc_dpb(&dpb, preset->block_size, preset->dir_entries,
                  preset->off, &preset->geom);

    cpm_disk_t *disk = cpm_open(&preset->geom, &dpb, file_read, NULL, &fd);
    if (!disk) {
        fprintf(stderr, "Fehler beim Öffnen\n");
        fclose(fd.fp);
        return 1;
    }

    cpm_error_t err = cpm_read_directory(disk);
    if (err != CPM_OK) {
        fprintf(stderr, "Directory-Fehler: %s\n", cpm_error_str(err));
        cpm_close(disk);
        fclose(fd.fp);
        return 1;
    }

    if (strcmp(argv[1], "info") == 0) {
        cpm_print_info(disk, stdout);
    }
    else if (strcmp(argv[1], "dir") == 0) {
        cpm_list_files(disk, stdout, 0xFF, true);
    }
    else if (strcmp(argv[1], "alloc") == 0) {
        cpm_print_allocation(disk, stdout);
    }
    else if (strcmp(argv[1], "extract") == 0 && argc >= 5) {
        const cpm_file_info_t *fi = cpm_find_file(disk, argv[4], 0xFF);
        if (!fi) {
            fprintf(stderr, "Datei nicht gefunden: %s\n", argv[4]);
        } else {
            const char *dest = argc >= 6 ? argv[5] : fi->name;
            err = cpm_extract_file(disk, fi, dest);
            if (err != CPM_OK)
                fprintf(stderr, "Fehler: %s\n", cpm_error_str(err));
            else
                printf("Extrahiert: %s → %s (%u Bytes)\n",
                       fi->name, dest, fi->size);
        }
    }
    else {
        print_usage(argv[0]);
    }

    cpm_close(disk);
    fclose(fd.fp);
    return 0;
}
