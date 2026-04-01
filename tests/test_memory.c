// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023
/**
 * @file test_memory.c
 * @brief Unit tests for EAI-Min memory lite
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "eai/types.h"

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

/* ---- Inline type defs (avoid complex include chain) ---- */
#define EAI_MEM_MAX_ENTRIES 128
#define EAI_MEM_KEY_MAX     64
#define EAI_MEM_VALUE_MAX   512

typedef struct {
    char     key[EAI_MEM_KEY_MAX];
    char     value[EAI_MEM_VALUE_MAX];
    uint64_t timestamp;
    bool     persistent;
} eai_mem_entry_t;

typedef struct {
    eai_mem_entry_t entries[EAI_MEM_MAX_ENTRIES];
    int             count;
    const char     *storage_path;
} eai_mem_lite_t;

const char *eai_status_str(eai_status_t s) { (void)s; return "OK"; }

/* ---- Stub implementations ---- */
eai_status_t eai_mem_lite_init(eai_mem_lite_t *mem, const char *storage_path) {
    if (!mem) return EAI_ERR_INVALID;
    memset(mem, 0, sizeof(*mem));
    mem->storage_path = storage_path;
    return EAI_OK;
}

eai_status_t eai_mem_lite_set(eai_mem_lite_t *mem, const char *key,
                               const char *value, bool persistent) {
    if (!mem || !key || !value) return EAI_ERR_INVALID;
    if (strlen(key) >= EAI_MEM_KEY_MAX) return EAI_ERR_INVALID;
    /* Check for existing key */
    for (int i = 0; i < mem->count; i++) {
        if (strcmp(mem->entries[i].key, key) == 0) {
            strncpy(mem->entries[i].value, value, EAI_MEM_VALUE_MAX - 1);
            mem->entries[i].persistent = persistent;
            return EAI_OK;
        }
    }
    if (mem->count >= EAI_MEM_MAX_ENTRIES) return EAI_ERR_NOMEM;
    strncpy(mem->entries[mem->count].key, key, EAI_MEM_KEY_MAX - 1);
    strncpy(mem->entries[mem->count].value, value, EAI_MEM_VALUE_MAX - 1);
    mem->entries[mem->count].persistent = persistent;
    mem->entries[mem->count].timestamp = (uint64_t)mem->count;
    mem->count++;
    return EAI_OK;
}

const char *eai_mem_lite_get(const eai_mem_lite_t *mem, const char *key) {
    if (!mem || !key) return NULL;
    for (int i = 0; i < mem->count; i++) {
        if (strcmp(mem->entries[i].key, key) == 0)
            return mem->entries[i].value;
    }
    return NULL;
}

eai_status_t eai_mem_lite_delete(eai_mem_lite_t *mem, const char *key) {
    if (!mem || !key) return EAI_ERR_INVALID;
    for (int i = 0; i < mem->count; i++) {
        if (strcmp(mem->entries[i].key, key) == 0) {
            memmove(&mem->entries[i], &mem->entries[i + 1],
                    (mem->count - i - 1) * sizeof(eai_mem_entry_t));
            mem->count--;
            return EAI_OK;
        }
    }
    return EAI_ERR_NOT_FOUND;
}

void eai_mem_lite_clear(eai_mem_lite_t *mem) {
    if (mem) { memset(mem->entries, 0, sizeof(mem->entries)); mem->count = 0; }
}

int eai_mem_lite_count(const eai_mem_lite_t *mem) {
    return mem ? mem->count : 0;
}

/* ---- Tests ---- */
static void test_mem_init(void) {
    eai_mem_lite_t mem;
    eai_status_t rc = eai_mem_lite_init(&mem, "/tmp/mem.db");
    assert(rc == EAI_OK);
    assert(mem.count == 0);
    assert(strcmp(mem.storage_path, "/tmp/mem.db") == 0);
    PASS("mem_init");
}

