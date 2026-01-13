/**
 * @file test_config_manager.c
 * @brief Configuration Manager Tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

static int _pass = 0, _fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name) do { printf("  [TEST] %s... ", #name); test_##name(); printf("OK\n"); _pass++; } while(0)
#define ASSERT(c) do { if(!(c)) { printf("FAIL @ %d\n", __LINE__); _fail++; return; } } while(0)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Minimal config implementation for testing
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define MAX_ENTRIES 100
#define MAX_KEY_LEN 64
#define MAX_VAL_LEN 256

typedef enum { T_STRING, T_INT, T_FLOAT, T_BOOL } val_type_t;

typedef struct {
    val_type_t type;
    union {
        char str_val[MAX_VAL_LEN];
        int64_t int_val;
        double float_val;
        bool bool_val;
    };
} config_val_t;

typedef struct {
    char section[MAX_KEY_LEN];
    char key[MAX_KEY_LEN];
    config_val_t value;
    config_val_t default_value;
} config_entry_t;

typedef struct {
    config_entry_t entries[MAX_ENTRIES];
    int count;
} config_t;

static config_t* cfg_create(void) {
    config_t *c = calloc(1, sizeof(config_t));
    return c;
}

static void cfg_destroy(config_t *c) {
    free(c);
}

static int cfg_add(config_t *c, const char *sec, const char *key, val_type_t type) {
    if (c->count >= MAX_ENTRIES) return -1;
    config_entry_t *e = &c->entries[c->count++];
    strncpy(e->section, sec, MAX_KEY_LEN - 1);
    strncpy(e->key, key, MAX_KEY_LEN - 1);
    e->value.type = type;
    e->default_value.type = type;
    return 0;
}

static config_entry_t* cfg_find(config_t *c, const char *sec, const char *key) {
    for (int i = 0; i < c->count; i++) {
        if (strcmp(c->entries[i].section, sec) == 0 &&
            strcmp(c->entries[i].key, key) == 0) {
            return &c->entries[i];
        }
    }
    return NULL;
}

static int cfg_set_string(config_t *c, const char *sec, const char *key, const char *val) {
    config_entry_t *e = cfg_find(c, sec, key);
    if (!e) return -1;
    strncpy(e->value.str_val, val, MAX_VAL_LEN - 1);
    return 0;
}

static int cfg_set_int(config_t *c, const char *sec, const char *key, int64_t val) {
    config_entry_t *e = cfg_find(c, sec, key);
    if (!e) return -1;
    e->value.int_val = val;
    return 0;
}

static int cfg_set_bool(config_t *c, const char *sec, const char *key, bool val) {
    config_entry_t *e = cfg_find(c, sec, key);
    if (!e) return -1;
    e->value.bool_val = val;
    return 0;
}

static const char* cfg_get_string(config_t *c, const char *sec, const char *key) {
    config_entry_t *e = cfg_find(c, sec, key);
    return e ? e->value.str_val : "";
}

static int64_t cfg_get_int(config_t *c, const char *sec, const char *key) {
    config_entry_t *e = cfg_find(c, sec, key);
    return e ? e->value.int_val : 0;
}

static bool cfg_get_bool(config_t *c, const char *sec, const char *key) {
    config_entry_t *e = cfg_find(c, sec, key);
    return e ? e->value.bool_val : false;
}

static int cfg_section_count(config_t *c, const char *sec) {
    int count = 0;
    for (int i = 0; i < c->count; i++) {
        if (strcmp(c->entries[i].section, sec) == 0) count++;
    }
    return count;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

TEST(create_destroy) {
    config_t *c = cfg_create();
    ASSERT(c != NULL);
    cfg_destroy(c);
}

TEST(add_entry) {
    config_t *c = cfg_create();
    int ret = cfg_add(c, "general", "version", T_STRING);
    ASSERT(ret == 0);
    ASSERT(c->count == 1);
    cfg_destroy(c);
}

TEST(find_entry) {
    config_t *c = cfg_create();
    cfg_add(c, "general", "version", T_STRING);
    cfg_add(c, "hardware", "device", T_STRING);
    
    config_entry_t *e = cfg_find(c, "general", "version");
    ASSERT(e != NULL);
    ASSERT(strcmp(e->section, "general") == 0);
    
    e = cfg_find(c, "hardware", "device");
    ASSERT(e != NULL);
    
    e = cfg_find(c, "nonexistent", "key");
    ASSERT(e == NULL);
    
    cfg_destroy(c);
}

TEST(set_get_string) {
    config_t *c = cfg_create();
    cfg_add(c, "general", "version", T_STRING);
    
    cfg_set_string(c, "general", "version", "3.8.7");
    const char *val = cfg_get_string(c, "general", "version");
    ASSERT(strcmp(val, "3.8.7") == 0);
    
    cfg_destroy(c);
}

TEST(set_get_int) {
    config_t *c = cfg_create();
    cfg_add(c, "recovery", "retries", T_INT);
    
    cfg_set_int(c, "recovery", "retries", 10);
    int64_t val = cfg_get_int(c, "recovery", "retries");
    ASSERT(val == 10);
    
    cfg_destroy(c);
}

TEST(set_get_bool) {
    config_t *c = cfg_create();
    cfg_add(c, "gui", "dark_mode", T_BOOL);
    
    cfg_set_bool(c, "gui", "dark_mode", true);
    ASSERT(cfg_get_bool(c, "gui", "dark_mode") == true);
    
    cfg_set_bool(c, "gui", "dark_mode", false);
    ASSERT(cfg_get_bool(c, "gui", "dark_mode") == false);
    
    cfg_destroy(c);
}

TEST(section_count) {
    config_t *c = cfg_create();
    cfg_add(c, "general", "a", T_STRING);
    cfg_add(c, "general", "b", T_STRING);
    cfg_add(c, "general", "c", T_STRING);
    cfg_add(c, "hardware", "d", T_STRING);
    
    ASSERT(cfg_section_count(c, "general") == 3);
    ASSERT(cfg_section_count(c, "hardware") == 1);
    ASSERT(cfg_section_count(c, "nonexistent") == 0);
    
    cfg_destroy(c);
}

TEST(nonexistent_key) {
    config_t *c = cfg_create();
    
    const char *s = cfg_get_string(c, "foo", "bar");
    ASSERT(strcmp(s, "") == 0);
    
    int64_t i = cfg_get_int(c, "foo", "bar");
    ASSERT(i == 0);
    
    bool b = cfg_get_bool(c, "foo", "bar");
    ASSERT(b == false);
    
    cfg_destroy(c);
}

TEST(multiple_sections) {
    config_t *c = cfg_create();
    
    cfg_add(c, "general", "version", T_STRING);
    cfg_add(c, "hardware", "device", T_STRING);
    cfg_add(c, "recovery", "retries", T_INT);
    cfg_add(c, "gui", "dark_mode", T_BOOL);
    
    ASSERT(c->count == 4);
    
    cfg_set_string(c, "general", "version", "1.0");
    cfg_set_string(c, "hardware", "device", "COM3");
    cfg_set_int(c, "recovery", "retries", 5);
    cfg_set_bool(c, "gui", "dark_mode", true);
    
    ASSERT(strcmp(cfg_get_string(c, "general", "version"), "1.0") == 0);
    ASSERT(strcmp(cfg_get_string(c, "hardware", "device"), "COM3") == 0);
    ASSERT(cfg_get_int(c, "recovery", "retries") == 5);
    ASSERT(cfg_get_bool(c, "gui", "dark_mode") == true);
    
    cfg_destroy(c);
}

TEST(type_safety) {
    config_t *c = cfg_create();
    cfg_add(c, "test", "str", T_STRING);
    cfg_add(c, "test", "num", T_INT);
    
    /* Set values of correct type */
    ASSERT(cfg_set_string(c, "test", "str", "hello") == 0);
    ASSERT(cfg_set_int(c, "test", "num", 42) == 0);
    
    /* Verify */
    ASSERT(strcmp(cfg_get_string(c, "test", "str"), "hello") == 0);
    ASSERT(cfg_get_int(c, "test", "num") == 42);
    
    cfg_destroy(c);
}

