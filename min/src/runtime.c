// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_min/runtime.h"
#include "eai_min/runtime_llama.h"
#include "eai/log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define LOG_MOD "min-rt"

/* ========================================================================
 * Simple binary model format for eAI-Min runtime:
 *
 *   Header (32 bytes):
 *     [0..3]   magic: "EAIM"
 *     [4..7]   version: uint32_t LE
 *     [8..11]  num_layers: uint32_t LE
 *     [12..15] input_dim: uint32_t LE
 *     [16..19] output_dim: uint32_t LE
 *     [20..23] total_params: uint32_t LE
 *     [24..31] reserved
 *
 *   Per-layer descriptor (16 bytes each):
 *     [0..3]   rows: uint32_t LE
 *     [4..7]   cols: uint32_t LE
 *     [8..11]  bias_count: uint32_t LE (== rows)
 *     [12..15] activation: uint32_t LE (0=none, 1=relu, 2=softmax)
 *
 *   Weights: float32 LE, row-major, for each layer:
 *     weights[rows * cols] followed by bias[rows]
 * ======================================================================== */

#define EAIM_MAGIC       0x4D494145  /* "EAIM" in LE */
#define EAIM_VERSION     1
#define EAIM_MAX_LAYERS  16
#define EAIM_MAX_DIM     4096

#define EAIM_ACT_NONE    0
#define EAIM_ACT_RELU    1
#define EAIM_ACT_SOFTMAX 2

typedef struct {
    uint32_t rows;
    uint32_t cols;
    uint32_t bias_count;
    uint32_t activation;
} eaim_layer_desc_t;

typedef struct {
    uint32_t rows;
    uint32_t cols;
    uint32_t activation;
    float   *weights;   /* rows x cols */
    float   *bias;      /* rows */
} eaim_layer_t;

typedef struct {
    uint32_t      num_layers;
    uint32_t      input_dim;
    uint32_t      output_dim;
    eaim_layer_t  layers[EAIM_MAX_LAYERS];
    float        *scratch_a;  /* working buffer */
    float        *scratch_b;  /* working buffer */
    uint32_t      scratch_size;
} eaim_model_t;

static uint32_t read_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static float read_f32_le(const uint8_t *p) {
    uint32_t u = read_u32_le(p);
    float f;
    memcpy(&f, &u, sizeof(f));
    return f;
}

static void apply_relu(float *v, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        if (v[i] < 0.0f) v[i] = 0.0f;
    }
}

static void apply_softmax(float *v, uint32_t n) {
    float max_val = v[0];
    for (uint32_t i = 1; i < n; i++) {
        if (v[i] > max_val) max_val = v[i];
    }
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        v[i] = expf(v[i] - max_val);
        sum += v[i];
    }
    if (sum > 0.0f) {
        for (uint32_t i = 0; i < n; i++) {
            v[i] /= sum;
        }
    }
}

/**
 * Dense layer forward pass: out = W * in + bias
 * W is [rows x cols], in is [cols], out is [rows]
 */
static void dense_forward(const eaim_layer_t *layer,
                           const float *input, float *output) {
    for (uint32_t r = 0; r < layer->rows; r++) {
        float acc = layer->bias[r];
        const float *w_row = &layer->weights[r * layer->cols];
        for (uint32_t c = 0; c < layer->cols; c++) {
            acc += w_row[c] * input[c];
        }
        output[r] = acc;
    }

    switch (layer->activation) {
    case EAIM_ACT_RELU:
        apply_relu(output, layer->rows);
        break;
    case EAIM_ACT_SOFTMAX:
        apply_softmax(output, layer->rows);
        break;
    default:
        break;
    }
}

static void eaim_model_free(eaim_model_t *m) {
    for (uint32_t i = 0; i < m->num_layers; i++) {
        free(m->layers[i].weights);
        free(m->layers[i].bias);
        m->layers[i].weights = NULL;
        m->layers[i].bias = NULL;
    }
    free(m->scratch_a);
    free(m->scratch_b);
    m->scratch_a = NULL;
    m->scratch_b = NULL;
    m->num_layers = 0;
}

/**
 * Parse a binary model buffer into eaim_model_t.
 * Returns EAI_OK on success.
 */
