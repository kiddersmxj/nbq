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

bool cmd_summary  (const Model& m, bool jsonMode);

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

#endif

// Copyright (c) 2026, Maxamilian Kidd-May
// All rights reserved.

// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.
