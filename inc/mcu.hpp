#ifndef K_MCU_NBQ
#define K_MCU_NBQ

#include <map>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// MCU signal mapping types
//
// Maps stable firmware signal names to physical package pads, silicon pin
// names, and net names. Top-level key is the MCU reference designator.
// ---------------------------------------------------------------------------

struct McuPin {
    std::string name;        // stable signal name  (e.g. "I2C_SCL")
    std::string pad;         // package pad number  (e.g. "36")
    std::string silicon_pin; // silicon pin name    (e.g. "P0.14/SCL")
    std::string net;         // net name            (e.g. "SCL")
};

struct McuDef {
    std::string         ref;   // MCU reference designator (e.g. "IC1")
    std::vector<McuPin> pins;  // ordered as in the JSON file
};

// ref → McuDef; std::map gives lexicographic ordering over MCU refs.
using McuMap = std::map<std::string, McuDef>;

// Load the MCU map from a JSON file.
// Preserves pin order within each MCU as declared in the file.
// Throws std::runtime_error if the file cannot be opened or parsed.
McuMap loadMcuMap(const std::string& path);

#endif

// Copyright (c) 2026, Maxamilian Kidd-May
// All rights reserved.

// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.
