#include "../inc/nbq.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void dieUsage(const std::string& msg = "") {
    if (!msg.empty()) std::cerr << "error: " << msg << "\n";
    Usage();
}

// ---------------------------------------------------------------------------
// nbq init
// ---------------------------------------------------------------------------

static int run_init(bool jsonMode) {
    fs::path nbqDir = fs::current_path() / ".nbq";

    if (fs::exists(nbqDir)) {
        if (jsonMode) {
            std::cout << "{\"error\":\"already-exists\",\"path\":\""
                      << nbqDir.string() << "\"}\n";
        } else {
            std::cerr << "error: .nbq already exists: " << nbqDir.string() << "\n";
        }
        return EXIT_FAILURE;
    }

    std::error_code ec;
    fs::create_directory(nbqDir, ec);
    if (ec) {
        std::cerr << "error: cannot create .nbq: " << ec.message() << "\n";
        return EXIT_FAILURE;
    }

    {
        std::ofstream f(nbqDir / "config.ini");
        if (!f) {
            std::cerr << "error: cannot create .nbq/config.ini\n";
            return EXIT_FAILURE;
        }
        f << "[files]\n"
          << "partlist = \"./partlist.txt\"\n"
          << "netlist = \"./netlist.txt\"\n"
          << "mcu_hal = \"./mcu_hal.json\"\n";
    }

    {
        std::ofstream f(nbqDir / "mcu_hal.json");
        if (!f) {
            std::cerr << "error: cannot create .nbq/mcu_hal.json\n";
            return EXIT_FAILURE;
        }
        f << "{}\n";
    }

    {
        std::ofstream f(nbqDir / "AGENT.md");
        if (!f) {
            std::cerr << "error: cannot create .nbq/AGENT.md\n";
            return EXIT_FAILURE;
        }
        f << R"(# nbq — Agent Guide

`nbq` is a deterministic CLI query tool for PCB connectivity data exported from
Fusion 360 / EAGLE. It reads a part list and net list, builds a strict in-memory
model, and answers queries grounded entirely in those files. This guide is written
for an AI agent learning to use `nbq` skillfully to traverse a board and assist
with firmware development.

---

## Guarantees you can rely on

- **Deterministic output** — identical inputs always produce identical outputs.
- **No inference** — every answer is grounded in the exported files; nothing is
  assumed or interpolated.
- **No fuzzy matching** — part refs, net names, and pad identifiers must match
  exactly (or pass the explicit case-insensitive fallback for `pin`).
- **First-hop only** — `connected` and `net` list direct neighbours, not
  transitive closure. You must chain calls to traverse further.
- **Exact net names** — net names are case-sensitive. Use `search` or `net` to
  verify the exact string before using it in other commands.

---

## Project layout

After `nbq init` the following files exist:

```
.nbq/
  config.ini    — paths to partlist, netlist, and mcu_hal files
  mcu_hal.json  — MCU signal map (edit this to add MCU pin definitions)
  AGENT.md      — this file
```

`config.ini` format:

```ini
[files]
partlist = "./partlist.txt"
netlist  = "./netlist.txt"
mcu_hal  = "./mcu_hal.json"
```

Paths are resolved relative to `.nbq/`. Place your EAGLE exports alongside
`.nbq/` or update the paths to point at them.

---

## Global flags

```
nbq [--json] <command> [args]
nbq -h | --help
nbq -v | --version
```

`--json` must appear **before** the command name. Use it whenever you need to
parse output programmatically; the JSON schema for every command is documented
below.

---

## Commands

### `summary`

Returns total part and net counts. Use this first to orient yourself on an
unfamiliar board.

```
nbq summary
nbq --json summary
```

JSON schema:
```json
{ "parts": <int>, "nets": <int> }
```

---

### `parts [filter]`

Lists all parts. The optional filter matches case-insensitively against `ref`,
`value`, `device`, `library`, and `package`. No filter = all parts.

```
nbq parts
nbq parts IC
nbq parts BNO055
nbq --json parts R
```

JSON schema:
```json
[
  {
    "ref":     "<string>",
    "value":   "<string>",
    "device":  "<string>",
    "package": "<string>",
    "library": "<string>"
  }
]
```

Use `parts` to enumerate component categories (all ICs, all resistors, all
connectors) before drilling into individual parts.

---

### `search <term>`

Searches case-insensitively across all part fields (`ref`, `value`, `device`,
`library`, `package`) **and** all net names. Returns matching parts and nets.

```
nbq search SDA
nbq search BNO055
nbq --json search VBUS
```

JSON schema:
```json
{
  "parts": [ { "ref":"<string>", "value":"<string>", "device":"<string>",
               "package":"<string>", "library":"<string>" } ],
  "nets":  [ "<string>" ]
}
```

Use `search` when you know a signal name or component keyword but not the
exact ref or net name. The returned net names are the exact strings required
by `net` and `pin`.

---

### `part <ref>`

Returns all metadata and the complete pin→net map for a single part.

```
nbq part IC1
nbq --json part R12
```

JSON schema:
```json
{
  "ref":       "<string>",
  "value":     "<string>",
  "device":    "<string>",
  "package":   "<string>",
  "library":   "<string>",
  "net_count": <int>,
  "pins": [
    { "pad": "<string>", "pin": "<string>", "net": "<string>" }
  ]
}
```

Pins are sorted by pad (numeric pads first, then lexicographic). `net_count`
is the number of distinct nets this part connects to.

---

### `net <name>`

Returns all members of a named net, sorted by `(part, pad)`.

```
nbq net 3V3
nbq net GND
nbq --json net SDA
```

Net names are **case-sensitive**. Obtain the exact name from `search` or from
the `net` field in `part` output before calling this command.

JSON schema:
```json
{
  "net":     "<string>",
  "members": <int>,
  "connections": [
    { "part": "<string>", "pad": "<string>", "pin": "<string>" }
  ]
}
```

---

### `pin <ref> <pad-or-pin>`

Resolves a single pin to its net and lists all other members (peers) on that
net. This is the primary command for single-pin investigation.

```
nbq pin IC1 28
nbq pin IC1 VDD
nbq --json pin R12 2
```

Matching order (stops at first hit):
1. Exact pad match
2. Exact pin-name match
3. Case-insensitive pad match
4. Case-insensitive pin-name match

JSON schema:
```json
{
  "part":  "<string>",
  "pad":   "<string>",
  "pin":   "<string>",
  "net":   "<string>",
  "peers": [
    { "part": "<string>", "pad": "<string>", "pin": "<string>" }
  ]
}
```

---

### `connected <ref>`

Returns the full first-hop topology of a part: for every pin, the net it
belongs to and every other part reachable on that net.

```
nbq connected IC4
nbq --json connected IC1
```

JSON schema:
```json
{
  "part": "<string>",
  "pins": [
    {
      "pad":             "<string>",
      "pin":             "<string>",
      "net":             "<string>",
      "connected_parts": [ "<string>" ]
    }
  ]
}
```

`connected_parts` lists the **refs** of other parts on the same net, sorted
lexicographically. The queried part itself is excluded.

This is the primary board traversal command. One call gives you every
first-hop neighbour of a part.

---

### `compare <ref1> <ref2>`

Compares two parts by their net sets. Shows shared nets and the nets unique
to each part.

```
nbq compare IC1 IC4
nbq --json compare IC5 IC6
```

JSON schema:
```json
{
  "ref1": "<string>",
  "ref2": "<string>",
  "shared": [
    {
      "net":      "<string>",
      "pin_diff": <bool>,
      "<ref1>":   { "pad": "<string>", "pin": "<string>" },
      "<ref2>":   { "pad": "<string>", "pin": "<string>" }
    }
  ],
  "only_<ref1>": [ "<string>" ],
  "only_<ref2>": [ "<string>" ]
}
```

`pin_diff` is `true` when both parts share the net but reach it via different
pad or pin names.

---

### `mcu [ref]`

Without an argument: lists all MCU refs defined in `mcu_hal.json`.
With a ref: lists every signal mapped for that MCU, with silicon pin, net,
pad, and pin name.

```
nbq mcu
nbq mcu IC1
nbq --json mcu IC1
```

JSON schema (no ref):
```json
[ "<ref>", ... ]
```

JSON schema (with ref):
```json
{
  "ref": "<string>",
  "signals": [
    {
      "signal":      "<string>",
      "silicon_pin": "<string>",
      "net":         "<string>",
      "pad":         "<string>",
      "pin":         "<string>"
    }
  ]
}
```

---

### `signal [query]`

Without an argument: lists all signal names defined across all MCU entries.
With a query: resolves by signal name, silicon pin, net name, or pad —
returns the matching signal record(s).

```
nbq signal
nbq signal SCL
nbq signal PA5
nbq --json signal MOSI
```

JSON schema (no query):
```json
[ "<signal_name>", ... ]
```

JSON schema (with query):
```json
[
  {
    "ref":         "<string>",
    "signal":      "<string>",
    "silicon_pin": "<string>",
    "net":         "<string>",
    "pad":         "<string>",
    "pin":         "<string>"
  }
]
```

---

## Board traversal strategy

The tool exposes first-hop relationships only. To traverse a multi-hop path,
chain calls. The recommended traversal workflow:

### 1. Orient yourself

```
nbq summary                 # how many parts and nets?
nbq parts                   # what components exist?
nbq --json parts IC         # which ICs are present?
```

### 2. Start from a known component

```
nbq part IC1                # what pins does IC1 have? which nets?
nbq connected IC1           # what parts is IC1 directly connected to?
```

### 3. Follow a specific signal

```
nbq search SDA              # find the exact net name for SDA
nbq net SDA                 # who is on the SDA net?
nbq pin IC1 SDA             # which pad on IC1 is SDA? who are its peers?
```

### 4. Expand hop by hop

From the `connected_parts` list returned by `connected`, call `part` or
`connected` on each neighbour to continue traversal:

```
nbq --json connected IC1    # find neighbour refs in connected_parts
nbq connected IC3           # expand the next hop
nbq connected J2            # expand further
```

To avoid revisiting parts, maintain your own visited set across calls.

### 5. Investigate a specific connection

```
nbq pin IC3 8               # what net is pad 8 of IC3 on?
nbq net VCC                 # who else is on that net?
nbq compare IC1 IC3         # do IC1 and IC3 share power or other nets?
```

---

## Firmware development workflow

Firmware development requires mapping MCU signal names (e.g. `SPI1_MOSI`,
`I2C1_SCL`) to physical board nets and component connections.

### Step 1 — Populate mcu_hal.json

Edit `.nbq/mcu_hal.json` to describe your MCU's pin assignments. The schema:

```json
{
  "<ref>": {
    "<signal_name>": {
      "silicon_pin": "<string>",
      "net":         "<string>",
      "pad":         "<string>",
      "pin":         "<string>"
    }
  }
}
```

Example:
```json
{
  "IC1": {
    "I2C0_SCL": { "silicon_pin": "P0.11", "net": "SCL", "pad": "11", "pin": "P0.11/TRACEDATA2" },
    "I2C0_SDA": { "silicon_pin": "P0.12", "net": "SDA", "pad": "12", "pin": "P0.12/TRACEDATA1" },
    "SPI0_MOSI": { "silicon_pin": "P0.13", "net": "MOSI", "pad": "13", "pin": "P0.13" }
  }
}
```

### Step 2 — Enumerate MCU signals

```
nbq mcu                     # which MCU refs have signal maps?
nbq mcu IC1                 # what signals are mapped for IC1?
nbq --json mcu IC1          # machine-readable signal list
```

### Step 3 — Resolve a signal to its board net and peers

```
nbq signal SCL              # what net does SCL map to? which pad?
nbq net SCL                 # who else is on the SCL net?
nbq pin IC1 11              # confirm via pad number
```

### Step 4 — Cross-reference peripherals

Given a signal's net, find every component on that net to confirm correct
peripheral wiring:

```
nbq --json signal I2C0_SCL  # get the net name
nbq --json net SCL          # list all connections on that net
```

For each connection in the result, use `part` to verify the component type:

```
nbq part IC3                # is IC3 the expected I2C peripheral?
```

### Step 5 — Validate power and ground

Before using any peripheral in firmware, confirm its power domain:

```
nbq search 3V3              # find the power net name
nbq net 3V3                 # confirm the peripheral is on the correct rail
nbq pin IC3 VCC             # which pad is VCC? what net?
```

---

## MCU HAL JSON format (detailed)

`mcu_hal.json` is a two-level JSON object:

- Top level: MCU reference designator (must match the `ref` in the partlist)
- Second level: signal name → signal record

Signal record fields:

| Field         | Required | Description |
|---------------|----------|-------------|
| `silicon_pin` | yes      | Physical MCU pin identifier (e.g. `P0.11`, `PA5`) |
| `net`         | yes      | Net name as it appears in the netlist |
| `pad`         | yes      | Pad identifier on the MCU component footprint |
| `pin`         | yes      | Pin name from the schematic symbol |

All four fields must be present. Values are case-preserved and matched
case-insensitively by the `signal` command.

---

## Common patterns

### Find all decoupling capacitors near an IC

```
nbq --json connected IC1    # get connected_parts
# for each cap (C*) in connected_parts:
nbq part C14                # confirm it is a bypass cap on 3V3 or VBUS
```

### Trace a bus end-to-end

```
nbq search SDA              # confirm exact net name
nbq --json net SDA          # all parts on the bus
# for each part, check its role:
nbq part IC3                # is this the sensor? the pull-up? the MCU?
```

### Identify pull-up or pull-down resistors

```
nbq net SDA                 # find resistors (R*) in the member list
nbq part R5                 # what value? what other nets does R5 connect?
nbq connected R5            # does the other end go to 3V3 (pull-up) or GND?
```

### Confirm a connector pinout

```
nbq part J1                 # list all pads and their nets
nbq --json part J1          # parse the pin array for firmware use
```

### Check whether two ICs share a bus

```
nbq compare IC1 IC3         # do they share SDA, SCL, or MOSI/MISO/SCK?
```

---

## Error conditions

| Situation | nbq behaviour |
|-----------|--------------|
| `.nbq` not found (searching upward) | Exit with error |
| `partlist` or `netlist` file missing | Exit with clear error |
| Unknown part ref | Print error, exit 1 |
| Unknown net name | Print error, exit 1 |
| Unknown command | Print usage, exit 1 |
| `init` called when `.nbq` already exists | Exit with error |

All errors go to stderr. stdout is reserved for command output. Exit code 0
means success; non-zero means an error occurred.

---

## Quick-reference

| Goal | Command |
|------|---------|
| Orient on board | `nbq summary` + `nbq parts` |
| Find a component | `nbq search <keyword>` |
| Inspect a component | `nbq part <ref>` |
| First-hop topology | `nbq connected <ref>` |
| Inspect a net | `nbq net <name>` |
| Single pin lookup | `nbq pin <ref> <pad-or-pin>` |
| Compare two parts | `nbq compare <ref1> <ref2>` |
| List MCU signals | `nbq mcu [ref]` |
| Resolve a signal | `nbq signal <query>` |
| Machine-readable output | Prepend `--json` |
)";
    }

    if (jsonMode) {
        std::cout << "{\"status\":\"ok\",\"path\":\""
                  << nbqDir.string() << "\"}\n";
    } else {
        std::cout << "initialized nbq project: " << nbqDir.string() << "\n";
    }

    return EXIT_SUCCESS;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {

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

    // ---- init runs before project discovery --------------------------------
    if (cmd == "init") {
        return run_init(jsonMode);
    }

    // ---- Config (requires .nbq project to exist) ---------------------------
    if (InitConfig() != 0) return EXIT_FAILURE;

    // ---- Load board model (not needed for mcu / signal) --------------------
    // mcu and signal commands operate solely on McuHalPath from config;
    // skip the partlist/netlist load for those commands.
    const bool needsModel = (cmd != "mcu" && cmd != "signal");

    Model model;
    if (needsModel) {
        if (!fs::exists(PartlistPath)) {
            std::cerr << "error: partlist not found: " << PartlistPath << "\n";
            return EXIT_FAILURE;
        }
        if (!fs::exists(NetlistPath)) {
            std::cerr << "error: netlist not found: " << NetlistPath << "\n";
            return EXIT_FAILURE;
        }

        try {
            auto parts   = parsePartList(PartlistPath);
            auto netRows = parseNetList(NetlistPath);
            model.build(parts, netRows);
        } catch (const std::exception& e) {
            std::cerr << "error: " << e.what() << "\n";
            return EXIT_FAILURE;
        }
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

        } else if (cmd == "mcu") {
            std::string ref = args.empty() ? "" : args[0];
            ok = cmd_mcu(McuHalPath, ref, jsonMode);

        } else if (cmd == "signal") {
            std::string query = args.empty() ? "" : args[0];
            ok = cmd_signal(McuHalPath, query, jsonMode);

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
