// SPDX-License-Identifier: MIT
// eai bench — Benchmarking tool for eAI inference

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eai/accel.h"
#include "eai/types.h"

#ifdef _WIN32
#include <windows.h>
static double time_ms(void)
{
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart * 1000.0 / (double)freq.QuadPart;
}
#else
#include <time.h>
static double time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}
#endif

typedef struct {
    double min_ms;
    double max_ms;
    double avg_ms;
    double p50_ms;
    double p95_ms;
    double p99_ms;
    size_t peak_memory;
    int    iterations;
} bench_result_t;

static int compare_double(const void *a, const void *b)
{
    double da = *(const double *)a, db = *(const double *)b;
    return (da > db) - (da < db);
}

static bench_result_t run_benchmark(int warmup, int iterations)
{
    bench_result_t result;
    memset(&result, 0, sizeof(result));
    result.iterations = iterations;

    double *times = (double *)calloc(iterations, sizeof(double));
    if (!times) return result;

    /* Create a simple matmul benchmark */
    int64_t shape_a[] = {128, 128};
    int64_t shape_b[] = {128, 128};
    int64_t shape_c[] = {128, 128};
    eai_tensor_t a, b, c;
    eai_tensor_create(&a, EAI_DTYPE_F32, shape_a, 2);
    eai_tensor_create(&b, EAI_DTYPE_F32, shape_b, 2);
    eai_tensor_create(&c, EAI_DTYPE_F32, shape_c, 2);

    /* Fill with random data */
    float *fa = (float *)a.data, *fb = (float *)b.data;
    for (int i = 0; i < 128 * 128; i++) {
        fa[i] = (float)(rand() % 100) / 100.0f;
        fb[i] = (float)(rand() % 100) / 100.0f;
    }

    /* Register CPU backend */
    extern const eai_accel_backend_ops_t eai_accel_cpu_ops;
    eai_accel_register(&eai_accel_cpu_ops);

    /* Setup compute graph */
    int input_idx[] = {0, 1};
    int output_idx[] = {2};
    eai_op_t op = {
        .type = EAI_OP_MATMUL,
        .input_count = 2,
        .output_count = 1,
        .input_indices = input_idx,
        .output_indices = output_idx,
    };

    eai_tensor_t tensors[] = {a, b, c};
    eai_compute_graph_t graph = {
        .ops = &op,
        .op_count = 1,
        .tensors = tensors,
        .tensor_count = 3,
    };

    /* Warmup */
    for (int i = 0; i < warmup; i++) {
        eai_accel_dispatch(&graph);
    }

    /* Benchmark */
    double total = 0;
    result.min_ms = 1e9;
    result.max_ms = 0;

    for (int i = 0; i < iterations; i++) {
        double t0 = time_ms();
        eai_accel_dispatch(&graph);
        double t1 = time_ms();

        double elapsed = t1 - t0;
        times[i] = elapsed;
        total += elapsed;
        if (elapsed < result.min_ms) result.min_ms = elapsed;
        if (elapsed > result.max_ms) result.max_ms = elapsed;
    }

    result.avg_ms = total / iterations;

    /* Sort for percentiles */
    qsort(times, iterations, sizeof(double), compare_double);
    result.p50_ms = times[iterations / 2];
    result.p95_ms = times[(int)(iterations * 0.95)];
    result.p99_ms = times[(int)(iterations * 0.99)];

    eai_tensor_destroy(&a);
    eai_tensor_destroy(&b);
    eai_tensor_destroy(&c);
    free(times);

    return result;
}

int eai_bench_main(int argc, const char **argv)
{
    int warmup = 10;
    int iterations = 100;

    (void)argc; (void)argv;

    printf("=== eAI Benchmark ===\n");
    printf("Warmup: %d iterations\n", warmup);
    printf("Benchmark: %d iterations\n\n", iterations);

    bench_result_t r = run_benchmark(warmup, iterations);

    printf("--- Results (128x128 F32 MatMul) ---\n");
    printf("  Min:  %8.3f ms\n", r.min_ms);
    printf("  Max:  %8.3f ms\n", r.max_ms);
    printf("  Avg:  %8.3f ms\n", r.avg_ms);
    printf("  P50:  %8.3f ms\n", r.p50_ms);
    printf("  P95:  %8.3f ms\n", r.p95_ms);
    printf("  P99:  %8.3f ms\n", r.p99_ms);

    /* JSON output */
    printf("\n{\"benchmark\": \"matmul_128x128_f32\", \"iterations\": %d, "
           "\"min_ms\": %.3f, \"max_ms\": %.3f, \"avg_ms\": %.3f, "
           "\"p50_ms\": %.3f, \"p95_ms\": %.3f, \"p99_ms\": %.3f}\n",
           iterations, r.min_ms, r.max_ms, r.avg_ms, r.p50_ms, r.p95_ms, r.p99_ms);

    return 0;
}
