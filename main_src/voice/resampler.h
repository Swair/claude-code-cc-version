// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <vector>

namespace prosophor {

/// Resample 24kHz PCM int16 → 16kHz using linear interpolation (3:2 ratio).
/// No external resampler needed for this simple ratio.
std::vector<int16_t> Resample24kTo16k(const std::vector<int16_t>& input);

}  // namespace prosophor
