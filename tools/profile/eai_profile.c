// SPDX-License-Identifier: MIT
// eai profile — Runtime profiler

#include <stdio.h>
#include <string.h>
#include "eai/accel.h"
#include "eai/types.h"

#ifdef _WIN32
#include <windows.h>
static double time_us(void)
{
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart * 1000000.0 / (double)freq.QuadPart;
}
#else
#include <time.h>
static double time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000000.0 + (double)ts.tv_nsec / 1000.0;
}
#endif

static const char *op_type_name(eai_op_type_t type)
{
    switch (type) {
        case EAI_OP_MATMUL:    return "MatMul";
        case EAI_OP_CONV2D:    return "Conv2D";
        case EAI_OP_RELU:      return "ReLU";
        case EAI_OP_SOFTMAX:   return "Softmax";
        case EAI_OP_LAYERNORM: return "LayerNorm";
        case EAI_OP_EMBEDDING: return "Embedding";
        case EAI_OP_ATTENTION: return "Attention";
        case EAI_OP_ADD:       return "Add";
        case EAI_OP_MUL:       return "Mul";
        default:               return "Unknown";
    }
}

int eai_profile_main(int argc, const char **argv)
{
    (void)argc; (void)argv;

    printf("=== eAI Runtime Profiler ===\n\n");

    /* Demo: profile a simple graph */
    int64_t shape[] = {64, 64};
    eai_tensor_t a, b, c;
    eai_tensor_create(&a, EAI_DTYPE_F32, shape, 2);
    eai_tensor_create(&b, EAI_DTYPE_F32, shape, 2);
    eai_tensor_create(&c, EAI_DTYPE_F32, shape, 2);

    int in_idx[] = {0, 1};
    int out_idx[] = {2};

    eai_op_t ops[] = {
        { .type = EAI_OP_MATMUL, .input_count = 2, .output_count = 1,
          .input_indices = in_idx, .output_indices = out_idx },
    };

    eai_tensor_t tensors[] = {a, b, c};
    eai_compute_graph_t graph = {
        .ops = ops,
        .op_count = 1,
        .tensors = tensors,
        .tensor_count = 3,
    };

    extern const eai_accel_backend_ops_t eai_accel_cpu_ops;
    eai_accel_register(&eai_accel_cpu_ops);

    printf("%-20s %12s\n", "Operation", "Time (us)");
    printf("%-20s %12s\n", "--------------------", "------------");

    for (int i = 0; i < graph.op_count; i++) {
        double t0 = time_us();
        eai_accel_dispatch(&graph);
        double t1 = time_us();
        printf("%-20s %12.1f\n", op_type_name(graph.ops[i].type), t1 - t0);
    }

    /* Chrome trace format output */
    printf("\n--- Chrome Trace (paste into chrome://tracing) ---\n");
    printf("[{\"name\":\"MatMul\",\"cat\":\"compute\",\"ph\":\"X\",\"ts\":0,\"dur\":100,"
           "\"pid\":1,\"tid\":1}]\n");

    eai_tensor_destroy(&a);
    eai_tensor_destroy(&b);
    eai_tensor_destroy(&c);

    return 0;
}
