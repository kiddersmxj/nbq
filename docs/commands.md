# nbq — Command Reference

`nbq` is a query interface over PCB connectivity data exported from Fusion/EAGLE.
It builds a strict in-memory model from the source files and answers queries grounded
entirely in that data.

---

## Global flags

```
nbq [--json] <command> [args]
nbq -h | --help
nbq -v | --version
```

| Flag | Effect |
|------|--------|
| `--json` | Output structured JSON instead of human-readable text |
| `-h` / `--help` | Print usage and exit |
| `-v` / `--version` | Print version and exit |

`--json` must appear **before** the command name.

---

## Commands

### `summary`

Print part and net counts.

```
nbq summary
nbq --json summary
```

**Text output**
```
parts : 70
nets  : 76
```

**JSON output**
```json
{
  "parts": 70,
  "nets": 76
}
```

---

### `parts [filter]`

List all parts. Optional `filter` matches case-insensitively against:
`ref`, `value`, `device`, `library`, `package`.

```
nbq parts
nbq parts R
nbq parts BNO055
nbq --json parts IC
```

**Text output** (one part per line: ref  value  device  [package])
```
IC1  MDBT50Q-1MV2  [MDBT50Q1MV2]
IC4  MCP73833T-AMI_MF  [SON50P300X300X100-11N]
```

**JSON output**
```json
[
  {"ref":"IC1","value":"MDBT50Q-1MV2","device":"MDBT50Q-1MV2","package":"MDBT50Q1MV2","library":"MDBT50Q-1MV2"},
  ...
]
```

---

### `search <term>`

Search across all part fields and all net names.

```
nbq search SDA
nbq search BNO
nbq --json search VBUS
```

**Text output**
```
parts:
  IC3  BNO055
nets:
  SDA
```

**JSON output**
```json
{
  "parts": [...],
  "nets": ["SDA"]
}
```

---

### `part <ref>`

Part metadata, pin/pad→net map, and total distinct net count.

```
nbq part IC1
nbq --json part R12
```

**Text output**
```
ref     : IC1
value   : MDBT50Q-1MV2
device  : MDBT50Q-1MV2
package : MDBT50Q1MV2
library : MDBT50Q-1MV2
nets    : 56
pins:
  pad 1  (GND_1)  →  GND
  pad 2  (GND_2)  →  GND
  pad 28 (VDD)    →  3V3
  ...
```

**JSON output**
```json
{
  "ref": "IC1",
  "value": "MDBT50Q-1MV2",
  "device": "MDBT50Q-1MV2",
  "package": "MDBT50Q1MV2",
  "library": "MDBT50Q-1MV2",
  "net_count": 56,
  "pins": [
    {"pad":"1","pin":"GND_1","net":"GND"},
    ...
  ]
}
```

Pins are sorted by pad (numeric pads first, then lexicographic).

---

### `net <name>`

All members of a named net, sorted by (part, pad).

```
nbq net 3V3
nbq net GND
nbq --json net SDA
```

Net names are **case-sensitive** and must match exactly as exported.

**Text output**
```
net     : 3V3
members : 29
  C14  pad 1
  IC1  pad 28  (VDD)
  R6   pad 1
  ...
```

**JSON output**
```json
{
  "net": "3V3",
  "members": 29,
  "connections": [
    {"part":"C14","pad":"1","pin":"1"},
    {"part":"IC1","pad":"28","pin":"VDD"},
    ...
  ]
}
```

---

### `pin <ref> <pad-or-pin>`

Resolve a single pin to its net and list all other members on that net.

```
nbq pin IC1 28
nbq pin IC1 VDD
nbq --json pin R12 2
```

**Matching priority** (stops at first hit — no fuzzy matching):
1. Exact pad match
2. Exact pin-name match
3. Case-insensitive pad match
4. Case-insensitive pin-name match

**Text output**
```
part : IC1
pad  : 28
pin  : VDD
net  : 3V3
peers:
  C14  pad 1
  IC3  pad 8  (VCC)
  ...
```

**JSON output**
```json
{
  "part": "IC1",
  "pad": "28",
  "pin": "VDD",
  "net": "3V3",
  "peers": [
    {"part":"C14","pad":"1","pin":"1"},
    ...
  ]
}
```

---

### `connected <ref>`

For every pin on a part, show the net and the other parts reachable on that net.
Deduplication is per-net only — a part appearing on multiple pins will be listed
separately under each.

```
nbq connected IC4
nbq --json connected R12
```

**Text output**
```
connected: IC4
  pad 1 (VDD_1)  →  VBUS
    C3
    IC1
    J4
  pad 7 (!PG(!TE))  →  !PG!_IC4
    IC1
    R12
    R25
  ...
```

**JSON output**
```json
{
  "part": "IC4",
  "pins": [
    {"pad":"1","pin":"VDD_1","net":"VBUS","connected_parts":["C3","IC1","J4",...]},
    ...
  ]
}
```

---

### `compare <ref1> <ref2>`

Compare two parts by their net sets.

```
nbq compare IC1 IC4
nbq --json compare IC5 IC6
```

**Output sections:**

| Section | Meaning |
|---------|---------|
| `shared nets` | Nets present on both parts. Annotated with pad numbers where they differ. |
| `only <ref1>` | Nets present only on the first part. |
| `only <ref2>` | Nets present only on the second part. |

**Text output**
```
compare: IC1 vs IC4

shared nets (5):
  GND  [IC1 pad 1 / IC4 pad 5]
  VBUS  [IC1 pad 32 / IC4 pad 1]
  ...

only IC1 (51):
  3V3
  SDA
  ...

only IC4 (3):
  IC4_PROG
  VBAT
  ...
```

**JSON output**
```json
{
  "ref1": "IC1",
  "ref2": "IC4",
  "shared": [
    {"net":"GND","pin_diff":true,"IC1":{"pad":"1","pin":"GND_1"},"IC4":{"pad":"5","pin":"VSS"}},
    ...
  ],
  "only_IC1": ["3V3","SDA",...],
  "only_IC4": ["IC4_PROG","VBAT","THERM_BAT"]
}
```

The `pin_diff` flag is `true` when the same net is reached via different pad or pin names on the two parts.

---

## Output ordering

All outputs are deterministic:

- Parts listed by `ref` (lexicographic)
- Nets listed by `name` (lexicographic)
- Net members sorted by `(part, pad)` — pads sorted numerically where possible
- Per-part pin lists sorted by `pad` — numeric pads first

---

## Error handling

| Situation | Behaviour |
|-----------|-----------|
| `partlist*` or `netlist*` not found in `data_dir` | Exit with clear error |
| Only one of the two files found | Exit with clear error |
| Unknown part / net reference | Print error message, exit 1 |
| Unknown command | Print usage, exit 1 |
