// Generators for the self-created sample files in data/ and for the large
// files used to test parser size limits (docs/file_limits.md).

#include "sample_data.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace uew {
namespace {

// Heat-sink geometry in millimetres: a base plate with five fins, extruded along y.
constexpr double kWidth = 60.0, kDepth = 40.0, kBaseHeight = 6.0, kTopZ = 36.0;
constexpr double kFinThickness = 3.0, kFinPitch = 14.25;
constexpr int kFins = 5;

using P2 = std::array<double, 2>;  // (x, z)
using P3 = std::array<double, 3>;

std::vector<P2> combOutline() {
    std::vector<P2> outline{{0, 0}, {kWidth, 0}};
    for (int i = kFins - 1; i >= 0; --i) {
        const double x0 = i * kFinPitch, x1 = x0 + kFinThickness;
        outline.push_back({x1, kTopZ});
        outline.push_back({x0, kTopZ});
        if (i > 0) {
            outline.push_back({x0, kBaseHeight});
            outline.push_back({(i - 1) * kFinPitch + kFinThickness, kBaseHeight});
        }
    }
    return outline;
}

class StlWriter {
public:
    explicit StlWriter(const std::string& path, const char* name) : out_(path), name_(name) {
        if (!out_) throw std::runtime_error("Cannot write " + path);
        out_ << "solid " << name_ << "\n";
    }
    ~StlWriter() { out_ << "endsolid " << name_ << "\n"; }

    void triangle(const P3& a, const P3& b, const P3& c) {
        const P3 u{b[0] - a[0], b[1] - a[1], b[2] - a[2]}, v{c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        P3 n{u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
        const double len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        for (double& k : n) k = len > 0 ? k / len : 0.0;
        char line[200];
        std::snprintf(line, sizeof line, "  facet normal %g %g %g\n    outer loop\n", n[0], n[1], n[2]);
        out_ << line;
        for (const P3* p : {&a, &b, &c}) {
            std::snprintf(line, sizeof line, "      vertex %.6e %.6e %.6e\n", (*p)[0], (*p)[1], (*p)[2]);
            out_ << line;
        }
        out_ << "    endloop\n  endfacet\n";
    }

    // Quad a-b-c-d (counter-clockwise seen from outside) as two triangles.
    void quad(const P3& a, const P3& b, const P3& c, const P3& d) {
        triangle(a, b, c);
        triangle(a, c, d);
    }

private:
    std::ofstream out_;
    const char* name_;
};

void writeRectCaps(StlWriter& stl, double x0, double x1, double z0, double z1) {
    stl.quad({x0, 0, z0}, {x1, 0, z0}, {x1, 0, z1}, {x0, 0, z1});              // y = 0 face (-y normal)
    stl.quad({x0, kDepth, z0}, {x0, kDepth, z1}, {x1, kDepth, z1}, {x1, kDepth, z0});  // y = depth (+y)
}

// Fin-equation-shaped synthetic field: base at ~82 C, decaying along each fin
// like cosh(m (Lf - s)) / cosh(m Lf) toward 25 C ambient. NOT an ANSYS result.
double syntheticTemperature(double xMm, double yMm, double zMm) {
    const double ambient = 25.0;
    const double base = 82.0 - 6.0 * std::abs(xMm - kWidth / 2) / (kWidth / 2) - 2.0 * yMm / kDepth;
    if (zMm <= kBaseHeight) return base + 3.0 * (1.0 - zMm / kBaseHeight);
    const double m = 60.0, finLength = (kTopZ - kBaseHeight) / 1000.0, s = (zMm - kBaseHeight) / 1000.0;
    return ambient + (base - ambient) * std::cosh(m * (finLength - s)) / std::cosh(m * finLength);
}

}  // namespace

void writeHeatSinkStl(const std::string& path) {
    StlWriter stl(path, "heat_sink_mm");
    const auto outline = combOutline();
    for (std::size_t i = 0; i < outline.size(); ++i) {  // side walls
        const P2 p = outline[i], q = outline[(i + 1) % outline.size()];
        stl.quad({p[0], 0, p[1]}, {p[0], kDepth, p[1]}, {q[0], kDepth, q[1]}, {q[0], 0, q[1]});
    }
    writeRectCaps(stl, 0, kWidth, 0, kBaseHeight);  // end caps from rectangles
    for (int i = 0; i < kFins; ++i)
        writeRectCaps(stl, i * kFinPitch, i * kFinPitch + kFinThickness, kBaseHeight, kTopZ);
}

void writeHeatSinkCsv(const std::string& path) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("Cannot write " + path);
    out << "node_id,x_m,y_m,z_m,temperature_C\n";
    long id = 0;
    char line[160];
    auto node = [&](double x, double y, double z) {
        std::snprintf(line, sizeof line, "%ld,%.6f,%.6f,%.6f,%.4f\n", ++id, x / 1000, y / 1000, z / 1000,
                      syntheticTemperature(x, y, z));
        out << line;
    };
    for (int iy = 0; iy <= 4; ++iy) {
        const double y = iy * kDepth / 4;
        for (int iz = 0; iz <= 2; ++iz)
            for (int ix = 0; ix <= 20; ++ix) node(ix * kWidth / 20, y, iz * kBaseHeight / 2);
        for (int f = 0; f < kFins; ++f)
            for (int iz = 1; iz <= 10; ++iz)
                for (int ix = 0; ix <= 2; ++ix) node(f * kFinPitch + ix * kFinThickness / 2, y, kBaseHeight + iz * 3.0);
    }
}

void writeLargeStl(const std::string& path, std::size_t triangles) {
    StlWriter stl(path, "size_test");
    for (std::size_t i = 0; i < triangles; ++i) {
        const double x = static_cast<double>(i % 1000), y = static_cast<double>(i / 1000);
        stl.triangle({x, y, 0}, {x + 1, y, 0}, {x, y + 1, 0.5});
    }
}

void writeLargeCsv(const std::string& path, std::size_t nodes) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("Cannot write " + path);
    out << "node_id,x_m,y_m,z_m,temperature_C\n";
    char line[160];
    for (std::size_t i = 0; i < nodes; ++i) {
        std::snprintf(line, sizeof line, "%zu,%.6f,%.6f,%.6f,%.4f\n", i + 1, (i % 1000) * 1e-3,
                      (i / 1000 % 1000) * 1e-3, (i / 1000000) * 1e-3, 20.0 + (i % 997) * 0.1);
        out << line;
    }
}

}  // namespace uew
