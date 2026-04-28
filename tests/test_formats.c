// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Comprehensive tests for GGUF/ONNX loaders and format registry

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "eai/gguf.h"
#include "eai/onnx.h"

static int tests_run = 0, tests_passed = 0, tests_failed = 0;
#define TEST(name) do { tests_run++; printf("  TEST %-44s ", #name); } while(0)
#define PASS() do { tests_passed++; printf("[PASS]\n"); } while(0)
#define FAIL(msg) do { tests_failed++; printf("[FAIL] %s\n", msg); } while(0)

/* Format registry functions (defined in format_registry.c) */
typedef enum { EAI_FORMAT_UNKNOWN = 0, EAI_FORMAT_GGUF = 1, EAI_FORMAT_ONNX = 2 } eai_format_t;
extern eai_format_t eai_format_detect(const char *path);

/* Helper: create a minimal GGUF file for testing */
static const char *create_test_gguf(void)
{
    static const char *path = "test_model.gguf";
    FILE *fp = fopen(path, "wb");
    if (!fp) return NULL;

    /* GGUF header: magic(4) + version(4) + n_tensors(8) + n_kv(8) */
    uint32_t magic = 0x46554747; /* "GGUF" little-endian */
    uint32_t version = 3;
    uint64_t n_tensors = 0;
    uint64_t n_kv = 1;

    fwrite(&magic, 4, 1, fp);
    fwrite(&version, 4, 1, fp);
    fwrite(&n_tensors, 8, 1, fp);
    fwrite(&n_kv, 8, 1, fp);

    /* Write one KV pair: key="general.architecture", type=STRING, value="test" */
    const char *key = "general.architecture";
    uint64_t key_len = strlen(key);
    fwrite(&key_len, 8, 1, fp);
    fwrite(key, 1, (size_t)key_len, fp);

    uint32_t kv_type = 8; /* GGUF_TYPE_STRING */
    fwrite(&kv_type, 4, 1, fp);

    const char *val = "test";
    uint64_t val_len = strlen(val);
    fwrite(&val_len, 8, 1, fp);
    fwrite(val, 1, (size_t)val_len, fp);

    fclose(fp);
    return path;
}

/* Helper: create a minimal file that looks like ONNX (protobuf) */
static const char *create_test_onnx(void)
{
    static const char *path = "test_model.onnx";
    FILE *fp = fopen(path, "wb");
    if (!fp) return NULL;

    /* Minimal ONNX-like protobuf: field 1 (ir_version) varint = 7, padded to 4+ bytes */
    uint8_t data[] = { 0x08, 0x07, 0x10, 0x01 }; /* field 1=7, field 2=1 */
    fwrite(data, 1, sizeof(data), fp);

    fclose(fp);
    return path;
}

static void cleanup_test_files(void)
{
    remove("test_model.gguf");
    remove("test_model.onnx");
    remove("test_bad_magic.bin");
}

/* ---- GGUF loader tests ---- */

static void test_gguf_load_null_path(void)
{
    TEST(gguf_load_null_path);
    gguf_context_t ctx;
    eai_status_t st = eai_gguf_load(NULL, &ctx);
    if (st != EAI_ERR_INVALID) { FAIL("expected INVALID for NULL path"); return; }
    PASS();
}

static void test_gguf_load_null_ctx(void)
{
    TEST(gguf_load_null_ctx);
    eai_status_t st = eai_gguf_load("some_file.gguf", NULL);
    if (st != EAI_ERR_INVALID) { FAIL("expected INVALID for NULL ctx"); return; }
    PASS();
}

static void test_gguf_load_nonexistent(void)
{
    TEST(gguf_load_nonexistent);
    gguf_context_t ctx;
    eai_status_t st = eai_gguf_load("nonexistent_file_xyz.gguf", &ctx);
    if (st != EAI_ERR_IO) { FAIL("expected IO error for missing file"); return; }
    PASS();
}