static eai_status_t eaim_model_load(eaim_model_t *m, const uint8_t *buf, size_t buf_len) {
    memset(m, 0, sizeof(*m));

    if (buf_len < 32) {
        EAI_LOG_ERROR(LOG_MOD, "model buffer too small for header (%zu bytes)", buf_len);
        return EAI_ERR_INVALID;
    }

    uint32_t magic = read_u32_le(buf);
    if (magic != EAIM_MAGIC) {
        EAI_LOG_ERROR(LOG_MOD, "invalid model magic: 0x%08x (expected 0x%08x)", magic, EAIM_MAGIC);
        return EAI_ERR_INVALID;
    }

    uint32_t version = read_u32_le(buf + 4);
    if (version != EAIM_VERSION) {
        EAI_LOG_ERROR(LOG_MOD, "unsupported model version: %u", version);
        return EAI_ERR_INVALID;
    }

    m->num_layers = read_u32_le(buf + 8);
    m->input_dim  = read_u32_le(buf + 12);
    m->output_dim = read_u32_le(buf + 16);

    if (m->num_layers == 0 || m->num_layers > EAIM_MAX_LAYERS) {
        EAI_LOG_ERROR(LOG_MOD, "invalid layer count: %u", m->num_layers);
        return EAI_ERR_INVALID;
    }
    if (m->input_dim == 0 || m->input_dim > EAIM_MAX_DIM ||
        m->output_dim == 0 || m->output_dim > EAIM_MAX_DIM) {
        EAI_LOG_ERROR(LOG_MOD, "invalid dims: in=%u out=%u", m->input_dim, m->output_dim);
        return EAI_ERR_INVALID;
    }

    size_t offset = 32;

    /* Read layer descriptors */
    size_t desc_size = m->num_layers * 16;
    if (offset + desc_size > buf_len) {
        EAI_LOG_ERROR(LOG_MOD, "buffer too small for layer descriptors");
        return EAI_ERR_INVALID;
    }

    eaim_layer_desc_t descs[EAIM_MAX_LAYERS];
    uint32_t max_dim = m->input_dim;
    for (uint32_t i = 0; i < m->num_layers; i++) {
        const uint8_t *p = buf + offset + i * 16;
        descs[i].rows       = read_u32_le(p);
        descs[i].cols       = read_u32_le(p + 4);
        descs[i].bias_count = read_u32_le(p + 8);
        descs[i].activation = read_u32_le(p + 12);

        if (descs[i].rows == 0 || descs[i].rows > EAIM_MAX_DIM ||
            descs[i].cols == 0 || descs[i].cols > EAIM_MAX_DIM) {
            EAI_LOG_ERROR(LOG_MOD, "invalid layer %u dims: %u x %u", i, descs[i].rows, descs[i].cols);
            return EAI_ERR_INVALID;
        }
        if (descs[i].rows > max_dim) max_dim = descs[i].rows;
        if (descs[i].cols > max_dim) max_dim = descs[i].cols;
    }
    offset += desc_size;

    /* Allocate scratch buffers */
    m->scratch_size = max_dim;
    m->scratch_a = (float *)calloc(max_dim, sizeof(float));
    m->scratch_b = (float *)calloc(max_dim, sizeof(float));
    if (!m->scratch_a || !m->scratch_b) {
        eaim_model_free(m);
        return EAI_ERR_NOMEM;
    }

    /* Read weights and biases for each layer */
    for (uint32_t i = 0; i < m->num_layers; i++) {
        uint32_t w_count = descs[i].rows * descs[i].cols;
        uint32_t b_count = descs[i].bias_count;
        size_t need = (w_count + b_count) * sizeof(float);

        if (offset + need > buf_len) {
            EAI_LOG_ERROR(LOG_MOD, "buffer too small for layer %u weights", i);
            eaim_model_free(m);
            return EAI_ERR_INVALID;
        }

        m->layers[i].rows       = descs[i].rows;
        m->layers[i].cols       = descs[i].cols;
        m->layers[i].activation = descs[i].activation;

        m->layers[i].weights = (float *)malloc(w_count * sizeof(float));
        m->layers[i].bias    = (float *)malloc(b_count * sizeof(float));
        if (!m->layers[i].weights || !m->layers[i].bias) {
            eaim_model_free(m);
            return EAI_ERR_NOMEM;
        }

        for (uint32_t j = 0; j < w_count; j++) {
            m->layers[i].weights[j] = read_f32_le(buf + offset);
            offset += 4;
        }
        for (uint32_t j = 0; j < b_count; j++) {
            m->layers[i].bias[j] = read_f32_le(buf + offset);
            offset += 4;
        }

        EAI_LOG_DEBUG(LOG_MOD, "  layer[%u]: %u x %u, act=%u",
                      i, descs[i].rows, descs[i].cols, descs[i].activation);
    }

    EAI_LOG_INFO(LOG_MOD, "model loaded: %u layers, in=%u, out=%u",
                 m->num_layers, m->input_dim, m->output_dim);
    return EAI_OK;
}

/**
 * Run inference through all dense layers.
 */