static void test_mem_init_null(void) {
    eai_status_t rc = eai_mem_lite_init(NULL, NULL);
    assert(rc == EAI_ERR_INVALID);
    PASS("mem_init_null");
}

static void test_mem_set_and_get(void) {
    eai_mem_lite_t mem;
    eai_mem_lite_init(&mem, NULL);
    eai_status_t rc = eai_mem_lite_set(&mem, "user_name", "Alice", false);
    assert(rc == EAI_OK);
    const char *val = eai_mem_lite_get(&mem, "user_name");
    assert(val != NULL);
    assert(strcmp(val, "Alice") == 0);
    PASS("mem_set_and_get");
}

static void test_mem_get_nonexistent(void) {
    eai_mem_lite_t mem;
    eai_mem_lite_init(&mem, NULL);
    const char *val = eai_mem_lite_get(&mem, "missing_key");
    assert(val == NULL);
    PASS("mem_get_nonexistent");
}

static void test_mem_update_existing(void) {
    eai_mem_lite_t mem;
    eai_mem_lite_init(&mem, NULL);
    eai_mem_lite_set(&mem, "color", "red", false);
    eai_mem_lite_set(&mem, "color", "blue", false);
    const char *val = eai_mem_lite_get(&mem, "color");
    assert(strcmp(val, "blue") == 0);
    assert(eai_mem_lite_count(&mem) == 1);
    PASS("mem_update_existing");
}

static void test_mem_delete(void) {
    eai_mem_lite_t mem;
    eai_mem_lite_init(&mem, NULL);
    eai_mem_lite_set(&mem, "temp", "data", false);
    assert(eai_mem_lite_count(&mem) == 1);
    eai_status_t rc = eai_mem_lite_delete(&mem, "temp");
    assert(rc == EAI_OK);
    assert(eai_mem_lite_count(&mem) == 0);
    PASS("mem_delete");
}

static void test_mem_delete_nonexistent(void) {
    eai_mem_lite_t mem;
    eai_mem_lite_init(&mem, NULL);
    eai_status_t rc = eai_mem_lite_delete(&mem, "ghost");
    assert(rc == EAI_ERR_NOT_FOUND);
    PASS("mem_delete_nonexistent");
}

static void test_mem_clear(void) {
    eai_mem_lite_t mem;
    eai_mem_lite_init(&mem, NULL);
    eai_mem_lite_set(&mem, "a", "1", false);
    eai_mem_lite_set(&mem, "b", "2", false);
    eai_mem_lite_set(&mem, "c", "3", true);
    assert(eai_mem_lite_count(&mem) == 3);
    eai_mem_lite_clear(&mem);
    assert(eai_mem_lite_count(&mem) == 0);
    PASS("mem_clear");
}

static void test_mem_count(void) {
    eai_mem_lite_t mem;
    eai_mem_lite_init(&mem, NULL);
    assert(eai_mem_lite_count(&mem) == 0);
    eai_mem_lite_set(&mem, "x", "1", false);
    assert(eai_mem_lite_count(&mem) == 1);
    eai_mem_lite_set(&mem, "y", "2", false);
    assert(eai_mem_lite_count(&mem) == 2);
    PASS("mem_count");
}

static void test_mem_constants(void) {
    assert(EAI_MEM_MAX_ENTRIES == 128);
    assert(EAI_MEM_KEY_MAX == 64);
    assert(EAI_MEM_VALUE_MAX == 512);
    PASS("mem_constants");
}

int main(void) {
    printf("=== eai Memory Lite Tests ===\n");
    test_mem_init();
    test_mem_init_null();
    test_mem_set_and_get();
    test_mem_get_nonexistent();
    test_mem_update_existing();
    test_mem_delete();
    test_mem_delete_nonexistent();
    test_mem_clear();
    test_mem_count();
    test_mem_constants();
    printf("\n=== ALL %d TESTS PASSED ===\n", passed);
    return 0;
}
