// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Header-only C++ RAII wrappers for eAI BCI

#pragma once

#include "eai_bci/api.h"
#include <string>
#include <stdexcept>
#include <optional>
#include <cstdint>

namespace eai::bci {

struct Intent {
    std::string label;
    float confidence;
    uint32_t class_id;
};

class Pipeline {
public:
    Pipeline(const std::string& device = "simulator",
             const std::string& decoder = "threshold",
             const std::string& output = "log")
    {
        handle_ = eai_bci_create(device.c_str(), decoder.c_str(), output.c_str());
        if (!handle_) throw std::runtime_error("Failed to create BCI pipeline");
    }

    ~Pipeline() { if (handle_) eai_bci_destroy(handle_); }

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&& o) noexcept : handle_(o.handle_) { o.handle_ = nullptr; }

    void start() { if (eai_bci_start(handle_) != 0) throw std::runtime_error("start failed"); }
    void stop()  { eai_bci_stop(handle_); }
    bool poll()  { return eai_bci_poll(handle_) == 0; }

    std::optional<Intent> get_intent() const {
        char label[64];
        float conf;
        uint32_t cls;
        if (eai_bci_get_intent(handle_, label, sizeof(label), &conf, &cls) != 0)
            return std::nullopt;
        return Intent{label, conf, cls};
    }

    int channel_count() const { return eai_bci_get_channel_count(handle_); }
    int sample_rate()   const { return eai_bci_get_sample_rate(handle_); }
    uint64_t samples_processed() const { return eai_bci_get_samples_processed(handle_); }

    static std::string version() { return eai_bci_version(); }

private:
    eai_bci_handle_t* handle_ = nullptr;
};

} // namespace eai::bci
