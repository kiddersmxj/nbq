#include "../inc/mcu.hpp"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Minimal recursive-descent JSON parser for the MCU map format.
//
// Expected top-level shape:
//   { "<ref>": { "pins": [ { "name":..., "pad":...,
//                            "silicon_pin":..., "net":... }, ... ] }, ... }
//
// Unknown keys at any level are skipped.
// ---------------------------------------------------------------------------

namespace {

struct Parser {
    const std::string& s;
    size_t pos = 0;

    void skipWS() {
        while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos])))
            ++pos;
    }

    // Peek at the next non-whitespace character without consuming it.
    char peekNWS() {
        skipWS();
        if (pos >= s.size())
            throw std::runtime_error("unexpected end of input");
        return s[pos];
    }

    // Consume one expected character (after skipping whitespace).
    void require(char c) {
        skipWS();
        if (pos >= s.size() || s[pos] != c) {
            std::string got = pos < s.size()
                ? std::string(1, s[pos]) : std::string("EOF");
            throw std::runtime_error(
                std::string("expected '") + c + "', got '" + got + "'");
        }
        ++pos;
    }

    // Parse a JSON string value (including the surrounding quotes).
    std::string parseString() {
        require('"');
        std::string out;
        while (pos < s.size()) {
            char c = s[pos++];
            if (c == '"') return out;
            if (c == '\\') {
                if (pos >= s.size())
                    throw std::runtime_error("truncated escape sequence");
                char e = s[pos++];
                switch (e) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case '/':  out += '/';  break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                case 'b':  out += '\b'; break;
                case 'f':  out += '\f'; break;
                case 'u': {
                    if (pos + 4 > s.size())
                        throw std::runtime_error("truncated \\u escape");
                    unsigned int cp = 0;
                    for (int i = 0; i < 4; ++i) {
                        char h = s[pos++];
                        cp <<= 4;
                        if      (h >= '0' && h <= '9') cp |= static_cast<unsigned>(h - '0');
                        else if (h >= 'a' && h <= 'f') cp |= static_cast<unsigned>(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') cp |= static_cast<unsigned>(h - 'A' + 10);
                        else throw std::runtime_error("invalid hex digit in \\u escape");
                    }
                    // Encode code point as UTF-8.
                    if (cp < 0x80) {
                        out += static_cast<char>(cp);
                    } else if (cp < 0x800) {
                        out += static_cast<char>(0xC0 | (cp >> 6));
                        out += static_cast<char>(0x80 | (cp & 0x3F));
                    } else {
                        out += static_cast<char>(0xE0 | (cp >> 12));
                        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        out += static_cast<char>(0x80 | (cp & 0x3F));
                    }
                    break;
                }
                default:
                    throw std::runtime_error(
                        std::string("unknown escape character '\\") + e + "'");
                }
            } else {
                out += c;
            }
        }
        throw std::runtime_error("unterminated string");
    }

    // Skip any JSON value: string, number, boolean, null, object, or array.
    void skipValue() {
        char c = peekNWS();
        if (c == '"') {
            parseString();
        } else if (c == '{') {
            ++pos; // consume '{'
            if (peekNWS() == '}') { ++pos; return; }
            while (true) {
                parseString();    // key
                require(':');
                skipValue();      // value
                char n = peekNWS();
                if (n == '}') { ++pos; break; }
                if (n == ',') { ++pos; continue; }
                throw std::runtime_error("expected ',' or '}' in object");
            }
        } else if (c == '[') {
            ++pos; // consume '['
            if (peekNWS() == ']') { ++pos; return; }
            while (true) {
                skipValue();
                char n = peekNWS();
                if (n == ']') { ++pos; break; }
                if (n == ',') { ++pos; continue; }
                throw std::runtime_error("expected ',' or ']' in array");
            }
        } else if (c == 't' || c == 'f' || c == 'n') {
            // true / false / null
            while (pos < s.size() && std::isalpha(static_cast<unsigned char>(s[pos])))
                ++pos;
        } else if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
            ++pos;
            while (pos < s.size()) {
                char d = s[pos];
                if (std::isdigit(static_cast<unsigned char>(d)) ||
                    d == '.' || d == 'e' || d == 'E' || d == '+' || d == '-')
                    ++pos;
                else break;
            }
        } else {
            throw std::runtime_error(
                std::string("unexpected character '") + c + "' in JSON value");
        }
    }

    // Parse one pin object: { "name":..., "pad":..., "silicon_pin":..., "net":... }
    McuPin parsePin() {
        require('{');
        McuPin pin;
        if (peekNWS() == '}') { ++pos; return pin; }
        while (true) {
            std::string key = parseString();
            require(':');
            if      (key == "name")        pin.name        = parseString();
            else if (key == "pad")         pin.pad         = parseString();
            else if (key == "silicon_pin") pin.silicon_pin = parseString();
            else if (key == "net")         pin.net         = parseString();
            else                           skipValue();
            char n = peekNWS();
            if (n == '}') { ++pos; break; }
            if (n == ',') { ++pos; continue; }
            throw std::runtime_error("expected ',' or '}' in pin object");
        }
        return pin;
    }

    // Parse a pins array: [ pin, pin, ... ]
    std::vector<McuPin> parsePins() {
        require('[');
        std::vector<McuPin> pins;
        if (peekNWS() == ']') { ++pos; return pins; }
        while (true) {
            pins.push_back(parsePin());
            char n = peekNWS();
            if (n == ']') { ++pos; break; }
            if (n == ',') { ++pos; continue; }
            throw std::runtime_error("expected ',' or ']' in pins array");
        }
        return pins;
    }

    // Parse one MCU definition: { "pins": [...], ... }
    McuDef parseMcuDef(const std::string& ref) {
        require('{');
        McuDef def;
        def.ref = ref;
        if (peekNWS() == '}') { ++pos; return def; }
        while (true) {
            std::string key = parseString();
            require(':');
            if (key == "pins") def.pins = parsePins();
            else               skipValue();
            char n = peekNWS();
            if (n == '}') { ++pos; break; }
            if (n == ',') { ++pos; continue; }
            throw std::runtime_error("expected ',' or '}' in MCU definition");
        }
        return def;
    }

    // Parse the top-level object: { "<ref>": { ... }, ... }
    McuMap parse() {
        require('{');
        McuMap result;
        if (peekNWS() == '}') { ++pos; return result; }
        while (true) {
            std::string ref = parseString();
            require(':');
            result[ref] = parseMcuDef(ref);
            char n = peekNWS();
            if (n == '}') { ++pos; break; }
            if (n == ',') { ++pos; continue; }
            throw std::runtime_error("expected ',' or '}' in top-level object");
        }
        return result;
    }
};

} // namespace

// ---------------------------------------------------------------------------

McuMap loadMcuMap(const std::string& path) {
    std::ifstream f(path);
    if (!f)
        throw std::runtime_error("cannot open MCU map file: " + path);

    std::ostringstream ss;
    ss << f.rdbuf();
    const std::string content = ss.str();

    if (content.empty())
        throw std::runtime_error("MCU map file is empty: " + path);

    try {
        Parser p{content};
        return p.parse();
    } catch (const std::exception& e) {
        throw std::runtime_error(
            "failed to parse MCU map '" + path + "': " + e.what());
    }
}

// Copyright (c) 2026, Maxamilian Kidd-May
// All rights reserved.

// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.
