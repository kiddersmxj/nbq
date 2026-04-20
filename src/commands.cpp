#include "../inc/commands.hpp"
#include "../inc/mcu.hpp"

#include <algorithm>
#include <deque>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>

// ---------------------------------------------------------------------------
// JSON helpers
// ---------------------------------------------------------------------------

static std::string jsonStr(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    out += '"';
    return out;
}

// Emit a JSON error object to stdout and return false.
static bool jsonError(const std::string& msg) {
    std::cout << "{\"error\":" << jsonStr(msg) << "}\n";
    return false;
}

// ---------------------------------------------------------------------------
// Case-insensitive helpers
// ---------------------------------------------------------------------------

static std::string toLower(std::string s) {
    for (char& c : s) c = static_cast<char>(
        std::tolower(static_cast<unsigned char>(c)));
    return s;
}

static bool containsCI(const std::string& haystack, const std::string& needle) {
    return toLower(haystack).find(toLower(needle)) != std::string::npos;
}

static bool equalsCI(const std::string& a, const std::string& b) {
    return toLower(a) == toLower(b);
}

// ---------------------------------------------------------------------------
// cmd_summary
// ---------------------------------------------------------------------------

bool cmd_summary(const Model& m, const std::string& mcuMapFile, bool jsonMode) {
    // Part type breakdown by leading-alpha refdes prefix
    std::map<std::string, std::vector<std::string>> byPrefix;
    for (const auto& [ref, part] : m.parts) {
        std::string prefix;
        for (char c : ref)
            if (std::isalpha(static_cast<unsigned char>(c))) prefix += c;
            else break;
        if (prefix.empty()) prefix = "?";
        byPrefix[prefix].push_back(ref);
    }

    // Power/ground rail detection by net name
    std::vector<std::string> powerRails;
    for (const auto& [name, net] : m.nets) {
        std::string up = name;
        for (char& c : up) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        bool isPower = false;
        if (up.find("GND") != std::string::npos || up.find("VSS") != std::string::npos)
            isPower = true;
        else if (!up.empty() && up[0] == 'V' && up.size() > 1 &&
                 (std::isupper(static_cast<unsigned char>(up[1])) ||
                  std::isdigit(static_cast<unsigned char>(up[1]))))
            isPower = true;
        else if (!up.empty() && std::isdigit(static_cast<unsigned char>(up[0])) &&
                 up.find('V') != std::string::npos)
            isPower = true;
        if (isPower) powerRails.push_back(name);
    }
    std::sort(powerRails.begin(), powerRails.end());

    // Top 5 nets by member count
    std::vector<std::pair<size_t, std::string>> netsBySize;
    netsBySize.reserve(m.nets.size());
    for (const auto& [name, net] : m.nets)
        netsBySize.push_back({net.members.size(), name});
    std::sort(netsBySize.begin(), netsBySize.end(),
              [](const auto& a, const auto& b) {
                  return a.first != b.first ? a.first > b.first : a.second < b.second;
              });
    if (netsBySize.size() > 5) netsBySize.resize(5);

    // Unconnected parts (in partlist but absent from netlist)
    size_t unconnected = 0;
    for (const auto& [ref, part] : m.parts)
        if (m.partPins.find(ref) == m.partPins.end()) ++unconnected;

    // MCU HAL map (optional; silently skip if missing/empty)
    McuMap mcuMap;
    if (!mcuMapFile.empty()) {
        try { mcuMap = loadMcuMap(mcuMapFile); } catch (...) {}
    }

    if (jsonMode) {
        std::cout << "{\n";
        std::cout << "  \"parts\": " << m.parts.size() << ",\n";
        std::cout << "  \"nets\": " << m.nets.size() << ",\n";

        // MCUs
        std::cout << "  \"mcus\": [";
        if (!mcuMap.empty()) {
            bool first = true;
            for (const auto& [ref, def] : mcuMap) {
                if (!first) std::cout << ",";
                first = false;
                std::string value;
                auto it = m.parts.find(ref);
                if (it != m.parts.end()) value = it->second.value;
                std::cout << "\n    {\"ref\": " << jsonStr(ref);
                if (!value.empty()) std::cout << ", \"value\": " << jsonStr(value);
                std::cout << ", \"mapped_signals\": " << def.pins.size() << "}";
            }
            std::cout << "\n  ";
        }
        std::cout << "],\n";

        // Part types
        std::cout << "  \"part_types\": {";
        bool firstPt = true;
        for (const auto& [pfx, refs] : byPrefix) {
            if (!firstPt) std::cout << ",";
            firstPt = false;
            std::cout << "\n    " << jsonStr(pfx) << ": {\"count\": " << refs.size();
            if (refs.size() <= 5) {
                std::cout << ", \"refs\": [";
                for (size_t i = 0; i < refs.size(); ++i) {
                    if (i) std::cout << ", ";
                    std::cout << jsonStr(refs[i]);
                }
                std::cout << "]";
            }
            std::cout << "}";
        }
        if (!byPrefix.empty()) std::cout << "\n  ";
        std::cout << "},\n";

        // Power rails
        std::cout << "  \"power_rails\": [";
        for (size_t i = 0; i < powerRails.size(); ++i) {
            if (i) std::cout << ", ";
            std::cout << jsonStr(powerRails[i]);
        }
        std::cout << "],\n";

        // Top nets
        std::cout << "  \"top_nets\": [";
        for (size_t i = 0; i < netsBySize.size(); ++i) {
            if (i) std::cout << ",";
            std::cout << "\n    {\"name\": " << jsonStr(netsBySize[i].second)
                      << ", \"pins\": " << netsBySize[i].first << "}";
        }
        if (!netsBySize.empty()) std::cout << "\n  ";
        std::cout << "]";

        if (unconnected > 0)
            std::cout << ",\n  \"unconnected_parts\": " << unconnected;

        std::cout << "\n}\n";

    } else {
        std::cout << "parts : " << m.parts.size() << "\n";
        std::cout << "nets  : " << m.nets.size() << "\n";

        // MCUs
        if (!mcuMap.empty()) {
            std::cout << "\nmcus\n";
            for (const auto& [ref, def] : mcuMap) {
                std::string value;
                auto it = m.parts.find(ref);
                if (it != m.parts.end()) value = it->second.value;
                std::cout << "  " << ref;
                if (!value.empty()) std::cout << "  " << value;
                std::cout << "  (" << def.pins.size() << " signals mapped)\n";
            }
        }

        // Part types
        std::cout << "\npart types\n";
        for (const auto& [pfx, refs] : byPrefix) {
            std::cout << "  " << std::left << std::setw(8) << pfx
                      << " : " << std::right << std::setw(4) << refs.size();
            if (refs.size() <= 5) {
                std::cout << "  (";
                for (size_t i = 0; i < refs.size(); ++i) {
                    if (i) std::cout << "  ";
                    std::cout << refs[i];
                }
                std::cout << ")";
            }
            std::cout << "\n";
        }

        // Power rails
        if (!powerRails.empty()) {
            std::cout << "\npower rails\n  ";
            for (size_t i = 0; i < powerRails.size(); ++i) {
                if (i) std::cout << "  ";
                std::cout << powerRails[i];
            }
            std::cout << "\n";
        }

        // Top nets by connection count
        if (!netsBySize.empty()) {
            std::cout << "\ntop nets by connections\n";
            for (const auto& [count, name] : netsBySize) {
                std::cout << "  " << std::left << std::setw(24) << name
                          << " : " << count << " pins\n";
            }
        }

        if (unconnected > 0)
            std::cout << "\nunconnected parts : " << unconnected << "\n";
    }
    return true;
}

