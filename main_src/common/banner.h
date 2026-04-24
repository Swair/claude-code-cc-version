// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

namespace prosophor {

/// Print the application banner with ASCII art
/// If show_buddy is true, displays the user's companion pet
void PrintBanner(const std::string& version, bool show_buddy = false);

/// Print help message for available commands
void PrintHelp();

}  // namespace prosophor