static void test_gguf_load_bad_magic(void)
{
    TEST(gguf_load_bad_magic);
    /* Create a file with wrong magic */
    FILE *fp = fopen("test_bad_magic.bin", "wb");
    if (!fp) { FAIL("cannot create test file"); return; }
    uint32_t bad_magic = 0xDEADBEEF;
    fwrite(&bad_magic, 4, 1, fp);
    fclose(fp);

    gguf_context_t ctx;
    eai_status_t st = eai_gguf_load("test_bad_magic.bin", &ctx);
    if (st != EAI_ERR_FORMAT) { FAIL("expected FORMAT error for bad magic"); return; }
    PASS();
}

static void test_gguf_load_valid(void)
{
    TEST(gguf_load_valid);
    const char *path = create_test_gguf();
    if (!path) { FAIL("cannot create test GGUF"); return; }

    gguf_context_t ctx;
    eai_status_t st = eai_gguf_load(path, &ctx);
    if (st != EAI_OK) { FAIL("load failed"); return; }

    if (ctx.magic != 0x46554747) { FAIL("magic mismatch"); eai_gguf_free(&ctx); return; }
    if (ctx.version != 3) { FAIL("version != 3"); eai_gguf_free(&ctx); return; }
    if (ctx.n_tensors != 0) { FAIL("n_tensors != 0"); eai_gguf_free(&ctx); return; }
    if (ctx.n_kv != 1) { FAIL("n_kv != 1"); eai_gguf_free(&ctx); return; }

    eai_gguf_free(&ctx);
    PASS();
}

static void test_gguf_get_str(void)
{
    TEST(gguf_get_str);
    const char *path = create_test_gguf();
    if (!path) { FAIL("cannot create test GGUF"); return; }

    gguf_context_t ctx;
    eai_gguf_load(path, &ctx);

    const char *arch = eai_gguf_get_str(&ctx, "general.architecture");
    if (!arch) { FAIL("get_str returned NULL"); eai_gguf_free(&ctx); return; }
    if (strcmp(arch, "test") != 0) { FAIL("expected 'test'"); eai_gguf_free(&ctx); return; }

    /* Non-existent key should return NULL */
    const char *missing = eai_gguf_get_str(&ctx, "nonexistent.key");
    if (missing != NULL) { FAIL("expected NULL for nonexistent key"); eai_gguf_free(&ctx); return; }

    eai_gguf_free(&ctx);
    PASS();
}

static void test_gguf_get_str_null_ctx(void)
{
    TEST(gguf_get_str_null_ctx);
    const char *result = eai_gguf_get_str(NULL, "any_key");
    if (result != NULL) { FAIL("expected NULL for NULL ctx"); return; }
    PASS();
}

static void test_gguf_get_str_null_key(void)
{
    TEST(gguf_get_str_null_key);
    gguf_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    const char *result = eai_gguf_get_str(&ctx, NULL);
    if (result != NULL) { FAIL("expected NULL for NULL key"); return; }
    PASS();
}

static void test_gguf_get_int_default(void)
{
    TEST(gguf_get_int_default);
    const char *path = create_test_gguf();
    if (!path) { FAIL("cannot create test GGUF"); return; }

    gguf_context_t ctx;
    eai_gguf_load(path, &ctx);

    /* Non-existent int key should return default */
    int val = eai_gguf_get_int(&ctx, "nonexistent.int", 42);
    if (val != 42) { FAIL("expected default value 42"); eai_gguf_free(&ctx); return; }

    /* NULL ctx should return default */
    val = eai_gguf_get_int(NULL, "any", 99);
    if (val != 99) { FAIL("expected 99 for NULL ctx"); eai_gguf_free(&ctx); return; }

    eai_gguf_free(&ctx);
    PASS();
}

