#include "../inc/parser.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static std::string stripCR(std::string s) {
    if (!s.empty() && s.back() == '\r') s.pop_back();
    return s;
}

static std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    auto a = s.find_first_not_of(ws);
    if (a == std::string::npos) return {};
    auto b = s.find_last_not_of(ws);
    return s.substr(a, b - a + 1);
}

// Locate the start position of each column by scanning the header line for
// runs of non-space characters.  Returns a vector of {name, start_col}.
struct ColDef { std::string name; size_t col; };

static std::vector<ColDef> detectColumns(const std::string& header) {
    std::vector<ColDef> cols;
    bool in_word = false;
    size_t start = 0;
    for (size_t i = 0; i <= header.size(); ++i) {
        bool sp = (i == header.size() || header[i] == ' ');
        if (!sp && !in_word) {
            start = i; in_word = true;
        } else if (sp && in_word) {
            cols.push_back({ header.substr(start, i - start), start });
            in_word = false;
        }
    }
    return cols;
}

// Extract one field: characters in [cols[idx].col, cols[idx+1].col), trimmed.
// If idx is the last column, read to end of line.
static std::string extractField(const std::string& line,
                                const std::vector<ColDef>& cols,
                                size_t idx) {
    size_t start = cols[idx].col;
    size_t end   = (idx + 1 < cols.size()) ? cols[idx + 1].col : line.size();
    if (start >= line.size()) return {};
    end = std::min(end, line.size());
    return trim(line.substr(start, end - start));
}

// ---------------------------------------------------------------------------
// parsePartList
// ---------------------------------------------------------------------------
//
// File structure (EAGLE export, CRLF):
//   Line 1:  "Partlist"
//   Lines 2-8: metadata / blank / "Assembly variant:"
//   Line 9:  column header ("Part   Value   Device   Package   Library   Sheet")
//   Line 10: blank
//   Lines 11+: data rows
//
// Columns are space-padded to fixed widths; some rows have an empty Value.

std::vector<Part> parsePartList(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open partlist: " + path);

    std::vector<Part> parts;
    std::string line;
    std::vector<ColDef> cols;
    bool headerFound = false;

    while (std::getline(f, line)) {
        line = stripCR(line);

        // Detect header row: contains "Part" as the very first non-space token
        if (!headerFound) {
            auto t = trim(line);
            if (t.size() >= 4 && t.substr(0, 4) == "Part" &&
                t.find("Value") != std::string::npos) {
                cols = detectColumns(line);
                headerFound = true;
            }
            continue;
        }

        // Skip blank lines
        if (trim(line).empty()) continue;

        // We need at least a "Part" column (index 0)
        if (cols.empty()) continue;

        // Build column name → index map for safety
        // Expected order: Part Value Device Package Library Sheet
        auto col = [&](const std::string& name) -> size_t {
            for (size_t i = 0; i < cols.size(); ++i)
                if (cols[i].name == name) return i;
            return SIZE_MAX;
        };

        size_t iRef  = col("Part");
        size_t iVal  = col("Value");
        size_t iDev  = col("Device");
        size_t iPkg  = col("Package");
        size_t iLib  = col("Library");

        if (iRef == SIZE_MAX) continue;

        Part p;
        p.ref     = extractField(line, cols, iRef);
        p.value   = (iVal  != SIZE_MAX) ? extractField(line, cols, iVal)  : "";
        p.device  = (iDev  != SIZE_MAX) ? extractField(line, cols, iDev)  : "";
        p.package = (iPkg  != SIZE_MAX) ? extractField(line, cols, iPkg)  : "";
        p.library = (iLib  != SIZE_MAX) ? extractField(line, cols, iLib)  : "";

        if (p.ref.empty()) continue;  // safety: skip malformed rows
        parts.push_back(std::move(p));
    }

    return parts;
}

// ---------------------------------------------------------------------------
// parseNetList
// ---------------------------------------------------------------------------
//
// File structure:
//   Line 1:  "Netlist"
//   Lines 2-6: metadata / blank
//   Line 7:  column header ("Net   Part   Pad   Pin   Sheet")
//   Line 8:  blank
//   Lines 9+: data rows, grouped by net.
//             First row in each group has the Net name at column 0.
//             Subsequent rows for the same net have spaces at column 0.
//             Blank lines separate groups (ignored).

std::vector<NetRow> parseNetList(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open netlist: " + path);

    std::vector<NetRow> rows;
    std::string line;
    std::vector<ColDef> cols;
    bool headerFound = false;
    std::string currentNet;

    while (std::getline(f, line)) {
        line = stripCR(line);

        if (!headerFound) {
            auto t = trim(line);
            if (t.size() >= 3 && t.substr(0, 3) == "Net" &&
                t.find("Part") != std::string::npos) {
                cols = detectColumns(line);
                headerFound = true;
            }
            continue;
        }

        // Skip blank lines (they separate net groups)
        if (trim(line).empty()) continue;

        if (cols.empty()) continue;

        auto col = [&](const std::string& name) -> size_t {
            for (size_t i = 0; i < cols.size(); ++i)
                if (cols[i].name == name) return i;
            return SIZE_MAX;
        };

        size_t iNet  = col("Net");
        size_t iPart = col("Part");
        size_t iPad  = col("Pad");
        size_t iPin  = col("Pin");

        if (iPart == SIZE_MAX) continue;

        // Detect whether this row begins a new net:
        // the first character of the line is non-space iff it carries a net name.
        bool newNet = (!line.empty() && line[0] != ' ');

        if (newNet && iNet != SIZE_MAX) {
            std::string n = extractField(line, cols, iNet);
            if (!n.empty()) currentNet = n;
        }

        NetRow r;
        r.net  = currentNet;
        r.part = (iPart != SIZE_MAX) ? extractField(line, cols, iPart) : "";
        r.pad  = (iPad  != SIZE_MAX) ? extractField(line, cols, iPad)  : "";
        r.pin  = (iPin  != SIZE_MAX) ? extractField(line, cols, iPin)  : "";

        if (r.part.empty() || r.net.empty()) continue;
        rows.push_back(std::move(r));
    }

    return rows;
}
