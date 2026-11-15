#pragma once
// Minimal JSON value + parser/serializer (presets, state, manifests). Not for the audio thread.
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace gnarl {

class Json {
public:
    enum Type { Null, Bool, Number, String, Array, Object };
    Json() = default;
    Json(std::nullptr_t) {}
    Json(bool b) : t(Bool), b(b) {}
    Json(double d) : t(Number), d(d) {}
    Json(float d) : t(Number), d(d) {}
    Json(int i) : t(Number), d(i) {}
    Json(long long i) : t(Number), d((double) i) {}
    Json(const char* s) : t(String), s(s) {}
    Json(std::string s) : t(String), s(std::move(s)) {}
    static Json array() { Json j; j.t = Array; return j; }
    static Json object() { Json j; j.t = Object; return j; }

    Type type() const { return t; }
    bool isNull() const { return t == Null; }
    bool isObject() const { return t == Object; }
    bool isArray() const { return t == Array; }
    bool isNumber() const { return t == Number; }
    bool isString() const { return t == String; }
    double num(double def = 0) const { return t == Number ? d : (t == Bool ? (b ? 1 : 0) : def); }
    bool boolean(bool def = false) const { return t == Bool ? b : (t == Number ? d != 0 : def); }
    const std::string& str() const { static const std::string e; return t == String ? s : e; }
    std::string str(const std::string& def) const { return t == String ? s : def; }

    // object access
    Json& operator[](const std::string& k) { if (t != Object) { t = Object; } return obj[k]; }
    const Json& operator[](const std::string& k) const { static const Json nul; auto it = obj.find(k); return it == obj.end() ? nul : it->second; }
    bool has(const std::string& k) const { return t == Object && obj.count(k) > 0; }
    const std::map<std::string, Json>& items() const { return obj; }
    // array access
    void push(Json v) { if (t != Array) t = Array; arr.push_back(std::move(v)); }
    size_t size() const { return t == Array ? arr.size() : (t == Object ? obj.size() : 0); }
    const Json& at(size_t i) const { static const Json nul; return (t == Array && i < arr.size()) ? arr[i] : nul; }
    const std::vector<Json>& elements() const { return arr; }

    std::string dump(int indent = 2) const;
    static Json parse(const std::string& text, std::string* error = nullptr);

private:
    void dumpTo(std::string& out, int indent, int level) const;
    Type t = Null;
    bool b = false;
    double d = 0;
    std::string s;
    std::vector<Json> arr;
    std::map<std::string, Json> obj;
};

} // namespace gnarl