static eai_status_t eaim_model_infer(eaim_model_t *m,
                                      const float *input, uint32_t input_len,
                                      float *output, uint32_t output_len) {
    if (input_len != m->input_dim) {
        EAI_LOG_ERROR(LOG_MOD, "input dim mismatch: got %u expected %u", input_len, m->input_dim);
        return EAI_ERR_INVALID;
    }

    /* Copy input to scratch_a */
    memcpy(m->scratch_a, input, input_len * sizeof(float));

    float *cur_in  = m->scratch_a;
    float *cur_out = m->scratch_b;

    for (uint32_t i = 0; i < m->num_layers; i++) {
        dense_forward(&m->layers[i], cur_in, cur_out);

        /* Swap buffers for next layer */
        float *tmp = cur_in;
        cur_in = cur_out;
        cur_out = tmp;
    }

    /* cur_in now points to the final output */
    uint32_t copy_n = m->output_dim < output_len ? m->output_dim : output_len;
    memcpy(output, cur_in, copy_n * sizeof(float));

    return EAI_OK;
}

/* ---- Stub runtime (for development/testing without real backends) ---- */

static eai_status_t stub_init(eai_runtime_t *rt)
{
    EAI_LOG_INFO(LOG_MOD, "stub runtime initialized");
    return EAI_OK;
}

static eai_status_t stub_load_model(eai_runtime_t *rt,
                                     const eai_model_manifest_t *manifest,
                                     const char *model_path)
{
    EAI_LOG_INFO(LOG_MOD, "stub: loading model '%s' from %s", manifest->name, model_path);

    /* Try to load binary model file */
    FILE *fp = fopen(model_path, "rb");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        long fsize = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        if (fsize > 0 && fsize < 64 * 1024 * 1024) {  /* max 64 MB */
            uint8_t *buf = (uint8_t *)malloc((size_t)fsize);
            if (buf) {
                size_t nread = fread(buf, 1, (size_t)fsize, fp);
                fclose(fp);

                if (nread == (size_t)fsize) {
                    eaim_model_t *model = (eaim_model_t *)calloc(1, sizeof(eaim_model_t));
                    if (model) {
                        eai_status_t s = eaim_model_load(model, buf, nread);
                        free(buf);
                        if (s == EAI_OK) {
                            rt->ctx = model;
                            rt->loaded = true;
                            memcpy(&rt->manifest, manifest, sizeof(eai_model_manifest_t));
                            EAI_LOG_INFO(LOG_MOD, "binary model loaded successfully");
                            return EAI_OK;
                        }
                        free(model);
                        EAI_LOG_WARN(LOG_MOD, "binary parse failed, falling back to stub");
                    } else {
                        free(buf);
                    }
                } else {
                    free(buf);
                    fclose(fp);
                }
            } else {
                fclose(fp);
            }
        } else {
            fclose(fp);
        }
    }

    /* Fallback: stub mode (no real model data) */
    rt->ctx = NULL;
    rt->loaded = true;
    memcpy(&rt->manifest, manifest, sizeof(eai_model_manifest_t));
    EAI_LOG_INFO(LOG_MOD, "stub mode: model '%s' marked as loaded", manifest->name);
    return EAI_OK;
}

static eai_status_t stub_infer(eai_runtime_t *rt,
                                const eai_inference_input_t *in,
                                eai_inference_output_t *out)
{
    if (!rt->loaded) return EAI_ERR_RUNTIME;

    memset(out, 0, sizeof(*out));

    /* If we have a real binary model, run dense inference */
    if (rt->ctx) {
        eaim_model_t *model = (eaim_model_t *)rt->ctx;

        /*
         * For text input: hash the prompt into a fixed-size float vector.
         * This is a simple simulation — real models would use tokenization.
         */
        float input_vec[EAIM_MAX_DIM];
        memset(input_vec, 0, sizeof(input_vec));
        if (in->text && in->text_len > 0) {
            for (size_t i = 0; i < in->text_len && i < model->input_dim; i++) {
                input_vec[i % model->input_dim] += (float)((uint8_t)in->text[i]) / 255.0f;
            }
        }

        float output_vec[EAIM_MAX_DIM];
        eai_status_t s = eaim_model_infer(model, input_vec, model->input_dim,
                                           output_vec, model->output_dim);
        if (s != EAI_OK) {
            snprintf(out->text, sizeof(out->text), "[error] inference failed");
            out->text_len = strlen(out->text);
            return s;
        }

        /* Find argmax for classification output */
        int best_idx = 0;
        float best_val = output_vec[0];
        for (uint32_t i = 1; i < model->output_dim; i++) {
            if (output_vec[i] > best_val) {
                best_val = output_vec[i];
                best_idx = (int)i;
            }
        }

        int n = snprintf(out->text, sizeof(out->text),
                         "class=%d confidence=%.4f", best_idx, best_val);
        out->text_len = (size_t)(n > 0 ? n : 0);
        out->confidence = best_val;
        out->tokens_used = (uint32_t)in->text_len;
        out->latency_ms = 1;

        return EAI_OK;
    }

    /* Stub fallback */
    const char *response = "[stub] inference result for input";
    size_t rlen = strlen(response);
    if (rlen >= sizeof(out->text)) rlen = sizeof(out->text) - 1;
    memcpy(out->text, response, rlen);
    out->text[rlen] = '\0';
    out->text_len = rlen;
    out->confidence = 0.95f;
    out->tokens_used = 12;
    out->latency_ms = 1;

    return EAI_OK;
}