static void test_gguf_get_tensor_nonexistent(void)
{
    TEST(gguf_get_tensor_nonexistent);
    const char *path = create_test_gguf();
    if (!path) { FAIL("cannot create test GGUF"); return; }

    gguf_context_t ctx;
    eai_gguf_load(path, &ctx);

    eai_tensor_t tensor;
    eai_status_t st = eai_gguf_get_tensor(&ctx, "nonexistent_tensor", &tensor);
    if (st != EAI_ERR_NOT_FOUND) { FAIL("expected NOT_FOUND"); eai_gguf_free(&ctx); return; }

    eai_gguf_free(&ctx);
    PASS();
}

static void test_gguf_get_tensor_null_params(void)
{
    TEST(gguf_get_tensor_null_params);
    eai_tensor_t tensor;
    eai_status_t st = eai_gguf_get_tensor(NULL, "name", &tensor);
    if (st != EAI_ERR_INVALID) { FAIL("expected INVALID for NULL ctx"); return; }

    gguf_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    st = eai_gguf_get_tensor(&ctx, NULL, &tensor);
    if (st != EAI_ERR_INVALID) { FAIL("expected INVALID for NULL name"); return; }

    st = eai_gguf_get_tensor(&ctx, "name", NULL);
    if (st != EAI_ERR_INVALID) { FAIL("expected INVALID for NULL tensor"); return; }

    PASS();
}

static void test_gguf_free_null(void)
{
    TEST(gguf_free_null);
    eai_gguf_free(NULL); /* should not crash */
    PASS();
}

static void test_gguf_free_double(void)
{
    TEST(gguf_free_double);
    const char *path = create_test_gguf();
    if (!path) { FAIL("cannot create test GGUF"); return; }

    gguf_context_t ctx;
    eai_gguf_load(path, &ctx);
    eai_gguf_free(&ctx);
    eai_gguf_free(&ctx); /* second free should be safe */
    PASS();
}

/* ---- ONNX loader tests ---- */

static void test_onnx_load_null_path(void)
{
    TEST(onnx_load_null_path);
    onnx_context_t ctx;
    eai_status_t st = eai_onnx_load(NULL, &ctx);
    if (st != EAI_ERR_INVALID) { FAIL("expected INVALID for NULL path"); return; }
    PASS();
}

static void test_onnx_load_null_ctx(void)
{
    TEST(onnx_load_null_ctx);
    eai_status_t st = eai_onnx_load("some_file.onnx", NULL);
    if (st != EAI_ERR_INVALID) { FAIL("expected INVALID for NULL ctx"); return; }
    PASS();
}

static void test_onnx_load_nonexistent(void)
{
    TEST(onnx_load_nonexistent);
    onnx_context_t ctx;
    eai_status_t st = eai_onnx_load("nonexistent_onnx_xyz.onnx", &ctx);
    if (st != EAI_ERR_IO) { FAIL("expected IO error"); return; }
    PASS();
}

static void test_onnx_load_valid(void)
{
    TEST(onnx_load_valid);
    const char *path = create_test_onnx();
    if (!path) { FAIL("cannot create test ONNX"); return; }

    onnx_context_t ctx;
    eai_status_t st = eai_onnx_load(path, &ctx);
    if (st != EAI_OK) { FAIL("load failed"); eai_onnx_free(&ctx); return; }

    /* The test file has ir_version = 7 */
    if (ctx.ir_version != 7) { FAIL("ir_version != 7"); eai_onnx_free(&ctx); return; }

    eai_onnx_free(&ctx);
    PASS();
}

static void test_onnx_to_graph_stub(void)
{
    TEST(onnx_to_graph_stub);
    onnx_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    eai_compute_graph_t graph;
    eai_status_t st = eai_onnx_to_graph(&ctx, &graph);
    /* Currently returns NOT_IMPLEMENTED */
    if (st != EAI_ERR_NOT_IMPLEMENTED) { FAIL("expected NOT_IMPLEMENTED"); return; }
    PASS();
}

