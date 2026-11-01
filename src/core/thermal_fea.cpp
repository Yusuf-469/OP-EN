#include "thermal_fea.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace uew {
namespace {

// Symmetric tridiagonal system: diag[i] = K(i,i), off[i] = K(i,i+1) = K(i+1,i).
struct TridiagonalSystem {
    std::vector<double> diag;
    std::vector<double> off;
    std::vector<double> rhs;
};

void requireFinite(double value, const char* name) {
    if (!std::isfinite(value))
        throw std::invalid_argument(std::string(name) + " must be a finite number.");
}

void requirePositive(double value, const char* name) {
    requireFinite(value, name);
    if (value <= 0.0)
        throw std::invalid_argument(std::string(name) + " must be greater than zero.");
}

// Adds every element's stiffness matrix and load vector to the global system.
//   k_e = (k A / h) [ 1 -1; -1 1 ],   f_e = (q A h / 2) [ 1; 1 ]
TridiagonalSystem assemble(const RodProblem& p) {
    const int nodes = p.elements + 1;
    const double h = p.length_m / p.elements;
    const double ke = p.conductivity_W_per_mK * p.area_m2 / h;
    const double fe = p.heat_generation_W_per_m3 * p.area_m2 * h / 2.0;

    TridiagonalSystem sys{std::vector<double>(nodes, 0.0), std::vector<double>(nodes - 1, 0.0),
                          std::vector<double>(nodes, 0.0)};
    for (int e = 0; e < p.elements; ++e) {
        sys.diag[e] += ke;
        sys.diag[e + 1] += ke;
        sys.off[e] -= ke;
        sys.rhs[e] += fe;
        sys.rhs[e + 1] += fe;
    }
    return sys;
}

// Imposes T(node) = value by moving the known column to the right-hand side,
// then replacing the row with the identity. Keeps the matrix symmetric.
void applyPrescribedTemperature(TridiagonalSystem& sys, int node, double value) {
    const int last = static_cast<int>(sys.diag.size()) - 1;
    if (node > 0) {
        sys.rhs[node - 1] -= sys.off[node - 1] * value;
        sys.off[node - 1] = 0.0;
    }
    if (node < last) {
        sys.rhs[node + 1] -= sys.off[node] * value;
        sys.off[node] = 0.0;
    }
    sys.diag[node] = 1.0;
    sys.rhs[node] = value;
}

// Thomas algorithm (Gaussian elimination specialised to tridiagonal matrices), O(n).
std::vector<double> solveTridiagonal(TridiagonalSystem sys) {
    const std::size_t n = sys.diag.size();
    for (std::size_t i = 1; i < n; ++i) {
        if (std::abs(sys.diag[i - 1]) < 1e-300)
            throw std::runtime_error("Thermal stiffness matrix is singular; check k, A and L.");
        const double m = sys.off[i - 1] / sys.diag[i - 1];
        sys.diag[i] -= m * sys.off[i - 1];
        sys.rhs[i] -= m * sys.rhs[i - 1];
    }
    std::vector<double> T(n);
    T[n - 1] = sys.rhs[n - 1] / sys.diag[n - 1];
    for (std::size_t i = n - 1; i-- > 0;)
        T[i] = (sys.rhs[i] - sys.off[i] * T[i + 1]) / sys.diag[i];
    return T;
}

}  // namespace

void validate(const RodProblem& p) {
    requirePositive(p.length_m, "Rod length L (m)");
    requirePositive(p.area_m2, "Cross-section area A (m^2)");
    requirePositive(p.conductivity_W_per_mK, "Thermal conductivity k (W/(m K))");
    requireFinite(p.heat_generation_W_per_m3, "Heat generation q (W/m^3)");
    requireFinite(p.temp_left_C, "Left temperature T(0) (C)");
    requireFinite(p.temp_right_C, "Right temperature T(L) (C)");
    if (p.temp_left_C < kAbsoluteZeroC || p.temp_right_C < kAbsoluteZeroC)
        throw std::invalid_argument("Boundary temperatures cannot be below absolute zero (-273.15 C).");
    if (p.elements < 1 || p.elements > kMaxElements)
        throw std::invalid_argument("Element count N must be between 1 and " +
                                    std::to_string(kMaxElements) + ".");
}

RodSolution solveRod(const RodProblem& p) {
    validate(p);
    const TridiagonalSystem unconstrained = assemble(p);
    TridiagonalSystem sys = unconstrained;
    applyPrescribedTemperature(sys, 0, p.temp_left_C);
    applyPrescribedTemperature(sys, p.elements, p.temp_right_C);

    RodSolution out;
    out.temperature_C = solveTridiagonal(std::move(sys));
    const double h = p.length_m / p.elements;
    out.x_m.resize(out.temperature_C.size());
    for (std::size_t i = 0; i < out.x_m.size(); ++i) out.x_m[i] = static_cast<double>(i) * h;

    // Reaction R = K T - F at a prescribed node is the heat the boundary must
    // supply; its negative is the heat leaving the rod there.
    const auto& T = out.temperature_C;
    const auto& K = unconstrained;
    const int n = p.elements;
    out.heat_out_left_W = -(K.diag[0] * T[0] + K.off[0] * T[1] - K.rhs[0]);
    out.heat_out_right_W = -(K.diag[n] * T[n] + K.off[n - 1] * T[n - 1] - K.rhs[n]);
    return out;
}

double exactTemperature(const RodProblem& p, double x) {
    const double L = p.length_m;
    return p.temp_left_C + (p.temp_right_C - p.temp_left_C) * x / L +
           p.heat_generation_W_per_m3 / (2.0 * p.conductivity_W_per_mK) * x * (L - x);
}

double interpolate(const RodSolution& s, double x) {
    const auto& xs = s.x_m;
    if (x <= xs.front()) return s.temperature_C.front();
    if (x >= xs.back()) return s.temperature_C.back();
    const auto upper = std::upper_bound(xs.begin(), xs.end(), x);
    const std::size_t j = static_cast<std::size_t>(upper - xs.begin());
    const double t = (x - xs[j - 1]) / (xs[j] - xs[j - 1]);
    return (1.0 - t) * s.temperature_C[j - 1] + t * s.temperature_C[j];
}

ErrorMeasures measureError(const RodProblem& p, const RodSolution& s) {
    ErrorMeasures err;
    for (std::size_t i = 0; i < s.x_m.size(); ++i)
        err.max_nodal_error_C = std::max(err.max_nodal_error_C,
                                         std::abs(s.temperature_C[i] - exactTemperature(p, s.x_m[i])));
    for (std::size_t e = 0; e + 1 < s.x_m.size(); ++e) {
        const double xm = 0.5 * (s.x_m[e] + s.x_m[e + 1]);
        const double feMid = 0.5 * (s.temperature_C[e] + s.temperature_C[e + 1]);
        err.max_midpoint_error_C = std::max(err.max_midpoint_error_C,
                                            std::abs(feMid - exactTemperature(p, xm)));
    }
    return err;
}

}  // namespace uew
