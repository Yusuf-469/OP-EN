#include "geometry_io.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <unordered_set>

#include "thermal_fea.h"  // kAbsoluteZeroC

namespace uew {
namespace {

void checkSize(std::string_view text, const char* what) {
    if (text.empty())
        throw ParseError(0, std::string(what) + " file is empty (0 bytes). Choose a file that contains data.");
    if (text.size() > kMaxInputBytes)
        throw ParseError(0, std::string(what) + " file is larger than the tested limit of " +
                                std::to_string(kMaxInputBytes / (1024 * 1024)) + " MB.");
}

// Strict, locale-independent number parsing: the whole token must be a finite number.
bool parseFinite(std::string_view token, double& out) {
    if (!token.empty() && token.front() == '+') token.remove_prefix(1);
    const char* end = token.data() + token.size();
    const auto [ptr, ec] = std::from_chars(token.data(), end, out);
    return ec == std::errc() && ptr == end && std::isfinite(out);
}

bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v'; }

std::string_view trim(std::string_view s) {
    while (!s.empty() && isSpace(s.front())) s.remove_prefix(1);
    while (!s.empty() && isSpace(s.back())) s.remove_suffix(1);
    return s;
}

std::string shown(std::string_view token) {
    return token.empty() ? "end of file" : "'" + std::string(token.substr(0, 40)) + "'";
}

// Whitespace tokenizer that tracks line numbers for error messages.
class Tokenizer {
public:
    explicit Tokenizer(std::string_view text) : text_(text) {}

    std::string_view next() {
        skipSpace();
        tokenLine_ = line_;
        const std::size_t start = pos_;
        while (pos_ < text_.size() && !isSpace(text_[pos_])) ++pos_;
        return text_.substr(start, pos_ - start);
    }

    std::string_view restOfLine() {
        const std::size_t start = pos_;
        while (pos_ < text_.size() && text_[pos_] != '\n') ++pos_;
        return trim(text_.substr(start, pos_ - start));
    }

    std::size_t line() const { return tokenLine_; }

    void expect(std::string_view keyword, const char* context) {
        const std::string_view tok = next();
        if (tok != keyword)
            throw ParseError(line(), "expected '" + std::string(keyword) + "' " + context + " but found " + shown(tok) + ".");
    }

    Vec3 readVector(const char* what) {
        Vec3 v{};
        for (double& component : v) {
            const std::string_view tok = next();
            if (!parseFinite(tok, component))
                throw ParseError(line(), std::string(what) + " component " + shown(tok) +
                                             " is not a finite number (NaN/Inf and text are rejected).");
        }
        return v;
    }

private:
    void skipSpace() {
        while (pos_ < text_.size() && isSpace(text_[pos_])) {
            if (text_[pos_] == '\n') ++line_;
            ++pos_;
        }
    }

