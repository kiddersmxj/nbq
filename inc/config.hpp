#ifndef K_CONFIG_NBQ
#define K_CONFIG_NBQ

#include <iostream>
#include <std-k.hpp>

// Resolved at startup by InitConfig() via upward .nbq discovery.
inline std::string NbqDir;       // absolute path to the .nbq directory
inline std::string PartlistPath; // absolute path to partlist file
inline std::string NetlistPath;  // absolute path to netlist file
inline std::string McuHalPath;   // absolute path to mcu_hal.json

int InitConfig();

const std::string ProgramName = "nbq";
const std::string Version = "1.0.0";
const std::string UsageNotes = R"(usage: nbq [--json] <command> [args]
commands:
    init                      create a new .nbq project in the current directory
    summary                   part and net counts
    parts [filter]            list parts, optional filter on ref/value/device/library/package
    search <term>             search across all parts and net names
    part <ref>                part metadata and pin/net map
    net <name>                all members of a net
    pin <ref> <pad>           resolve a pin to its net and peers
    connected <ref>           first-hop connections for every pin
    compare <ref1> <ref2>     shared and differing nets between two parts
options:
    -h / --help               show help and exit
    -v / --version            print version and exit
    --json                    output as JSON)";

void Usage();
void Usage(std::string Message);
void PrintVersion();

#endif

// Copyright (c) 2026, Maxamilian Kidd-May
// All rights reserved.

// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.
