# nbq — Data Format Reference

## Source files

`nbq` expects two files exported from Fusion 360 / EAGLE in the `data_dir`
configured in `config.conf`:

| File | Name pattern | Description |
|------|-------------|-------------|
| Part list | `Partlist*` | Component metadata |
| Net list | `Netlist*` | Pin-level connectivity |

Both files are located using a prefix match.  
If multiple versions exist (e.g. `Partlist r0.3.1`, `Partlist r0.3.2`),
the **lexicographically latest** filename is used.

---

## Partlist format

EAGLE fixed-width space-padded export. Line endings: CRLF.

```
Partlist

Exported from <schematic> at <date>

EAGLE Version 9.7.0 ...

Assembly variant:

Part     Value                Device                   Package               Library            Sheet

C1       GRM1885C1H200JA01J   GRM1885C1H200JA01J       CAPC1608X90N          GRM1885C1H200JA01J 1
R3       2k2                  R_CHIP-0402(1005-METRIC) RESC1005X40           Resistor           1
IC1      MDBT50Q-1MV2         MDBT50Q-1MV2             MDBT50Q1MV2           MDBT50Q-1MV2       1
```

**Fields:**

| Column | Description |
|--------|-------------|
| `Part` | Reference designator — unique part identifier (`C1`, `IC1`, `R12`, ...) |
| `Value` | Component value or part number. May be empty for connectors / hierarchy symbols. |
| `Device` | Device name as defined in the EAGLE schematic library |
| `Package` | PCB footprint name |
| `Library` | Source library name |
| `Sheet` | Schematic sheet number (not used by `nbq`) |

Column positions are detected automatically from the header row so format
variations are handled without hard-coded offsets.

---

## Netlist format

Same export tool, CRLF. Net groups are separated by blank lines.

```
Netlist

Exported from <schematic> at <date>

EAGLE Version 9.7.0 ...

Net                Part     Pad      Pin        Sheet

3V3                C14      1        1          1
                   IC1      28       VDD        1
                   R6       1        1          1

GND                IC1      1        GND_1      1
                   IC1      2        GND_2      1
```

**Fields:**

| Column | Description |
|--------|-------------|
| `Net` | Signal name. Present only on the first row of each group; blank for continuation rows. |
| `Part` | Reference designator of the connected part |
| `Pad` | Physical pad identifier on the component (`1`, `28`, `P$2`, `A`, `EP`, ...) |
| `Pin` | Functional pin name from the schematic symbol (`VDD`, `GND_1`, `P1.00`, ...) |
| `Sheet` | Schematic sheet (not used by `nbq`) |

Net-name carry-forward: when `Net` is empty (row begins with whitespace),
the net name from the most recent non-blank Net field is used.

---

## Net name conventions

Net names are preserved exactly as exported. Common patterns in EAGLE exports:

| Pattern | Meaning |
|---------|---------|
| `3V3`, `VBUS`, `GND` | Power rails |
| `SDA`, `SCL`, `MOSI`, `MISO` | Bus signals |
| `!PG!_IC4` | Active-low signal (EAGLE `!` prefix) |
| `!PG(!TE)` | Pin-level negation notation |
| `P1.00`, `P0.03/AIN1` | MCU port pins with optional alternate function |
| `STAT1_IC4` | Schematic-local net with identifying suffix |

`nbq` does not interpret these patterns — net names are treated as opaque strings.

---

## Field normalisation

All parsed fields are whitespace-trimmed (leading and trailing spaces/tabs/CR
stripped). Original casing is preserved. Comparisons in filter and search
operations are case-insensitive.
