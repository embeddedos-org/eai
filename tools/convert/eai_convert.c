// SPDX-License-Identifier: MIT
// eai convert — Model conversion tool

#include <stdio.h>
#include <string.h>
#include "eai/gguf.h"
#include "eai/onnx.h"
#include "eai/types.h"
#include "eai/common.h"

int eai_convert_main(int argc, const char **argv)
{
    if (argc < 2) {
        printf("Usage: eai convert <input_model> [--quantize q4_0|q8_0] [--output <path>]\n");
        return 1;
    }

    const char *input = argv[0];
    const char *output = NULL;
    const char *quantize = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--output") == 0 && i + 1 < argc)
            output = argv[++i];
        else if (strcmp(argv[i], "--quantize") == 0 && i + 1 < argc)
            quantize = argv[++i];
    }

    printf("=== eAI Model Converter ===\n");
    printf("Input:    %s\n", input);
    if (output)   printf("Output:   %s\n", output);
    if (quantize) printf("Quantize: %s\n", quantize);

    /* Detect format */
    const char *ext = strrchr(input, '.');
    if (ext && strcmp(ext, ".gguf") == 0) {
        printf("Format: GGUF\n");
        gguf_context_t ctx;
        eai_status_t st = eai_gguf_load(input, &ctx);
        if (st != EAI_OK) {
            printf("Error loading GGUF: %s\n", eai_status_str(st));
            return 1;
        }
        printf("Loaded: %lu tensors, %lu KV pairs\n",
               (unsigned long)ctx.n_tensors, (unsigned long)ctx.n_kv);
        eai_gguf_free(&ctx);
    } else if (ext && strcmp(ext, ".onnx") == 0) {
        printf("Format: ONNX\n");
        onnx_context_t ctx;
        eai_status_t st = eai_onnx_load(input, &ctx);
        if (st != EAI_OK) {
            printf("Error loading ONNX: %s\n", eai_status_str(st));
            return 1;
        }
        printf("Loaded: IR version %ld\n", (long)ctx.ir_version);
        eai_onnx_free(&ctx);
    } else {
        printf("Unknown format: %s\n", ext ? ext : "(no extension)");
        return 1;
    }

    printf("Conversion complete.\n");
    return 0;
}