// ---------------------------------------------------------------------------
// cmd_parts
// ---------------------------------------------------------------------------

// A filter is a refdes prefix query if every character is a letter or '$'.
// Examples: "R", "IC", "LED", "G$" → prefix match against ref only.
// Examples: "BNO055", "CHIP" (contains digits or unusual chars) → broad search.
static bool isRefdesPrefix(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s)
        if (!std::isalpha(static_cast<unsigned char>(c)) && c != '$')
            return false;
    return true;
}

static bool startsWithCI(const std::string& s, const std::string& prefix) {
    if (s.size() < prefix.size()) return false;
    return equalsCI(s.substr(0, prefix.size()), prefix);
}

bool cmd_parts(const Model& m, const std::string& filter, bool jsonMode) {
    const bool refdesMode = isRefdesPrefix(filter);

    std::vector<const Part*> matches;
    for (const auto& [ref, p] : m.parts) {
        bool hit = filter.empty();
        if (!hit) {
            if (refdesMode)
                hit = startsWithCI(p.ref, filter);
            else
                hit = containsCI(p.ref,     filter) ||
                      containsCI(p.value,   filter) ||
                      containsCI(p.device,  filter) ||
                      containsCI(p.library, filter) ||
                      containsCI(p.package, filter);
        }
        if (hit) matches.push_back(&p);
    }

    if (jsonMode) {
        std::cout << "[\n";
        for (size_t i = 0; i < matches.size(); ++i) {
            const auto& p = *matches[i];
            std::cout << "  {"
                      << "\"ref\":"     << jsonStr(p.ref)     << ","
                      << "\"value\":"   << jsonStr(p.value)   << ","
                      << "\"device\":"  << jsonStr(p.device)  << ","
                      << "\"package\":" << jsonStr(p.package) << ","
                      << "\"library\":" << jsonStr(p.library)
                      << "}";
            if (i + 1 < matches.size()) std::cout << ",";
            std::cout << "\n";
        }
        std::cout << "]\n";
    } else {
        for (const auto* p : matches) {
            std::cout << p->ref;
            if (!p->value.empty())  std::cout << "  " << p->value;
            if (!p->device.empty() && p->device != p->value)
                                    std::cout << "  " << p->device;
            if (!p->package.empty()) std::cout << "  [" << p->package << "]";
            std::cout << "\n";
        }
        if (matches.empty()) std::cout << "(no matches)\n";
    }
    return true;
}

