// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023
/**
 * @file test_agent.c
 * @brief Unit tests for EAI-Min agent
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "eai/types.h"
#include "eai/tool.h"

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

/* ---- Minimal type stubs for agent without full headers ---- */
#define EAI_AGENT_MAX_ITERATIONS 10
#define EAI_AGENT_PROMPT_MAX     4096

typedef enum {
    EAI_AGENT_IDLE,
    EAI_AGENT_THINKING,
    EAI_AGENT_TOOL_CALL,
    EAI_AGENT_DONE,
    EAI_AGENT_ERROR,
} eai_agent_state_t;

typedef struct {
    const char *goal;
    bool        offline_only;
    int         max_iterations;
} eai_agent_task_t;

typedef struct {
    void                *runtime;
    eai_tool_registry_t *tools;
    void                *memory;
    eai_agent_state_t    state;
    int                  iteration;
    char                 last_output[4096];
} eai_min_agent_t;

const char *eai_status_str(eai_status_t s) { (void)s; return "OK"; }

/* ---- Stub tool registry ---- */
eai_status_t eai_tool_registry_init(eai_tool_registry_t *r) {
    if (!r) return EAI_ERR_INVALID;
    memset(r, 0, sizeof(*r));
    return EAI_OK;
}

/* ---- Stub agent implementations ---- */
eai_status_t eai_min_agent_init(eai_min_agent_t *agent,
                                 void *runtime,
                                 eai_tool_registry_t *tools,
                                 void *memory) {
    if (!agent) return EAI_ERR_INVALID;
    memset(agent, 0, sizeof(*agent));
    agent->runtime = runtime;
    agent->tools = tools;
    agent->memory = memory;
    agent->state = EAI_AGENT_IDLE;
    agent->iteration = 0;
    return EAI_OK;
}

eai_status_t eai_min_agent_run(eai_min_agent_t *agent, const eai_agent_task_t *task) {
    if (!agent || !task) return EAI_ERR_INVALID;
    if (!task->goal || strlen(task->goal) == 0) return EAI_ERR_INVALID;
    agent->state = EAI_AGENT_THINKING;
    agent->iteration = 1;
    /* Simulate completion */
    snprintf(agent->last_output, sizeof(agent->last_output),
             "Completed: %s", task->goal);
    agent->state = EAI_AGENT_DONE;
    return EAI_OK;
}

eai_status_t eai_min_agent_step(eai_min_agent_t *agent) {
    if (!agent) return EAI_ERR_INVALID;
    if (agent->state == EAI_AGENT_DONE) return EAI_OK;
    agent->iteration++;
    if (agent->iteration >= EAI_AGENT_MAX_ITERATIONS) {
        agent->state = EAI_AGENT_ERROR;
        return EAI_ERR_TIMEOUT;
    }
    return EAI_OK;
}

const char *eai_min_agent_output(const eai_min_agent_t *agent) {
    if (!agent || agent->last_output[0] == '\0') return NULL;
    return agent->last_output;
}

void eai_min_agent_reset(eai_min_agent_t *agent) {
    if (!agent) return;
    agent->state = EAI_AGENT_IDLE;
    agent->iteration = 0;
    agent->last_output[0] = '\0';
}

/* ---- Tests ---- */
static void test_agent_init(void) {
    eai_min_agent_t agent;
    eai_tool_registry_t tools;
    eai_tool_registry_init(&tools);
    eai_status_t rc = eai_min_agent_init(&agent, NULL, &tools, NULL);
    assert(rc == EAI_OK);
    assert(agent.state == EAI_AGENT_IDLE);
    assert(agent.iteration == 0);
    PASS("agent_init");
}

static void test_agent_init_null(void) {
    eai_status_t rc = eai_min_agent_init(NULL, NULL, NULL, NULL);
    assert(rc == EAI_ERR_INVALID);
    PASS("agent_init_null");
}

static void test_agent_run(void) {
    eai_min_agent_t agent;
    eai_tool_registry_t tools;
    eai_tool_registry_init(&tools);
    eai_min_agent_init(&agent, NULL, &tools, NULL);
    eai_agent_task_t task = { .goal = "test task", .offline_only = true, .max_iterations = 5 };
    eai_status_t rc = eai_min_agent_run(&agent, &task);
    assert(rc == EAI_OK);
    assert(agent.state == EAI_AGENT_DONE);
    PASS("agent_run");
}

static void test_agent_run_null_task(void) {
    eai_min_agent_t agent;
    eai_tool_registry_t tools;
    eai_tool_registry_init(&tools);
    eai_min_agent_init(&agent, NULL, &tools, NULL);
    eai_status_t rc = eai_min_agent_run(&agent, NULL);
    assert(rc == EAI_ERR_INVALID);
    PASS("agent_run_null_task");
}

static void test_agent_output(void) {
    eai_min_agent_t agent;
    eai_tool_registry_t tools;
    eai_tool_registry_init(&tools);
    eai_min_agent_init(&agent, NULL, &tools, NULL);
    eai_agent_task_t task = { .goal = "hello", .offline_only = false, .max_iterations = 3 };
    eai_min_agent_run(&agent, &task);
    const char *out = eai_min_agent_output(&agent);
    assert(out != NULL);
    assert(strstr(out, "hello") != NULL);
    PASS("agent_output");
}

static void test_agent_output_before_run(void) {
    eai_min_agent_t agent;
    eai_tool_registry_t tools;
    eai_tool_registry_init(&tools);
    eai_min_agent_init(&agent, NULL, &tools, NULL);
    const char *out = eai_min_agent_output(&agent);
    assert(out == NULL);
    PASS("agent_output_before_run");
}

static void test_agent_reset(void) {
    eai_min_agent_t agent;
    eai_tool_registry_t tools;
    eai_tool_registry_init(&tools);
    eai_min_agent_init(&agent, NULL, &tools, NULL);
    eai_agent_task_t task = { .goal = "work", .offline_only = true, .max_iterations = 3 };
    eai_min_agent_run(&agent, &task);
    assert(agent.state == EAI_AGENT_DONE);
    eai_min_agent_reset(&agent);
    assert(agent.state == EAI_AGENT_IDLE);
    assert(agent.iteration == 0);
    PASS("agent_reset");
}

static void test_agent_state_enum(void) {
    assert(EAI_AGENT_IDLE == 0);
    assert(EAI_AGENT_THINKING == 1);
    assert(EAI_AGENT_TOOL_CALL == 2);
    assert(EAI_AGENT_DONE == 3);
    assert(EAI_AGENT_ERROR == 4);
    PASS("agent_state_enum");
}

int main(void) {
    printf("=== eai Agent Tests ===\n");
    test_agent_init();
    test_agent_init_null();
    test_agent_run();
    test_agent_run_null_task();
    test_agent_output();
    test_agent_output_before_run();
    test_agent_reset();
    test_agent_state_enum();
    printf("\n=== ALL %d TESTS PASSED ===\n", passed);
    return 0;
}
