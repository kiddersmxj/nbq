# nbq — Architecture

## Overview

```
config.conf
    │
    ▼
InitConfig()        load DataDir path
    │
    ▼
findFile()          locate Partlist* and Netlist* in DataDir
    │
    ├── parsePartList()   → vector<Part>
    └── parseNetList()    → vector<NetRow>
            │
            ▼
        Model::build()    → Model { parts, nets, partPins }
                │
                ▼
            cmd_*()       → stdout (text or JSON)
```

---

## Source layout

```
inc/
  config.hpp      global config vars, usage/version declarations
  parser.hpp      Part, NetRow structs; file discovery and parse functions
  model.hpp       PinEntry, NetMember, Net, Model; PadOrder sort helper
  commands.hpp    one declaration per command handler
  nbq.hpp         umbrella include

src/
  config.cpp      InitConfig(), Usage(), PrintVersion()
  parser.cpp      findFile(), parsePartList(), parseNetList()
  model.cpp       Model::build(), PadOrder
  commands.cpp    cmd_summary/parts/search/part/net/pin/connected/compare
  nbq.cpp         main() — config load, file discovery, model build, dispatch
```

---

## Internal model

Three ordered maps (all `std::map` — lexicographic key order):

```
parts    : map<ref, Part>
nets     : map<net_name, Net>
partPins : map<ref, vector<PinEntry>>
```

`parts` and `nets` are populated directly from the parsed files.  
`partPins` is a convenience index built during `Model::build()` so that
pin lookups by part require no net-map traversal.

Net members (`Net::members`) and per-part pin lists (`partPins[ref]`) are
sorted at build time and never mutated afterwards, guaranteeing deterministic
output for all commands.

---

## Ordering rules

| Container | Key / sort order |
|-----------|-----------------|
| `parts` | `ref` lexicographic (std::map) |
| `nets` | `net_name` lexicographic (std::map) |
| `Net::members` | `(part, pad)` — pad: numeric < lexicographic |
| `partPins[ref]` | `pad` — numeric < lexicographic |

The `PadOrder` comparator sorts pads that parse as non-negative integers
numerically; all others sort lexicographically after the numeric group.

---

## Parser design

Both parsers use the same strategy:

1. Skip lines until the column header row is detected (by content, not line number)
2. Record the start column of each field from the header using `detectColumns()`
3. Extract fields from each data row by slicing `[col_start, next_col_start)`
4. Trim whitespace from every extracted field

This makes the parser robust to minor changes in EAGLE export layout without
hard-coded column offsets.

For the netlist, continuation rows (those beginning with a space) inherit the
net name from the preceding named row.

---

## Command design

Each command is a free function `void cmd_*(const Model&, ..., bool jsonMode)`.

- All reads are against the already-built model — no file I/O in commands
- `jsonMode` selects output format; the same data path is used for both
- JSON is hand-written: shallow structure, no nesting beyond two levels,
  correct string escaping via `jsonStr()`
- No global state; commands are pure functions of their arguments

---

## File discovery

`findFile(dir, prefix)` iterates `dir` with `std::filesystem::directory_iterator`
and collects all regular files whose names start with `prefix`. If multiple
matches exist, the lexicographically latest filename is returned — this gives
stable behaviour across version-suffixed exports (e.g. `Partlist r0.3.2` wins
over `Partlist r0.3.1`).

Main validates that both files were found before building the model; a missing
file is a hard error.
