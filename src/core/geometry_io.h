#pragma once
// Parsers for ASCII STL geometry and ANSYS-style nodal temperature CSV
// (node_id,x_m,y_m,z_m,temperature_C). Both reject malformed or non-finite
// data with a message naming the line and the problem.

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace uew {

constexpr std::size_t kMaxInputBytes = 128u * 1024u * 1024u;  // tested limit, see docs/file_limits.md
constexpr std::size_t kMaxTriangles = 2'000'000;
constexpr std::size_t kMaxNodes = 2'000'000;

class ParseError : public std::runtime_error {
public:
    ParseError(std::size_t line, const std::string& message)
        : std::runtime_error(line ? "Line " + std::to_string(line) + ": " + message : message), line_(line) {}
    std::size_t line() const { return line_; }

private:
    std::size_t line_;
};

using Vec3 = std::array<double, 3>;

struct TriangleMesh {
    std::string solid_name;
    std::vector<float> vertices;  // x,y,z for 3 vertices per triangle, in file units
    std::size_t triangles = 0;
    std::size_t degenerate_triangles = 0;  // zero area; kept but reported
    Vec3 min{}, max{};
    double surface_area = 0.0;  // file units squared
};

struct NodeRecord {
    std::int64_t id = 0;
    double x_m = 0, y_m = 0, z_m = 0, temperature_C = 0;
};

struct NodalResults {
    std::vector<NodeRecord> nodes;
    double min_C = 0, max_C = 0, mean_C = 0;
    Vec3 min_m{}, max_m{};
};

TriangleMesh parseAsciiStl(std::string_view text);
NodalResults parseNodalCsv(std::string_view text);

}  // namespace uew
