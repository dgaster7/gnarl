#include "Json.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace gnarl {

static void escape(std::string& out, const std::string& s) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) { char buf[8]; std::snprintf(buf, sizeof buf, "\\u%04x", c); out += buf; }
                else out += (char) c;
        }
    }
    out += '"';
}

void Json::dumpTo(std::string& out, int indent, int level) const {
    auto nl = [&](int lv) { if (indent > 0) { out += '\n'; out.append((size_t) (lv * indent), ' '); } };
    switch (t) {
        case Null: out += "null"; break;
        case Bool: out += b ? "true" : "false"; break;
        case Number: {
            if (!std::isfinite(d)) { out += "0"; break; }
            char buf[40];
            if (d == std::floor(d) && std::fabs(d) < 1e15) std::snprintf(buf, sizeof buf, "%lld", (long long) d);
            else std::snprintf(buf, sizeof buf, "%.9g", d);
            out += buf; break;
        }
        case String: escape(out, s); break;
        case Array: {
            out += '[';
            bool compact = true;
            for (auto& e : arr) if (e.t == Array || e.t == Object) { compact = false; break; }
            for (size_t i = 0; i < arr.size(); ++i) {
                if (i) out += compact ? ", " : ",";
                if (!compact) nl(level + 1);
                arr[i].dumpTo(out, indent, level + 1);
            }
            if (!compact && !arr.empty()) nl(level);
            out += ']'; break;
        }
        case Object: {
            out += '{';
            size_t i = 0;
            for (auto& [k, v] : obj) {
                if (i++) out += ',';
                nl(level + 1);
                escape(out, k);
                out += indent > 0 ? ": " : ":";
                v.dumpTo(out, indent, level + 1);
            }
            if (!obj.empty()) nl(level);
            out += '}'; break;
        }
    }
}

std::string Json::dump(int indent) const { std::string o; dumpTo(o, indent, 0); return o; }

namespace {
struct Parser {
    const char* p; const char* end; std::string err;
    void ws() { while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p; }
    bool lit(const char* s) { size_t n = std::strlen(s); if ((size_t) (end - p) >= n && std::memcmp(p, s, n) == 0) { p += n; return true; } return false; }
    Json value(int depth) {
        if (depth > 200) { err = "too deep"; return {}; }
        ws();
        if (p >= end) { err = "unexpected end"; return {}; }
        char c = *p;
        if (c == '{') {
            ++p; Json o = Json::object(); ws();
            if (p < end && *p == '}') { ++p; return o; }
            while (p < end) {
                ws(); if (p >= end || *p != '"') { err = "expected key"; return {}; }
                std::string k = string(); ws();
                if (p >= end || *p != ':') { err = "expected :"; return {}; }
                ++p; o[k] = value(depth + 1); if (!err.empty()) return {};
                ws(); if (p < end && *p == ',') { ++p; continue; }
                if (p < end && *p == '}') { ++p; return o; }
                err = "expected , or }"; return {};
            }
            err = "unterminated object"; return {};
        }
        if (c == '[') {
            ++p; Json a = Json::array(); ws();
            if (p < end && *p == ']') { ++p; return a; }
            while (p < end) {
                a.push(value(depth + 1)); if (!err.empty()) return {};
                ws(); if (p < end && *p == ',') { ++p; continue; }
                if (p < end && *p == ']') { ++p; return a; }
                err = "expected , or ]"; return {};
            }
            err = "unterminated array"; return {};
        }
        if (c == '"') return Json(string());
        if (lit("true")) return Json(true);
        if (lit("false")) return Json(false);
        if (lit("null")) return Json();
        char* e = nullptr;
        double d = std::strtod(p, &e);
        if (e == p) { err = "bad token"; return {}; }
        p = e; return Json(d);
    }
    std::string string() {
        std::string out; ++p;
        while (p < end && *p != '"') {
            if (*p == '\\' && p + 1 < end) {
                ++p;
                switch (*p) {
                    case 'n': out += '\n'; break; case 't': out += '\t'; break; case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break; case 'f': out += '\f'; break;
                    case 'u': {
                        if (end - p < 5) { err = "bad escape"; return out; }
                        unsigned cp = (unsigned) std::strtoul(std::string(p + 1, 4).c_str(), nullptr, 16); p += 4;
                        if (cp < 0x80) out += (char) cp;
                        else if (cp < 0x800) { out += (char) (0xC0 | (cp >> 6)); out += (char) (0x80 | (cp & 0x3F)); }
                        else { out += (char) (0xE0 | (cp >> 12)); out += (char) (0x80 | ((cp >> 6) & 0x3F)); out += (char) (0x80 | (cp & 0x3F)); }
                        break;
                    }
                    default: out += *p;
                }
                ++p;
            } else out += *p++;
        }
        if (p < end) ++p; else err = "unterminated string";
        return out;
    }
};
} // namespace

Json Json::parse(const std::string& text, std::string* error) {
    Parser ps{text.data(), text.data() + text.size(), {}};
    Json v = ps.value(0);
    if (ps.err.empty()) { ps.ws(); if (ps.p != ps.end) ps.err = "trailing characters"; }
    if (error) *error = ps.err;
    return ps.err.empty() ? v : Json();
}

} // namespace gnarl