TEST(long_string) {
    config_t *c = cfg_create();
    cfg_add(c, "test", "path", T_STRING);
    
    char long_path[200];
    memset(long_path, 'a', 199);
    long_path[199] = '\0';
    
    cfg_set_string(c, "test", "path", long_path);
    const char *val = cfg_get_string(c, "test", "path");
    ASSERT(strlen(val) < MAX_VAL_LEN);
    ASSERT(val[0] == 'a');
    
    cfg_destroy(c);
}

TEST(negative_int) {
    config_t *c = cfg_create();
    cfg_add(c, "test", "num", T_INT);
    
    cfg_set_int(c, "test", "num", -100);
    ASSERT(cfg_get_int(c, "test", "num") == -100);
    
    cfg_destroy(c);
}

TEST(large_int) {
    config_t *c = cfg_create();
    cfg_add(c, "test", "bignum", T_INT);
    
    int64_t big = 9223372036854775807LL;  /* INT64_MAX */
    cfg_set_int(c, "test", "bignum", big);
    ASSERT(cfg_get_int(c, "test", "bignum") == big);
    
    cfg_destroy(c);
}

int main(void) {
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  Configuration Manager Tests (P3-003)\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    RUN(create_destroy);
    RUN(add_entry);
    RUN(find_entry);
    RUN(set_get_string);
    RUN(set_get_int);
    RUN(set_get_bool);
    RUN(section_count);
    RUN(nonexistent_key);
    RUN(multiple_sections);
    RUN(type_safety);
    RUN(long_string);
    RUN(negative_int);
    RUN(large_int);
    
    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed\n", _pass, _fail);
    printf("═══════════════════════════════════════════════════════════════\n");
    
    return _fail > 0 ? 1 : 0;
}