// ---------------------------------------------------------------------------
// cmd_search
// ---------------------------------------------------------------------------

bool cmd_search(const Model& m, const std::string& term, bool jsonMode) {
    std::vector<const Part*> partHits;
    for (const auto& [ref, p] : m.parts) {
        if (containsCI(p.ref,     term) ||
            containsCI(p.value,   term) ||
            containsCI(p.device,  term) ||
            containsCI(p.library, term) ||
            containsCI(p.package, term))
            partHits.push_back(&p);
    }

    std::vector<const Net*> netHits;
    for (const auto& [name, n] : m.nets)
        if (containsCI(name, term)) netHits.push_back(&n);

    if (jsonMode) {
        std::cout << "{\n  \"parts\": [\n";
        for (size_t i = 0; i < partHits.size(); ++i) {
            const auto& p = *partHits[i];
            std::cout << "    {"
                      << "\"ref\":"     << jsonStr(p.ref)     << ","
                      << "\"value\":"   << jsonStr(p.value)   << ","
                      << "\"device\":"  << jsonStr(p.device)  << ","
                      << "\"package\":" << jsonStr(p.package) << ","
                      << "\"library\":" << jsonStr(p.library)
                      << "}";
            if (i + 1 < partHits.size()) std::cout << ",";
            std::cout << "\n";
        }
        std::cout << "  ],\n  \"nets\": [\n";
        for (size_t i = 0; i < netHits.size(); ++i) {
            std::cout << "    " << jsonStr(netHits[i]->name);
            if (i + 1 < netHits.size()) std::cout << ",";
            std::cout << "\n";
        }
        std::cout << "  ]\n}\n";
    } else {
        if (!partHits.empty()) {
            std::cout << "parts:\n";
            for (const auto* p : partHits) {
                std::cout << "  " << p->ref;
                if (!p->value.empty())  std::cout << "  " << p->value;
                if (!p->device.empty() && p->device != p->value)
                                        std::cout << "  " << p->device;
                std::cout << "\n";
            }
        }
        if (!netHits.empty()) {
            std::cout << "nets:\n";
            for (const auto* n : netHits) std::cout << "  " << n->name << "\n";
        }
        if (partHits.empty() && netHits.empty())
            std::cout << "(no matches)\n";
    }
    return true;
}

// ---------------------------------------------------------------------------
// cmd_part
// ---------------------------------------------------------------------------