static void test_onnx_to_graph_null(void)
{
    TEST(onnx_to_graph_null);
    eai_compute_graph_t graph;
    eai_status_t st = eai_onnx_to_graph(NULL, &graph);
    if (st != EAI_ERR_INVALID) { FAIL("expected INVALID for NULL ctx"); return; }

    onnx_context_t ctx;
    st = eai_onnx_to_graph(&ctx, NULL);
    if (st != EAI_ERR_INVALID) { FAIL("expected INVALID for NULL graph"); return; }
    PASS();
}

static void test_onnx_free_null(void)
{
    TEST(onnx_free_null);
    eai_onnx_free(NULL); /* should not crash */
    PASS();
}

/* ---- Format registry tests ---- */

static void test_format_detect_null(void)
{
    TEST(format_detect_null);
    eai_format_t fmt = eai_format_detect(NULL);
    if (fmt != EAI_FORMAT_UNKNOWN) { FAIL("expected UNKNOWN for NULL"); return; }
    PASS();
}

static void test_format_detect_nonexistent(void)
{
    TEST(format_detect_nonexistent);
    eai_format_t fmt = eai_format_detect("nonexistent_xyz_file.bin");
    if (fmt != EAI_FORMAT_UNKNOWN) { FAIL("expected UNKNOWN for nonexistent file"); return; }
    PASS();
}

static void test_format_detect_gguf_by_magic(void)
{
    TEST(format_detect_gguf_by_magic);
    const char *path = create_test_gguf();
    if (!path) { FAIL("cannot create test GGUF"); return; }
    eai_format_t fmt = eai_format_detect(path);
    if (fmt != EAI_FORMAT_GGUF) { FAIL("expected GGUF"); return; }
    PASS();
}

static void test_format_detect_onnx_by_magic(void)
{
    TEST(format_detect_onnx_by_magic);
    const char *path = create_test_onnx();
    if (!path) { FAIL("cannot create test ONNX"); return; }
    eai_format_t fmt = eai_format_detect(path);
    if (fmt != EAI_FORMAT_ONNX) { FAIL("expected ONNX"); return; }
    PASS();
}

static void test_format_detect_bad_magic_with_gguf_ext(void)
{
    TEST(format_detect_bad_magic_gguf_ext);
    /* Create a file with wrong magic but .gguf extension */
    const char *path = "test_fakename.gguf";
    FILE *fp = fopen(path, "wb");
    if (!fp) { FAIL("cannot create file"); return; }
    uint8_t data[] = {0x00, 0x00, 0x00, 0x00, 0x00};
    fwrite(data, 1, sizeof(data), fp);
    fclose(fp);

    eai_format_t fmt = eai_format_detect(path);
    /* Magic doesn't match GGUF or ONNX, but extension is .gguf → should detect as GGUF */
    if (fmt != EAI_FORMAT_GGUF) { FAIL("expected GGUF by extension fallback"); remove(path); return; }
    remove(path);
    PASS();
}

int main(void)
{
    printf("=== EAI Format Tests (Comprehensive) ===\n\n");

    test_gguf_load_null_path();
    test_gguf_load_null_ctx();
    test_gguf_load_nonexistent();
    test_gguf_load_bad_magic();
    test_gguf_load_valid();
    test_gguf_get_str();
    test_gguf_get_str_null_ctx();
    test_gguf_get_str_null_key();
    test_gguf_get_int_default();
    test_gguf_get_tensor_nonexistent();
    test_gguf_get_tensor_null_params();
    test_gguf_free_null();
    test_gguf_free_double();
    test_onnx_load_null_path();
    test_onnx_load_null_ctx();
    test_onnx_load_nonexistent();
    test_onnx_load_valid();
    test_onnx_to_graph_stub();
    test_onnx_to_graph_null();
    test_onnx_free_null();
    test_format_detect_null();
    test_format_detect_nonexistent();
    test_format_detect_gguf_by_magic();
    test_format_detect_onnx_by_magic();
    test_format_detect_bad_magic_with_gguf_ext();

    cleanup_test_files();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
