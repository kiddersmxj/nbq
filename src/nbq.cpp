#include "../inc/nbq.hpp"

#include <iostream>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void dieUsage(const std::string& msg = "") {
    if (!msg.empty()) std::cerr << "error: " << msg << "\n";
    Usage();
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {

    // ---- Config ------------------------------------------------------------
    if (InitConfig() != 0) return EXIT_FAILURE;

    // ---- Global flags (getopt for -h / -v only) ----------------------------
    int HelpFlag    = 0;
    int VersionFlag = 0;
    int JsonFlag    = 0;

    struct option Opts[] = {
        { "help",    no_argument, &HelpFlag,    1 },
        { "version", no_argument, &VersionFlag, 1 },
        { "json",    no_argument, &JsonFlag,     1 },
        { 0 }
    };

    // Consume -h / -v / --help / --version / --json; stop at first
    // non-option (the subcommand name).
    int opt;
    while ((opt = getopt_long(argc, argv, "+hv", Opts, nullptr)) != -1) {
        switch (opt) {
        case 'h': HelpFlag    = 1; break;
        case 'v': VersionFlag = 1; break;
        case '?': dieUsage(); return EXIT_FAILURE;
        default:  break;   // flag set by getopt for long options
        }
    }

    if (HelpFlag)    { Usage();         return EXIT_SUCCESS; }
    if (VersionFlag) { PrintVersion();  return EXIT_SUCCESS; }

    // ---- Remaining args: [--json] <command> [args...] ----------------------
    // argv[optind..] holds everything getopt didn't consume
    std::vector<std::string> args;
    for (int i = optind; i < argc; ++i) args.emplace_back(argv[i]);

    if (args.empty()) { dieUsage("no command given"); return EXIT_FAILURE; }

    // --json may also appear inline before the subcommand (not caught by getopt)
    bool jsonMode = (JsonFlag == 1);
    if (!args.empty() && args[0] == "--json") {
        jsonMode = true;
        args.erase(args.begin());
    }
    if (args.empty()) { dieUsage("no command given"); return EXIT_FAILURE; }

    const std::string cmd = args[0];
    args.erase(args.begin());   // args now holds command arguments only

    // ---- Load data ---------------------------------------------------------
    std::string partFile = findFile(DataDir, "Partlist");
    std::string netFile  = findFile(DataDir, "Netlist");

    if (partFile.empty() && netFile.empty()) {
        std::cerr << "error: no partlist* or netlist* files found in: "
                  << DataDir << "\n";
        return EXIT_FAILURE;
    }
    if (partFile.empty()) {
        std::cerr << "error: no partlist* file found in: " << DataDir << "\n";
        return EXIT_FAILURE;
    }
    if (netFile.empty()) {
        std::cerr << "error: no netlist* file found in: " << DataDir << "\n";
        return EXIT_FAILURE;
    }

    Model model;
    try {
        auto parts   = parsePartList(partFile);
        auto netRows = parseNetList(netFile);
        model.build(parts, netRows);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    // ---- Dispatch ----------------------------------------------------------
    bool ok = false;
    try {
        if (cmd == "summary") {
            ok = cmd_summary(model, jsonMode);

        } else if (cmd == "parts") {
            std::string filter = args.empty() ? "" : args[0];
            ok = cmd_parts(model, filter, jsonMode);

        } else if (cmd == "search") {
            if (args.empty()) { dieUsage("search requires a term"); return EXIT_FAILURE; }
            ok = cmd_search(model, args[0], jsonMode);

        } else if (cmd == "part") {
            if (args.empty()) { dieUsage("part requires a ref"); return EXIT_FAILURE; }
            ok = cmd_part(model, args[0], jsonMode);

        } else if (cmd == "net") {
            if (args.empty()) { dieUsage("net requires a name"); return EXIT_FAILURE; }
            ok = cmd_net(model, args[0], jsonMode);

        } else if (cmd == "pin") {
            if (args.size() < 2) {
                dieUsage("pin requires <ref> <pad-or-pin>"); return EXIT_FAILURE;
            }
            ok = cmd_pin(model, args[0], args[1], jsonMode);

        } else if (cmd == "connected") {
            if (args.empty()) { dieUsage("connected requires a ref"); return EXIT_FAILURE; }
            ok = cmd_connected(model, args[0], jsonMode);

        } else if (cmd == "compare") {
            if (args.size() < 2) {
                dieUsage("compare requires <ref1> <ref2>"); return EXIT_FAILURE;
            }
            ok = cmd_compare(model, args[0], args[1], jsonMode);

        } else {
            dieUsage("unknown command: " + cmd);
            return EXIT_FAILURE;
        }
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

// Copyright (c) 2026, Maxamilian Kidd-May
// All rights reserved.

// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.
