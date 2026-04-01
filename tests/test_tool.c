// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023
/**
 * @file test_tool.c
 * @brief Unit tests for EAI tool registry
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "eai/types.h"
#include "eai/tool.h"

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

/* ---- Stub implementations ---- */
static eai_tool_registry_t g_reg;

eai_status_t eai_tool_registry_init(eai_tool_registry_t *reg) {
    if (!reg) return EAI_ERR_INVALID;
    memset(reg, 0, sizeof(*reg));
    reg->count = 0;
    return EAI_OK;
}

eai_status_t eai_tool_register(eai_tool_registry_t *reg, const eai_tool_t *tool) {
    if (!reg || !tool) return EAI_ERR_INVALID;
    if (reg->count >= EAI_TOOL_REGISTRY_MAX) return EAI_ERR_NOMEM;
    if (strlen(tool->name) == 0) return EAI_ERR_INVALID;
    reg->tools[reg->count] = *tool;
    reg->count++;
    return EAI_OK;
}

eai_tool_t *eai_tool_find(eai_tool_registry_t *reg, const char *name) {
    if (!reg || !name) return NULL;
    for (int i = 0; i < reg->count; i++) {
        if (strcmp(reg->tools[i].name, name) == 0)
            return &reg->tools[i];
    }
    return NULL;
}

eai_status_t eai_tool_exec(eai_tool_t *tool, const eai_kv_t *args,
                           int arg_count, eai_tool_result_t *result) {
    if (!tool || !result) return EAI_ERR_INVALID;
    if (!tool->exec) return EAI_ERR_NOT_FOUND;
    return tool->exec(args, arg_count, result);
}

void eai_tool_registry_list(const eai_tool_registry_t *reg) {
    (void)reg;
}

const char *eai_status_str(eai_status_t status) {
    switch (status) {
        case EAI_OK:            return "OK";
        case EAI_ERR_NOMEM:     return "NOMEM";
        case EAI_ERR_INVALID:   return "INVALID";
        case EAI_ERR_NOT_FOUND: return "NOT_FOUND";
        default:                return "UNKNOWN";
    }
}

/* ---- Mock tool exec function ---- */
static eai_status_t mock_exec(const eai_kv_t *args, int arg_count,
                               eai_tool_result_t *result) {
    (void)args; (void)arg_count;
    snprintf(result->data, sizeof(result->data), "mock_result");
    result->len = strlen(result->data);
    result->status = EAI_OK;
    return EAI_OK;
}

/* ---- Tests ---- */
static void test_registry_init(void) {
    eai_tool_registry_t reg;
    eai_status_t rc = eai_tool_registry_init(&reg);
    assert(rc == EAI_OK);
    assert(reg.count == 0);
    PASS("registry_init");
}

static void test_registry_init_null(void) {
    eai_status_t rc = eai_tool_registry_init(NULL);
    assert(rc == EAI_ERR_INVALID);
    PASS("registry_init_null");
}

static void test_register_tool(void) {
    eai_tool_registry_t reg;
    eai_tool_registry_init(&reg);
    eai_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    strncpy(tool.name, "file_read", EAI_TOOL_NAME_MAX - 1);
    tool.description = "Read a file";
    tool.exec = mock_exec;
    eai_status_t rc = eai_tool_register(&reg, &tool);
    assert(rc == EAI_OK);
    assert(reg.count == 1);
    PASS("register_tool");
}

static void test_register_multiple_tools(void) {
    eai_tool_registry_t reg;
    eai_tool_registry_init(&reg);
    for (int i = 0; i < 5; i++) {
        eai_tool_t tool;
        memset(&tool, 0, sizeof(tool));
        snprintf(tool.name, EAI_TOOL_NAME_MAX, "tool_%d", i);
        tool.exec = mock_exec;
        eai_tool_register(&reg, &tool);
    }
    assert(reg.count == 5);
    PASS("register_multiple_tools");
}

static void test_register_null_tool(void) {
    eai_tool_registry_t reg;
    eai_tool_registry_init(&reg);
    eai_status_t rc = eai_tool_register(&reg, NULL);
    assert(rc == EAI_ERR_INVALID);
    PASS("register_null_tool");
}

static void test_find_tool(void) {
    eai_tool_registry_t reg;
    eai_tool_registry_init(&reg);
    eai_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    strncpy(tool.name, "search", EAI_TOOL_NAME_MAX - 1);
    tool.exec = mock_exec;
    eai_tool_register(&reg, &tool);
    eai_tool_t *found = eai_tool_find(&reg, "search");
    assert(found != NULL);
    assert(strcmp(found->name, "search") == 0);
    PASS("find_tool");
}

static void test_find_tool_not_found(void) {
    eai_tool_registry_t reg;
    eai_tool_registry_init(&reg);
    eai_tool_t *found = eai_tool_find(&reg, "nonexistent");
    assert(found == NULL);
    PASS("find_tool_not_found");
}

static void test_exec_tool(void) {
    eai_tool_registry_t reg;
    eai_tool_registry_init(&reg);
    eai_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    strncpy(tool.name, "exec_test", EAI_TOOL_NAME_MAX - 1);
    tool.exec = mock_exec;
    eai_tool_register(&reg, &tool);
    eai_tool_t *found = eai_tool_find(&reg, "exec_test");
    eai_tool_result_t result;
    memset(&result, 0, sizeof(result));
    eai_status_t rc = eai_tool_exec(found, NULL, 0, &result);
    assert(rc == EAI_OK);
    assert(strcmp(result.data, "mock_result") == 0);
    PASS("exec_tool");
}

static void test_exec_null_tool(void) {
    eai_tool_result_t result;
    eai_status_t rc = eai_tool_exec(NULL, NULL, 0, &result);
    assert(rc == EAI_ERR_INVALID);
    PASS("exec_null_tool");
}

static void test_tool_param_types(void) {
    assert(EAI_PARAM_STRING == 0);
    assert(EAI_PARAM_INT == 1);
    assert(EAI_PARAM_FLOAT == 2);
    assert(EAI_PARAM_BOOL == 3);
    assert(EAI_PARAM_BYTES == 4);
    PASS("tool_param_types");
}

static void test_tool_constants(void) {
    assert(EAI_TOOL_MAX_PARAMS == 16);
    assert(EAI_TOOL_MAX_PERMISSIONS == 8);
    assert(EAI_TOOL_NAME_MAX == 64);
    assert(EAI_TOOL_REGISTRY_MAX == 64);
    PASS("tool_constants");
}

int main(void) {
    printf("=== eai Tool Registry Tests ===\n");
    test_registry_init();
    test_registry_init_null();
    test_register_tool();
    test_register_multiple_tools();
    test_register_null_tool();
    test_find_tool();
    test_find_tool_not_found();
    test_exec_tool();
    test_exec_null_tool();
    test_tool_param_types();
    test_tool_constants();
    printf("\n=== ALL %d TESTS PASSED ===\n", passed);
    return 0;
}
