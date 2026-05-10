// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "banner.h"

#include <iostream>

namespace prosophor {

void PrintBanner(const std::string& version) {
    std::cout << "\n";

    // Default robot face
    std::cout << "          ____        \n";
    std::cout << "     ╭─(─|    |─)-──╮\n";
    std::cout << "    ╭─────────────╮ |\n";
    std::cout << " )==│  ◉      ◉   │ |==( \n";
    std::cout << "    │     V       │ |\n";
    std::cout << "    ╰─────────────╯|\n";
    std::cout << "    ╰──────────────╯   \n";
    std::cout << "     ╰────╯  ╰────╯    \n";
    std::cout << "\n";

    std::cout << "Prosophor v" << version << "\n";
    std::cout << "AI Coding Assistant"  << "\n";
    std::cout << "\n";
    std::cout << "  Type 'exit' or Ctrl+D to quit, Ctrl+C to interrupt\n";
    std::cout << "\n";
}

}  // namespace prosophor
