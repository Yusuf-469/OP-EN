#pragma once
// Minimal streaming JSON writer for API responses. The backend only *emits*
// JSON (requests use query parameters or raw file bodies), so a full JSON
// library is not needed.

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

namespace uew {

class JsonWriter {
public:
    JsonWriter& beginObject() { separate(); out_ += '{'; first_ = true; return *this; }
    JsonWriter& endObject() { out_ += '}'; first_ = false; return *this; }
    JsonWriter& beginArray() { separate(); out_ += '['; first_ = true; return *this; }
    JsonWriter& endArray() { out_ += ']'; first_ = false; return *this; }

    JsonWriter& key(std::string_view name) {
        separate();
        appendString(name);
        out_ += ':';
        first_ = true;  // the value that follows must not be preceded by a comma
        return *this;
    }

    JsonWriter& value(std::string_view text) { separate(); appendString(text); return *this; }
    JsonWriter& value(const char* text) { return value(std::string_view(text)); }
    JsonWriter& value(bool flag) { separate(); out_ += flag ? "true" : "false"; return *this; }
    JsonWriter& value(int number) { return value(static_cast<std::int64_t>(number)); }
    JsonWriter& value(std::size_t number) { return value(static_cast<std::int64_t>(number)); }
    JsonWriter& value(std::int64_t number) { separate(); out_ += std::to_string(number); return *this; }

    // Non-finite numbers are not valid JSON, so they are written as null.
    JsonWriter& value(double number, int significantDigits = 12) {
        separate();
        if (!std::isfinite(number)) { out_ += "null"; return *this; }
        char buffer[40];  // to_chars: locale-independent and much faster than snprintf for large arrays
        const auto result = std::to_chars(buffer, buffer + sizeof buffer, number, std::chars_format::general, significantDigits);
        out_.append(buffer, result.ptr);
        return *this;
    }

    // Inserts already-serialised JSON produced by another JsonWriter.
    JsonWriter& raw(std::string_view json) { separate(); out_ += json; return *this; }

    template <typename T>
    JsonWriter& field(std::string_view name, const T& v) { key(name); return value(v); }

    const std::string& str() const { return out_; }

private:
    void separate() {
        if (!first_) out_ += ',';
        first_ = false;
    }

    void appendString(std::string_view text) {
        out_ += '"';
        for (char ch : text) {
            const auto c = static_cast<unsigned char>(ch);
            if (ch == '"' || ch == '\\') { out_ += '\\'; out_ += ch; }
            else if (ch == '\n') out_ += "\\n";
            else if (ch == '\r') out_ += "\\r";
            else if (ch == '\t') out_ += "\\t";
            else if (c < 0x20) {
                char escaped[8];
                std::snprintf(escaped, sizeof escaped, "\\u%04x", c);
                out_ += escaped;
            } else out_ += ch;
        }
        out_ += '"';
    }

    std::string out_;
    bool first_ = true;
};

}  // namespace uew
