# nbq [![CMake](https://img.shields.io/github/actions/workflow/status/kiddersmxj/nbq/cmake.yml?style=for-the-badge)](https://github.com/kiddersmxj/nbq/actions/workflows/cmake.yml)

**Net/Board Query** — a deterministic CLI tool for inspecting PCB connectivity exported from Fusion 360 / EAGLE.

`nbq` reads a part list and net list, builds a strict in-memory connectivity model, and answers precise queries about parts, pins, nets, and relationships. It is designed as a query interface for AI agents and humans alike: low context usage, high precision, repeatable outputs.

---

## Install

Requires the [`std-k`](https://github.com/kiddersmxj/std-k) library.

```bash
./install.sh
```

This configures, builds, and installs the binary to `/usr/local/bin/` and copies `config.conf` to `~/.nbq.conf` (only on first install).

---

## Configuration

`~/.nbq.conf` (or `config.conf` in the working directory):

```toml
[data]
dir = "./data"
```

`data_dir` must contain at least one file matching `Partlist*` and one matching `Netlist*`. If multiple versions exist, the lexicographically latest is used.

---

## Usage

```
nbq [--json] <command> [args]
nbq -h | --help
nbq -v | --version
```

### Commands

| Command | Args | Description |
|---------|------|-------------|
| `summary` | | Part and net counts |
| `parts` | `[filter]` | List parts; filter matches ref/value/device/library/package |
| `search` | `<term>` | Search across all part fields and net names |
| `part` | `<ref>` | Part metadata and full pin→net map |
| `net` | `<name>` | All members of a net |
| `pin` | `<ref> <pad>` | Resolve a pin to its net and all peers |
| `connected` | `<ref>` | First-hop connections for every pin on a part |
| `compare` | `<ref1> <ref2>` | Shared and differing nets between two parts |

All commands support `--json` for structured, machine-readable output.

### Examples

```bash
nbq summary
nbq parts R                    # filter to resistors
nbq part IC1                   # full pin map for IC1
nbq net 3V3                    # all members of the 3V3 net
nbq pin IC1 28                 # resolve pad 28 → net + peers
nbq pin IC1 VDD                # same, matched by pin name
nbq connected IC4              # first-hop topology for IC4
nbq compare IC5 IC6            # diff two parts by net sets
nbq search SDA                 # find SDA across parts and nets
nbq --json summary             # JSON output
nbq --json connected IC1       # JSON for agent consumption
```

---

## Output guarantees

- **Deterministic ordering**: parts by ref, nets by name, members by (part, pad) — pads sorted numerically where applicable
- **No semantic inference**: outputs are grounded strictly in the input files
- **No recursive expansion**: first-hop relationships only
- **Exact matching for `pin`**: exact pad → exact pin name → case-insensitive fallback; no fuzzy matching

---

## Documentation

| File | Content |
|------|---------|
| [`docs/commands.md`](docs/commands.md) | Full command reference with examples and JSON schemas |
| [`docs/data-format.md`](docs/data-format.md) | Input file format specification |
| [`docs/architecture.md`](docs/architecture.md) | Internal model, source layout, design decisions |