    std::string_view text_;
    std::size_t pos_ = 0;
    std::size_t line_ = 1;
    std::size_t tokenLine_ = 1;
};

double triangleArea(const Vec3& a, const Vec3& b, const Vec3& c) {
    const Vec3 u{b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const Vec3 v{c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    const Vec3 n{u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
    return 0.5 * std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
}

void addFacet(TriangleMesh& mesh, const std::array<Vec3, 3>& tri) {
    if (mesh.triangles == 0) mesh.min = mesh.max = tri[0];
    for (const Vec3& p : tri) {
        for (int k = 0; k < 3; ++k) {
            mesh.vertices.push_back(static_cast<float>(p[k]));
            mesh.min[k] = std::min(mesh.min[k], p[k]);
            mesh.max[k] = std::max(mesh.max[k], p[k]);
        }
    }
    const double area = triangleArea(tri[0], tri[1], tri[2]);
    mesh.surface_area += area;
    if (area == 0.0) ++mesh.degenerate_triangles;
    ++mesh.triangles;
}

// facet normal nx ny nz / outer loop / vertex x3 / endloop / endfacet
void parseFacet(Tokenizer& tok, TriangleMesh& mesh) {
    tok.expect("normal", "after 'facet'");
    tok.readVector("Facet normal");  // ignored: normals are recomputed from vertex order for display
    tok.expect("outer", "to start the vertex loop");
    tok.expect("loop", "after 'outer'");
    std::array<Vec3, 3> tri{};
    for (Vec3& vertex : tri) {
        tok.expect("vertex", "(each facet needs exactly three vertices)");
        vertex = tok.readVector("Vertex");
    }
    tok.expect("endloop", "after the third vertex (facets must be triangles)");
    tok.expect("endfacet", "after 'endloop'");
    addFacet(mesh, tri);
}

}  // namespace

TriangleMesh parseAsciiStl(std::string_view text) {
    checkSize(text, "STL");
    if (text.find('\0') != std::string_view::npos)
        throw ParseError(0, "This looks like a binary STL file. Only ASCII STL is supported; re-export as ASCII.");

    Tokenizer tok(text);
    if (tok.next() != "solid")
        throw ParseError(tok.line(), "an ASCII STL file must start with 'solid'.");
    TriangleMesh mesh;
    mesh.solid_name = std::string(tok.restOfLine());

    for (;;) {
        const std::string_view word = tok.next();
        if (word == "facet") {
            if (mesh.triangles >= kMaxTriangles)
                throw ParseError(tok.line(), "more than " + std::to_string(kMaxTriangles) + " triangles (tested limit).");
            parseFacet(tok, mesh);
        } else if (word == "endsolid") {
            break;
        } else {
            throw ParseError(tok.line(), "expected 'facet' or 'endsolid' but found " + shown(word) + ".");
        }
    }
    if (mesh.triangles == 0)
        throw ParseError(0, "The STL solid contains no facets, so there is no geometry to display.");
    return mesh;
}

namespace {

std::vector<std::string_view> splitFields(std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t start = 0;
    for (;;) {
        const std::size_t comma = line.find(',', start);
        fields.push_back(trim(line.substr(start, comma == std::string_view::npos ? line.npos : comma - start)));
        if (comma == std::string_view::npos) return fields;
        start = comma + 1;
    }
}

constexpr const char* kColumns[] = {"node_id", "x_m", "y_m", "z_m", "temperature_C"};

void checkHeader(std::string_view header) {
    const auto fields = splitFields(header);
    bool ok = fields.size() == 5;
    for (std::size_t i = 0; ok && i < 5; ++i) ok = fields[i] == kColumns[i];
    if (!ok)
        throw ParseError(1, "header must be exactly 'node_id,x_m,y_m,z_m,temperature_C' but was '" +
                                std::string(header.substr(0, 80)) + "'.");
}

NodeRecord parseRow(std::string_view line, std::size_t lineNo) {
    const auto fields = splitFields(line);
    if (fields.size() != 5)
        throw ParseError(lineNo, "expected 5 comma-separated values but found " + std::to_string(fields.size()) + ".");
    NodeRecord node;
    const char* end = fields[0].data() + fields[0].size();
    const auto [ptr, ec] = std::from_chars(fields[0].data(), end, node.id);
    if (ec != std::errc() || ptr != end || node.id < 1)
        throw ParseError(lineNo, "node_id " + shown(fields[0]) + " must be a positive integer.");
    double* targets[] = {&node.x_m, &node.y_m, &node.z_m, &node.temperature_C};
    for (int i = 0; i < 4; ++i) {
        if (!parseFinite(fields[i + 1], *targets[i]))
            throw ParseError(lineNo, std::string(kColumns[i + 1]) + " value " + shown(fields[i + 1]) +
                                         " is not a finite number (NaN/Inf and text are rejected).");
    }
    if (node.temperature_C < kAbsoluteZeroC)
        throw ParseError(lineNo, "temperature_C is below absolute zero (-273.15 C); check the export units.");
    return node;
}

void summarise(NodalResults& r) {
    const NodeRecord& first = r.nodes.front();
    r.min_C = r.max_C = first.temperature_C;
    r.min_m = r.max_m = {first.x_m, first.y_m, first.z_m};
    double sum = 0.0;
    for (const NodeRecord& n : r.nodes) {
        r.min_C = std::min(r.min_C, n.temperature_C);
        r.max_C = std::max(r.max_C, n.temperature_C);
        sum += n.temperature_C;
        const Vec3 p{n.x_m, n.y_m, n.z_m};
        for (int k = 0; k < 3; ++k) {
            r.min_m[k] = std::min(r.min_m[k], p[k]);
            r.max_m[k] = std::max(r.max_m[k], p[k]);
        }
    }
    r.mean_C = sum / static_cast<double>(r.nodes.size());
}

}  // namespace

NodalResults parseNodalCsv(std::string_view text) {
    checkSize(text, "CSV");
    if (text.substr(0, 3) == "\xEF\xBB\xBF") text.remove_prefix(3);  // UTF-8 BOM written by Excel

    NodalResults result;
    std::unordered_set<std::int64_t> seenIds;
    std::size_t lineNo = 0;
    bool headerSeen = false;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t eol = std::min(text.find('\n', pos), text.size());
        const std::string_view line = trim(text.substr(pos, eol - pos));
        pos = eol + 1;
        ++lineNo;
        if (line.empty()) continue;
        if (!headerSeen) { checkHeader(line); headerSeen = true; continue; }
        if (result.nodes.size() >= kMaxNodes)
            throw ParseError(lineNo, "more than " + std::to_string(kMaxNodes) + " nodes (tested limit).");
        NodeRecord node = parseRow(line, lineNo);
        if (!seenIds.insert(node.id).second)
            throw ParseError(lineNo, "duplicate node_id " + std::to_string(node.id) + ".");
        result.nodes.push_back(node);
    }
    if (!headerSeen) throw ParseError(0, "CSV file contains only blank lines; expected a header and data rows.");
    if (result.nodes.empty())
        throw ParseError(0, "CSV has a valid header but no data rows, so there are no nodes to display.");
    summarise(result);
    return result;
}

}  // namespace uew
