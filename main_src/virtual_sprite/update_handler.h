// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>

namespace prosophor {

class UpdateHandler {
public:
    void CheckAndShowUpdate();
    void Render();

private:
    bool update_popup_open_ = false;
    std::string update_status_text_;
};

}  // namespace prosophor
