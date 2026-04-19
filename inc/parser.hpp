#ifndef K_PARSER_NBQ
#define K_PARSER_NBQ

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Data structures from the raw files
// ---------------------------------------------------------------------------

struct Part {
    std::string ref;      // reference designator (e.g. "IC1")
    std::string value;    // component value/model
    std::string device;   // device name in schematic
    std::string package;  // PCB footprint
    std::string library;  // source library
};

struct NetRow {
    std::string net;   // signal name (carried forward from first row of group)
    std::string part;  // reference designator
    std::string pad;   // physical pad identifier
    std::string pin;   // functional pin name
};

// ---------------------------------------------------------------------------
// File discovery
// ---------------------------------------------------------------------------

// Return the lexicographically latest file in `dir` whose name starts with
// `prefix` (case-sensitive). Returns empty string if none found.
std::string findFile(const std::string& dir, const std::string& prefix);

// ---------------------------------------------------------------------------
// Parsers
// ---------------------------------------------------------------------------

// Parse a Fusion/EAGLE partlist export.
// Skips header block; detects column positions from the header row.
// All fields are whitespace-trimmed. Preserves original casing.
std::vector<Part> parsePartList(const std::string& path);

// Parse a Fusion/EAGLE netlist export.
// Skips header block; detects column positions from the header row.
// Carries the net name forward for continuation rows (those starting with
// whitespace). Blank separator lines are ignored.
// All fields are whitespace-trimmed. Preserves original casing.
std::vector<NetRow> parseNetList(const std::string& path);

#endif

// Copyright (c) 2026, Maxamilian Kidd-May
// All rights reserved.

// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.
