#ifndef K_CONFIG_NBQ
#define K_CONFIG_NBQ

#include <iostream>
#include <std-k.hpp>

inline std::string DataDir;

const std::string ConfigFilePath = "config.conf";
int InitConfig();

const std::string ProgramName = "nbq";
const std::string Version = "1.0.0";
const std::string UsageNotes = R"(usage: nbq [--json] <command> [args]
commands:
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