bool cmd_part(const Model& m, const std::string& ref, bool jsonMode) {
    auto it = m.parts.find(ref);
    if (it == m.parts.end()) {
        if (jsonMode) return jsonError("part not found: " + ref);
        std::cerr << "error: part not found: " << ref << "\n";
        return false;
    }
    const Part& p = it->second;

    std::set<std::string> netSet;
    auto pit = m.partPins.find(ref);
    if (pit != m.partPins.end())
        for (const auto& pe : pit->second) netSet.insert(pe.net);

    if (jsonMode) {
        std::cout << "{\n"
                  << "  \"ref\":"       << jsonStr(p.ref)     << ",\n"
                  << "  \"value\":"     << jsonStr(p.value)   << ",\n"
                  << "  \"device\":"    << jsonStr(p.device)  << ",\n"
                  << "  \"package\":"   << jsonStr(p.package) << ",\n"
                  << "  \"library\":"   << jsonStr(p.library) << ",\n"
                  << "  \"net_count\":" << netSet.size()      << ",\n"
                  << "  \"pins\": [\n";
        if (pit != m.partPins.end()) {
            const auto& pins = pit->second;
            for (size_t i = 0; i < pins.size(); ++i) {
                std::cout << "    {"
                          << "\"pad\":" << jsonStr(pins[i].pad) << ","
                          << "\"pin\":" << jsonStr(pins[i].pin) << ","
                          << "\"net\":" << jsonStr(pins[i].net)
                          << "}";
                if (i + 1 < pins.size()) std::cout << ",";
                std::cout << "\n";
            }
        }
        std::cout << "  ]\n}\n";
    } else {
        std::cout << "ref     : " << p.ref     << "\n"
                  << "value   : " << p.value   << "\n"
                  << "device  : " << p.device  << "\n"
                  << "package : " << p.package << "\n"
                  << "library : " << p.library << "\n"
                  << "nets    : " << netSet.size() << "\n";
        if (pit != m.partPins.end()) {
            std::cout << "pins:\n";
            for (const auto& pe : pit->second) {
                std::cout << "  pad " << pe.pad;
                if (!pe.pin.empty() && pe.pin != pe.pad)
                    std::cout << "  (" << pe.pin << ")";
                std::cout << "  →  " << pe.net << "\n";
            }
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// cmd_net
// ---------------------------------------------------------------------------

bool cmd_net(const Model& m, const std::string& name, bool jsonMode) {
    auto it = m.nets.find(name);
    if (it == m.nets.end()) {
        if (jsonMode) return jsonError("net not found: " + name);
        std::cerr << "error: net not found: " << name << "\n";
        return false;
    }
    const Net& net = it->second;

    if (jsonMode) {
        std::cout << "{\n"
                  << "  \"net\":"     << jsonStr(net.name)  << ",\n"
                  << "  \"members\":" << net.members.size() << ",\n"
                  << "  \"connections\": [\n";
        for (size_t i = 0; i < net.members.size(); ++i) {
            const auto& mb = net.members[i];
            std::cout << "    {"
                      << "\"part\":" << jsonStr(mb.part) << ","
                      << "\"pad\":"  << jsonStr(mb.pad)  << ","
                      << "\"pin\":"  << jsonStr(mb.pin)
                      << "}";
            if (i + 1 < net.members.size()) std::cout << ",";
            std::cout << "\n";
        }
        std::cout << "  ]\n}\n";
    } else {
        std::cout << "net     : " << net.name << "\n"
                  << "members : " << net.members.size() << "\n";
        for (const auto& mb : net.members) {
            std::cout << "  " << mb.part << "  pad " << mb.pad;
            if (!mb.pin.empty() && mb.pin != mb.pad)
                std::cout << "  (" << mb.pin << ")";
            std::cout << "\n";
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// cmd_pin
// ---------------------------------------------------------------------------

bool cmd_pin(const Model& m, const std::string& ref,
             const std::string& pad_or_pin, bool jsonMode) {
    auto pit = m.partPins.find(ref);
    if (pit == m.partPins.end()) {
        if (jsonMode) return jsonError("part not found: " + ref);
        std::cerr << "error: part not found: " << ref << "\n";
        return false;
    }
    const auto& pins = pit->second;

    // Matching priority: exact pad → exact pin → case-insensitive pad → CI pin
    const PinEntry* found = nullptr;
    for (const auto& pe : pins) if (pe.pad == pad_or_pin) { found = &pe; break; }
    if (!found)
        for (const auto& pe : pins) if (pe.pin == pad_or_pin) { found = &pe; break; }
    if (!found)
        for (const auto& pe : pins) if (equalsCI(pe.pad, pad_or_pin)) { found = &pe; break; }
    if (!found)
        for (const auto& pe : pins) if (equalsCI(pe.pin, pad_or_pin)) { found = &pe; break; }

    if (!found) {
        if (jsonMode) return jsonError("pin not found: " + ref + " " + pad_or_pin);
        std::cerr << "error: pin not found: " << ref << " " << pad_or_pin << "\n";
        return false;
    }

    std::vector<NetMember> others;
    auto nit = m.nets.find(found->net);
    if (nit != m.nets.end()) {
        for (const auto& mb : nit->second.members)
            if (mb.part != ref || mb.pad != found->pad)
                others.push_back(mb);
    }

    if (jsonMode) {
        std::cout << "{\n"
                  << "  \"part\":" << jsonStr(ref)        << ",\n"
                  << "  \"pad\":"  << jsonStr(found->pad) << ",\n"
                  << "  \"pin\":"  << jsonStr(found->pin) << ",\n"
                  << "  \"net\":"  << jsonStr(found->net) << ",\n"
                  << "  \"peers\": [\n";
        for (size_t i = 0; i < others.size(); ++i) {
            std::cout << "    {"
                      << "\"part\":" << jsonStr(others[i].part) << ","
                      << "\"pad\":"  << jsonStr(others[i].pad)  << ","
                      << "\"pin\":"  << jsonStr(others[i].pin)
                      << "}";
            if (i + 1 < others.size()) std::cout << ",";
            std::cout << "\n";
        }
        std::cout << "  ]\n}\n";
    } else {
        std::cout << "part : " << ref        << "\n"
                  << "pad  : " << found->pad << "\n";
        if (!found->pin.empty() && found->pin != found->pad)
            std::cout << "pin  : " << found->pin << "\n";
        std::cout << "net  : " << found->net << "\n"
                  << "peers:\n";
        for (const auto& mb : others) {
            std::cout << "  " << mb.part << "  pad " << mb.pad;
            if (!mb.pin.empty() && mb.pin != mb.pad)
                std::cout << "  (" << mb.pin << ")";
            std::cout << "\n";
        }
        if (others.empty()) std::cout << "  (none)\n";
    }
    return true;
}

// ---------------------------------------------------------------------------
// cmd_connected — compact output helpers
// ---------------------------------------------------------------------------

static constexpr size_t kPeerInlineMax = 6;

static bool isRailNet(const std::string& name) {
    if (name.empty()) return false;
    if (name.find("GND") != std::string::npos) return true;
    if (name[0] == 'V' && name.size() > 1 &&
        (std::isupper(static_cast<unsigned char>(name[1])) ||
         std::isdigit(static_cast<unsigned char>(name[1])))) return true;
    if (std::isdigit(static_cast<unsigned char>(name[0])) &&
        name.find('V') != std::string::npos) return true;
    return false;
}

// ---------------------------------------------------------------------------
// cmd_connected
// ---------------------------------------------------------------------------

bool cmd_connected(const Model& m, const std::string& ref, bool jsonMode) {
    auto pit = m.partPins.find(ref);
    if (pit == m.partPins.end()) {
        if (jsonMode) return jsonError("part not found: " + ref);
        std::cerr << "error: part not found: " << ref << "\n";
        return false;
    }

    struct PinInfo {
        const PinEntry*          pe;
        std::vector<std::string> peers;
        bool                     showPeers;
    };
    std::vector<PinInfo> rows;
    for (const auto& pe : pit->second) {
        std::vector<std::string> peers;
        std::set<std::string>    seen;
        auto nit = m.nets.find(pe.net);
        if (nit != m.nets.end()) {
            for (const auto& mb : nit->second.members)
                if (mb.part != ref && seen.insert(mb.part).second)
                    peers.push_back(mb.part);
        }
        if (peers.empty()) continue;
        bool elide = isRailNet(pe.net) || peers.size() > kPeerInlineMax;
        rows.push_back({ &pe, std::move(peers), !elide });
    }

    if (jsonMode) {
        std::cout << "{\n  \"part\":" << jsonStr(ref) << ",\n  \"pins\": [\n";
        for (size_t i = 0; i < rows.size(); ++i) {
            const auto& r = rows[i];
            std::cout << "    {"
                      << "\"pad\":"         << jsonStr(r.pe->pad) << ","
                      << "\"pin\":"         << jsonStr(r.pe->pin) << ","
                      << "\"net\":"         << jsonStr(r.pe->net) << ","
                      << "\"peer_count\":"  << r.peers.size()     << ","
                      << "\"peers_elided\":" << (r.showPeers ? "false" : "true") << ","
                      << "\"peers\":[";
            if (r.showPeers) {
                for (size_t j = 0; j < r.peers.size(); ++j) {
                    std::cout << jsonStr(r.peers[j]);
                    if (j + 1 < r.peers.size()) std::cout << ",";
                }
            }
            std::cout << "]}";
            if (i + 1 < rows.size()) std::cout << ",";
            std::cout << "\n";
        }
        std::cout << "  ]\n}\n";
    } else {
        std::cout << "connected: " << ref << "\n";
        for (const auto& r : rows) {
            const auto& pe = *r.pe;
            std::cout << "  pad " << pe.pad;
            if (!pe.pin.empty() && pe.pin != pe.pad)
                std::cout << " (" << pe.pin << ")";
            std::cout << "  →  " << pe.net << "  ";
            if (r.showPeers) {
                std::cout << "[" << r.peers.size()
                          << (r.peers.size() == 1 ? " peer: " : " peers: ");
                for (size_t j = 0; j < r.peers.size(); ++j) {
                    std::cout << r.peers[j];
                    if (j + 1 < r.peers.size()) std::cout << ", ";
                }
                std::cout << "]\n";
            } else {
                std::cout << "[" << r.peers.size()
                          << (r.peers.size() == 1 ? " peer]\n" : " peers]\n");
            }
        }
        if (rows.empty()) std::cout << "  (no connected pins)\n";
    }
    return true;
}

// ---------------------------------------------------------------------------
// cmd_compare
// ---------------------------------------------------------------------------

bool cmd_compare(const Model& m, const std::string& ref1,
                 const std::string& ref2, bool jsonMode) {
    bool ok1 = m.parts.count(ref1), ok2 = m.parts.count(ref2);
    if (!ok1 || !ok2) {
        std::string missing = (!ok1 ? ref1 : ref2);
        if (jsonMode) return jsonError("part not found: " + missing);
        std::cerr << "error: part not found: " << missing << "\n";
        return false;
    }

    // Build net → all PinEntries for each part (preserve pad-sorted order from model)
    using PinVec = std::vector<PinEntry>;
    auto buildNetMap = [&](const std::string& ref) -> std::map<std::string, PinVec> {
        std::map<std::string, PinVec> nm;
        auto it = m.partPins.find(ref);
        if (it != m.partPins.end())
            for (const auto& pe : it->second)
                nm[pe.net].push_back(pe);   // accumulate all pins per net
        return nm;
    };

    auto nm1 = buildNetMap(ref1);
    auto nm2 = buildNetMap(ref2);

    // Classify nets
    struct SharedNet {
        std::string net;
        PinVec      pins1, pins2;
        bool        pinDiff;   // true if the two pin sets are not identical
    };
    std::vector<SharedNet> shared;
    std::vector<std::string> only1, only2;

    for (const auto& [net, pv1] : nm1) {
        if (nm2.count(net)) {
            const auto& pv2 = nm2.at(net);
            // pin_diff: compare pad+pin pairs as sorted sets
            auto sig = [](const PinVec& v) {
                std::vector<std::pair<std::string,std::string>> s;
                for (const auto& pe : v) s.emplace_back(pe.pad, pe.pin);
                std::sort(s.begin(), s.end());
                return s;
            };
            bool diff = (sig(pv1) != sig(pv2));
            shared.push_back({ net, pv1, pv2, diff });
        } else {
            only1.push_back(net);
        }
    }
    for (const auto& [net, _] : nm2)
        if (!nm1.count(net)) only2.push_back(net);

    // shared is already net-name ordered (std::map iteration)
    std::sort(only1.begin(), only1.end());
    std::sort(only2.begin(), only2.end());

    // Helper: render a PinVec as "pad/pin, pad/pin, ..."
    auto pinList = [](const PinVec& v) -> std::string {
        std::string s;
        for (size_t i = 0; i < v.size(); ++i) {
            if (i) s += ", ";
            s += v[i].pad;
            if (!v[i].pin.empty() && v[i].pin != v[i].pad)
                s += "/" + v[i].pin;
        }
        return s;
    };

    if (jsonMode) {
        std::cout << "{\n"
                  << "  \"ref1\":" << jsonStr(ref1) << ",\n"
                  << "  \"ref2\":" << jsonStr(ref2) << ",\n"
                  << "  \"shared\": [\n";
        for (size_t i = 0; i < shared.size(); ++i) {
            const auto& s = shared[i];
            std::cout << "    {\"net\":" << jsonStr(s.net) << ","
                      << "\"pin_diff\":" << (s.pinDiff ? "true" : "false") << ","
                      << "\"" << ref1 << "\":[";
            for (size_t j = 0; j < s.pins1.size(); ++j) {
                std::cout << "{\"pad\":" << jsonStr(s.pins1[j].pad)
                          << ",\"pin\":" << jsonStr(s.pins1[j].pin) << "}";
                if (j + 1 < s.pins1.size()) std::cout << ",";
            }
            std::cout << "],\"" << ref2 << "\":[";
            for (size_t j = 0; j < s.pins2.size(); ++j) {
                std::cout << "{\"pad\":" << jsonStr(s.pins2[j].pad)
                          << ",\"pin\":" << jsonStr(s.pins2[j].pin) << "}";
                if (j + 1 < s.pins2.size()) std::cout << ",";
            }
            std::cout << "]}";
            if (i + 1 < shared.size()) std::cout << ",";
            std::cout << "\n";
        }
        std::cout << "  ],\n  \"only_" << ref1 << "\": [";
        for (size_t i = 0; i < only1.size(); ++i) {
            std::cout << jsonStr(only1[i]);
            if (i + 1 < only1.size()) std::cout << ",";
        }
        std::cout << "],\n  \"only_" << ref2 << "\": [";
        for (size_t i = 0; i < only2.size(); ++i) {
            std::cout << jsonStr(only2[i]);
            if (i + 1 < only2.size()) std::cout << ",";
        }
        std::cout << "]\n}\n";
    } else {
        std::cout << "compare: " << ref1 << " vs " << ref2 << "\n\n"
                  << "shared nets (" << shared.size() << "):\n";
        for (const auto& s : shared) {
            std::cout << "  " << s.net << "\n"
                      << "    " << ref1 << ": " << pinList(s.pins1) << "\n"
                      << "    " << ref2 << ": " << pinList(s.pins2) << "\n";
        }

        std::cout << "\nonly " << ref1 << " (" << only1.size() << "):\n";
        for (const auto& n : only1) std::cout << "  " << n << "\n";
        if (only1.empty()) std::cout << "  (none)\n";

        std::cout << "\nonly " << ref2 << " (" << only2.size() << "):\n";
        for (const auto& n : only2) std::cout << "  " << n << "\n";
        if (only2.empty()) std::cout << "  (none)\n";
    }
    return true;
}

// ---------------------------------------------------------------------------
// MCU map loading helper — used by cmd_mcu and cmd_signal.
// Emits an appropriate error and returns false on failure.
// ---------------------------------------------------------------------------

static bool loadMcuMapForCmd(const std::string& mcuMapFile, McuMap& out,
                              bool jsonMode) {
    if (mcuMapFile.empty()) {
        if (jsonMode) return jsonError("mcu.map not configured");
        std::cerr << "error: mcu.map not configured\n";
        return false;
    }
    try {
        out = loadMcuMap(mcuMapFile);
    } catch (const std::exception& e) {
        if (jsonMode) return jsonError(e.what());
        std::cerr << "error: " << e.what() << "\n";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// cmd_mcu
// ---------------------------------------------------------------------------

bool cmd_mcu(const std::string& mcuMapFile, const std::string& ref,
             bool jsonMode) {
    McuMap mcuMap;
    if (!loadMcuMapForCmd(mcuMapFile, mcuMap, jsonMode)) return false;

    // ---- list all MCU refs ------------------------------------------------
    if (ref.empty()) {
        if (jsonMode) {
            std::cout << "[\n";
            size_t i = 0;
            for (const auto& [r, _] : mcuMap) {
                std::cout << "  " << jsonStr(r);
                if (++i < mcuMap.size()) std::cout << ",";
                std::cout << "\n";
            }
            std::cout << "]\n";
        } else {
            for (const auto& [r, _] : mcuMap)
                std::cout << r << "\n";
            if (mcuMap.empty()) std::cout << "(no MCUs defined)\n";
        }
        return true;
    }

    // ---- show signals for one MCU -----------------------------------------
    auto it = mcuMap.find(ref);
    if (it == mcuMap.end()) {
        // Case-insensitive fallback
        for (const auto& [r, def] : mcuMap) {
            if (equalsCI(r, ref)) { it = mcuMap.find(r); break; }
        }
    }
    if (it == mcuMap.end()) {
        if (jsonMode) return jsonError("MCU ref not found: " + ref);
        std::cerr << "error: MCU ref not found: " << ref << "\n";
        return false;
    }

    const McuDef& def = it->second;

    if (jsonMode) {
        std::cout << "{\n"
                  << "  \"ref\":" << jsonStr(def.ref) << ",\n"
                  << "  \"pins\": [\n";
        for (size_t i = 0; i < def.pins.size(); ++i) {
            const auto& p = def.pins[i];
            std::cout << "    {"
                      << "\"name\":"        << jsonStr(p.name)        << ","
                      << "\"pad\":"         << jsonStr(p.pad)         << ","
                      << "\"silicon_pin\":" << jsonStr(p.silicon_pin) << ","
                      << "\"net\":"         << jsonStr(p.net)
                      << "}";
            if (i + 1 < def.pins.size()) std::cout << ",";
            std::cout << "\n";
        }
        std::cout << "  ]\n}\n";
    } else {
        std::cout << "mcu     : " << def.ref      << "\n"
                  << "signals : " << def.pins.size() << "\n";
        for (const auto& p : def.pins) {
            std::cout << "  " << p.name
                      << "   pad " << p.pad
                      << "   (" << p.silicon_pin << ")"
                      << "   →  " << p.net << "\n";
        }
        if (def.pins.empty()) std::cout << "  (no signals defined)\n";
    }
    return true;
}

// ---------------------------------------------------------------------------
// cmd_signal
// ---------------------------------------------------------------------------

// Collect all (mcu_ref, pin) pairs sorted by signal name then MCU ref.
static std::vector<std::pair<std::string, const McuPin*>>
allSignalsSorted(const McuMap& mcuMap) {
    std::vector<std::pair<std::string, const McuPin*>> v;
    for (const auto& [ref, def] : mcuMap)
        for (const auto& p : def.pins)
            v.emplace_back(ref, &p);
    std::stable_sort(v.begin(), v.end(),
        [](const auto& a, const auto& b) {
            return a.second->name < b.second->name;
        });
    return v;
}

// Resolve a query against all pins; returns {mcu_ref, pin*} or {""‚ nullptr}.
// Priority: exact name → exact silicon_pin → exact net → exact pad →
//           CI name → CI silicon_pin → CI net → CI pad.
static std::pair<std::string, const McuPin*>
resolveSignal(const McuMap& mcuMap, const std::string& query) {
    // Each pass: iterate MCUs in ref order (std::map), pins in definition order.
    auto tryExact = [&](auto field) -> std::pair<std::string, const McuPin*> {
        for (const auto& [ref, def] : mcuMap)
            for (const auto& p : def.pins)
                if (field(p) == query) return {ref, &p};
        return {"", nullptr};
    };
    auto tryCI = [&](auto field) -> std::pair<std::string, const McuPin*> {
        for (const auto& [ref, def] : mcuMap)
            for (const auto& p : def.pins)
                if (equalsCI(field(p), query)) return {ref, &p};
        return {"", nullptr};
    };

    auto name_f  = [](const McuPin& p) -> const std::string& { return p.name;        };
    auto spin_f  = [](const McuPin& p) -> const std::string& { return p.silicon_pin; };
    auto net_f   = [](const McuPin& p) -> const std::string& { return p.net;         };
    auto pad_f   = [](const McuPin& p) -> const std::string& { return p.pad;         };

    if (auto r = tryExact(name_f);  r.second) return r;
    if (auto r = tryExact(spin_f);  r.second) return r;
    if (auto r = tryExact(net_f);   r.second) return r;
    if (auto r = tryExact(pad_f);   r.second) return r;
    if (auto r = tryCI(name_f);     r.second) return r;
    if (auto r = tryCI(spin_f);     r.second) return r;
    if (auto r = tryCI(net_f);      r.second) return r;
    if (auto r = tryCI(pad_f);      r.second) return r;

    return {"", nullptr};
}

bool cmd_signal(const std::string& mcuMapFile, const std::string& query,
                bool jsonMode) {
    McuMap mcuMap;
    if (!loadMcuMapForCmd(mcuMapFile, mcuMap, jsonMode)) return false;

    // ---- list all signal names --------------------------------------------
    if (query.empty()) {
        auto all = allSignalsSorted(mcuMap);
        if (jsonMode) {
            std::cout << "[\n";
            for (size_t i = 0; i < all.size(); ++i) {
                std::cout << "  " << jsonStr(all[i].second->name);
                if (i + 1 < all.size()) std::cout << ",";
                std::cout << "\n";
            }
            std::cout << "]\n";
        } else {
            for (const auto& [ref, pin] : all)
                std::cout << pin->name << "\n";
            if (all.empty()) std::cout << "(no signals defined)\n";
        }
        return true;
    }

    // ---- resolve one signal -----------------------------------------------
    auto [foundRef, foundPin] = resolveSignal(mcuMap, query);
    if (!foundPin) {
        if (jsonMode) return jsonError("signal not found: " + query);
        std::cerr << "error: signal not found: " << query << "\n";
        return false;
    }

    if (jsonMode) {
        std::cout << "{"
                  << "\"mcu\":"         << jsonStr(foundRef)           << ","
                  << "\"name\":"        << jsonStr(foundPin->name)        << ","
                  << "\"pad\":"         << jsonStr(foundPin->pad)         << ","
                  << "\"silicon_pin\":" << jsonStr(foundPin->silicon_pin) << ","
                  << "\"net\":"         << jsonStr(foundPin->net)
                  << "}\n";
    } else {
        std::cout << "mcu         : " << foundRef             << "\n"
                  << "name        : " << foundPin->name        << "\n"
                  << "pad         : " << foundPin->pad         << "\n"
                  << "silicon_pin : " << foundPin->silicon_pin << "\n"
                  << "net         : " << foundPin->net         << "\n";
    }
    return true;
}

// ---------------------------------------------------------------------------
// cmd_walk
// ---------------------------------------------------------------------------

bool cmd_walk(const Model& m, const std::string& startRef,
              int maxDepth, const std::string& viaNet,
              bool includePowerNets, bool jsonMode) {
    if (m.parts.find(startRef) == m.parts.end()) {
        if (jsonMode) return jsonError("part not found: " + startRef);
        std::cerr << "error: part not found: " << startRef << "\n";
        return false;
    }
    if (!viaNet.empty() && m.nets.find(viaNet) == m.nets.end()) {
        if (jsonMode) return jsonError("net not found: " + viaNet);
        std::cerr << "error: net not found: " << viaNet << "\n";
        return false;
    }

    struct WalkNode {
        std::string ref;
        int         distance;
        std::string via_net;   // net through which this node was first reached
        std::string from_ref;  // parent ref (empty for start)
    };

    std::vector<WalkNode> result;
    std::set<std::string> visited;
    std::deque<WalkNode>  queue;

    visited.insert(startRef);
    queue.push_back({startRef, 0, "", ""});

    while (!queue.empty()) {
        WalkNode node = queue.front();
        queue.pop_front();
        result.push_back(node);

        if (maxDepth >= 0 && node.distance >= maxDepth) continue;

        auto pit = m.partPins.find(node.ref);
        if (pit == m.partPins.end()) continue;

        // Collect neighbours sorted by ref for determinism within each expansion.
        // Maps neighbour ref → (via_net) for its first discovered net.
        std::map<std::string, std::string> neighbours;
        for (const auto& pe : pit->second) {
            if (!viaNet.empty() && pe.net != viaNet) continue;
            // When --via names a net explicitly the power filter doesn't apply.
            if (viaNet.empty() && !includePowerNets && isRailNet(pe.net)) continue;
            auto nit = m.nets.find(pe.net);
            if (nit == m.nets.end()) continue;
            for (const auto& mb : nit->second.members) {
                if (mb.part == node.ref) continue;
                if (visited.count(mb.part)) continue;
                neighbours.emplace(mb.part, pe.net); // emplace keeps first insertion
            }
        }

        for (const auto& [nref, net] : neighbours) {
            if (visited.insert(nref).second)
                queue.push_back({nref, node.distance + 1, net, node.ref});
        }
    }

    // Sort by (distance, ref) for fully deterministic output.
    std::stable_sort(result.begin(), result.end(),
        [](const WalkNode& a, const WalkNode& b) {
            return a.distance != b.distance ? a.distance < b.distance
                                            : a.ref < b.ref;
        });

    int maxReached = result.empty() ? 0 : result.back().distance;

    if (jsonMode) {
        std::cout << "{\n"
                  << "  \"start\":" << jsonStr(startRef) << ",\n"
                  << "  \"depth\":" << maxReached << ",\n";
        if (!viaNet.empty())
            std::cout << "  \"via_net\":" << jsonStr(viaNet) << ",\n";
        std::cout << "  \"visited\": [\n";
        for (size_t i = 0; i < result.size(); ++i) {
            const auto& n = result[i];
            std::cout << "    {"
                      << "\"ref\":"      << jsonStr(n.ref)      << ","
                      << "\"distance\":" << n.distance;
            if (!n.via_net.empty())
                std::cout << ",\"via_net\":" << jsonStr(n.via_net)
                          << ",\"from\":"    << jsonStr(n.from_ref);
            std::cout << "}";
            if (i + 1 < result.size()) std::cout << ",";
            std::cout << "\n";
        }
        std::cout << "  ]\n}\n";
    } else {
        std::cout << "walk: " << startRef << "\n"
                  << "depth: " << maxReached << "\n";
        if (!viaNet.empty())
            std::cout << "via: " << viaNet << "\n";

        int curLevel = -1;
        for (const auto& n : result) {
            if (n.distance != curLevel) {
                curLevel = n.distance;
                std::cout << "\n" << curLevel << ":\n";
            }
            std::cout << "  " << n.ref << "\n";
        }
    }
    return true;
}
