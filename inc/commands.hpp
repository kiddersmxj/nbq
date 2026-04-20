#ifndef K_COMMANDS_NBQ
#define K_COMMANDS_NBQ

#include "model.hpp"
#include <string>

// ---------------------------------------------------------------------------
// Command handlers
//
// Each function writes to stdout.
// Returns true on success, false on any lookup/data failure.
// jsonMode == true  → structured JSON output
// jsonMode == false → human-readable text
//
// On failure the handler emits an error message (JSON object or plain text)
// and returns false; the caller should exit with EXIT_FAILURE.
// ---------------------------------------------------------------------------

bool cmd_summary  (const Model& m, const std::string& mcuMapFile, bool jsonMode);

bool cmd_parts    (const Model& m, const std::string& filter, bool jsonMode);

bool cmd_search   (const Model& m, const std::string& term, bool jsonMode);

bool cmd_part     (const Model& m, const std::string& ref, bool jsonMode);

bool cmd_net      (const Model& m, const std::string& name, bool jsonMode);

// pad_or_pin: matched against pad (exact), then pin name (exact),
//             then either (case-insensitive). No fuzzy matching.
bool cmd_pin      (const Model& m, const std::string& ref,
                   const std::string& pad_or_pin, bool jsonMode);

bool cmd_connected(const Model& m, const std::string& ref, bool jsonMode);

bool cmd_compare  (const Model& m, const std::string& ref1,
                   const std::string& ref2, bool jsonMode);

// maxDepth < 0 = unlimited; viaNet = "" = all nets.
// includePowerNets = false → rail nets (GND, VCC, 3V3 …) are not traversed.
bool cmd_walk     (const Model& m, const std::string& startRef,
                   int maxDepth, const std::string& viaNet,
                   bool includePowerNets, bool jsonMode);

// ---------------------------------------------------------------------------
// MCU signal mapping commands
//
// mcuMapFile: path from config (mcu.map).  Empty → error.
// ref:        empty → list all MCU refs; non-empty → show signals for that ref.
// query:      empty → list all signal names; non-empty → resolve by any field.
// ---------------------------------------------------------------------------

bool cmd_mcu   (const std::string& mcuMapFile, const std::string& ref,
                bool jsonMode);

bool cmd_signal(const std::string& mcuMapFile, const std::string& query,
                bool jsonMode);

#endif

// Copyright (c) 2026, Maxamilian Kidd-May
// All rights reserved.

// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.
