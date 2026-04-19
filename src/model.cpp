#include "../inc/model.hpp"

#include <algorithm>
#include <stdexcept>

// ---------------------------------------------------------------------------
// PadOrder
// ---------------------------------------------------------------------------

static bool isNumeric(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    return true;
}

bool PadOrder::operator()(const std::string& a, const std::string& b) const {
    bool na = isNumeric(a), nb = isNumeric(b);
    if (na && nb) return std::stoi(a) < std::stoi(b);
    if (na)       return true;   // numeric < lexicographic
    if (nb)       return false;
    return a < b;
}

// ---------------------------------------------------------------------------
// Model::build
// ---------------------------------------------------------------------------

void Model::build(const std::vector<Part>& partList,
                  const std::vector<NetRow>& netRows) {
    // Populate parts map (sorted by ref via std::map)
    for (const auto& p : partList) {
        parts.emplace(p.ref, p);
    }

    // Build nets and partPins from net rows
    for (const auto& row : netRows) {
        // Net members
        Net& net = nets[row.net];
        if (net.name.empty()) net.name = row.net;
        net.members.push_back({ row.part, row.pad, row.pin });

        // Part pins
        partPins[row.part].push_back({ row.pad, row.pin, row.net });
    }

    // Sort net members by (part, pad) — pad uses PadOrder
    for (auto& [name, net] : nets) {
        std::sort(net.members.begin(), net.members.end(),
            [](const NetMember& a, const NetMember& b) {
                if (a.part != b.part) return a.part < b.part;
                return PadOrder{}(a.pad, b.pad);
            });
    }

    // Sort each part's pin list by pad using PadOrder
    for (auto& [ref, pins] : partPins) {
        std::sort(pins.begin(), pins.end(),
            [](const PinEntry& a, const PinEntry& b) {
                return PadOrder{}(a.pad, b.pad);
            });
    }
}
