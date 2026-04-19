#ifndef K_MODEL_NBQ
#define K_MODEL_NBQ

#include "parser.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Model types
// ---------------------------------------------------------------------------

// One pin/pad on a part, resolved to its net.
struct PinEntry {
    std::string pad;   // physical pad identifier  (e.g. "28", "P$2", "A")
    std::string pin;   // functional pin name      (e.g. "VDD", "P1.00")
    std::string net;   // resolved signal net name
};

// One member of a net: (part, pad, pin) tuple.
struct NetMember {
    std::string part;
    std::string pad;
    std::string pin;
};

// A net and all its members, sorted by (part, pad).
struct Net {
    std::string name;
    std::vector<NetMember> members;
};

// ---------------------------------------------------------------------------
// Pad sorting helper
// ---------------------------------------------------------------------------
// Pads that parse as integers sort numerically; others sort lexicographically
// after all numeric pads.
struct PadOrder {
    bool operator()(const std::string& a, const std::string& b) const;
};

// ---------------------------------------------------------------------------
// Model
// ---------------------------------------------------------------------------

class Model {
public:
    std::map<std::string, Part>                  parts;    // ref → Part
    std::map<std::string, Net>                   nets;     // name → Net
    std::map<std::string, std::vector<PinEntry>> partPins; // ref → sorted pins

    // Build the model from parsed raw data.
    void build(const std::vector<Part>& partList,
               const std::vector<NetRow>& netRows);
};

#endif

// Copyright (c) 2026, Maxamilian Kidd-May
// All rights reserved.

// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.