static eai_status_t stub_unload_model(eai_runtime_t *rt)
{
    if (rt->ctx) {
        eaim_model_t *model = (eaim_model_t *)rt->ctx;
        eaim_model_free(model);
        free(model);
        rt->ctx = NULL;
    }
    rt->loaded = false;
    EAI_LOG_INFO(LOG_MOD, "model unloaded");
    return EAI_OK;
}

static void stub_shutdown(eai_runtime_t *rt)
{
    if (rt->loaded) {
        stub_unload_model(rt);
    }
    EAI_LOG_INFO(LOG_MOD, "runtime shutdown");
}

const eai_runtime_ops_t eai_runtime_stub_ops = {
    .name         = "stub",
    .init         = stub_init,
    .load_model   = stub_load_model,
    .infer        = stub_infer,
    .unload_model = stub_unload_model,
    .shutdown     = stub_shutdown,
};

/* ---- EAI-Min runtime wrapper ---- */

eai_status_t eai_min_runtime_create(eai_min_runtime_t *rt, eai_runtime_type_t type)
{
    if (!rt) return EAI_ERR_INVALID;
    memset(rt, 0, sizeof(*rt));

    const eai_runtime_ops_t *ops = NULL;

    switch (type) {
    case EAI_RUNTIME_LLAMA_CPP:
#ifdef EAI_LLAMA_CPP_ENABLED
        EAI_LOG_INFO(LOG_MOD, "llama.cpp backend selected (real)");
        ops = &eai_runtime_llama_ops;
#else
        EAI_LOG_INFO(LOG_MOD, "llama.cpp backend selected (stub)");
        ops = &eai_runtime_stub_ops;
#endif
        break;
    case EAI_RUNTIME_ONNX:
        EAI_LOG_INFO(LOG_MOD, "ONNX backend selected (using stub)");
        ops = &eai_runtime_stub_ops;
        break;
    case EAI_RUNTIME_TFLITE:
        EAI_LOG_INFO(LOG_MOD, "TFLite backend selected (using stub)");
        ops = &eai_runtime_stub_ops;
        break;
    default:
        EAI_LOG_INFO(LOG_MOD, "custom/stub backend selected");
        ops = &eai_runtime_stub_ops;
        break;
    }

    eai_status_t s = eai_runtime_init(&rt->base, ops);
    if (s != EAI_OK) return s;

    rt->max_tokens   = 256;
    rt->temperature  = 0.7f;
    rt->initialized  = true;

    return EAI_OK;
}

eai_status_t eai_min_runtime_load(eai_min_runtime_t *rt, const char *model_path,
                                   const eai_model_manifest_t *manifest)
{
    if (!rt || !rt->initialized) return EAI_ERR_INVALID;
    rt->model_path = model_path;
    return eai_runtime_load(&rt->base, manifest, model_path);
}

eai_status_t eai_min_runtime_infer(eai_min_runtime_t *rt,
                                    const char *prompt,
                                    char *output, size_t output_size)
{
    if (!rt || !rt->initialized || !rt->base.loaded) return EAI_ERR_RUNTIME;

    eai_inference_input_t in = {
        .text     = prompt,
        .text_len = strlen(prompt),
        .binary   = NULL,
        .binary_len = 0,
    };

    eai_inference_output_t out;
    memset(&out, 0, sizeof(out));

    eai_status_t s = eai_runtime_infer(&rt->base, &in, &out);
    if (s != EAI_OK) return s;

    size_t copy_len = out.text_len < output_size - 1 ? out.text_len : output_size - 1;
    memcpy(output, out.text, copy_len);
    output[copy_len] = '\0';

    return EAI_OK;
}

void eai_min_runtime_destroy(eai_min_runtime_t *rt)
{
    if (!rt) return;
    if (rt->base.loaded) eai_runtime_unload(&rt->base);
    eai_runtime_shutdown(&rt->base);
    rt->initialized = false;
}
