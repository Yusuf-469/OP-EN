#include "thermal_fea.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace uew {
namespace {

// The workspace holds a symmetric tridiagonal system:
//   diag[i] = K(i,i),  off[i] = K(i,i+1) = K(i+1,i),  rhs[i] = F(i).
using TridiagonalSystem = SolverWorkspace;

void requireFinite(double value, const char* name) {
    if (!std::isfinite(value))
        throw std::invalid_argument(std::string(name) + " must be a finite number.");
}

void requirePositive(double value, const char* name) {
    requireFinite(value, name);
    if (value <= 0.0)
        throw std::invalid_argument(std::string(name) + " must be greater than zero.");
}

double elementConductance(const RodProblem& p) { return p.conductivity_W_per_mK * p.area_m2 / (p.length_m / p.elements); }
double elementLoad(const RodProblem& p) { return p.heat_generation_W_per_m3 * p.area_m2 * (p.length_m / p.elements) / 2.0; }

// Adds every element's stiffness matrix and load vector to the global system.
//   k_e = (k A / h) [ 1 -1; -1 1 ],   f_e = (q A h / 2) [ 1; 1 ]
void assemble(const RodProblem& p, TridiagonalSystem& sys) {
    const std::size_t nodes = static_cast<std::size_t>(p.elements) + 1;
    sys.diag.assign(nodes, 0.0);  // assign() keeps existing capacity: no reallocation when reused
    sys.off.assign(nodes - 1, 0.0);
    sys.rhs.assign(nodes, 0.0);
    const double ke = elementConductance(p), fe = elementLoad(p);
    for (int e = 0; e < p.elements; ++e) {
        sys.diag[e] += ke;
        sys.diag[e + 1] += ke;
        sys.off[e] -= ke;
        sys.rhs[e] += fe;
        sys.rhs[e + 1] += fe;
    }
}

// Imposes T(node) = value by moving the known column to the right-hand side,
// then replacing the row with the identity. Keeps the matrix symmetric.
void applyPrescribedTemperature(TridiagonalSystem& sys, std::size_t node, double value) {
    const std::size_t last = sys.diag.size() - 1;
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
// Overwrites the system during forward elimination.
void solveTridiagonal(TridiagonalSystem& sys, std::vector<double>& T) {
    const std::size_t n = sys.diag.size();
    for (std::size_t i = 1; i < n; ++i) {
        if (std::abs(sys.diag[i - 1]) < 1e-300)
            throw std::runtime_error("Thermal stiffness matrix is singular; check k, A and L.");
        const double m = sys.off[i - 1] / sys.diag[i - 1];
        sys.diag[i] -= m * sys.off[i - 1];
        sys.rhs[i] -= m * sys.rhs[i - 1];
    }
    T.resize(n);
    T[n - 1] = sys.rhs[n - 1] / sys.diag[n - 1];
    for (std::size_t i = n - 1; i-- > 0;)
        T[i] = (sys.rhs[i] - sys.off[i] * T[i + 1]) / sys.diag[i];
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

void solveTemperatures(const RodProblem& p, SolverWorkspace& workspace, std::vector<double>& temperatures) {
    validate(p);
    assemble(p, workspace);
    applyPrescribedTemperature(workspace, 0, p.temp_left_C);
    applyPrescribedTemperature(workspace, static_cast<std::size_t>(p.elements), p.temp_right_C);
    solveTridiagonal(workspace, temperatures);
}

RodSolution solveRod(const RodProblem& p) {
    SolverWorkspace workspace;
    RodSolution out;
    solveTemperatures(p, workspace, out.temperature_C);
    const double h = p.length_m / p.elements;
    out.x_m.resize(out.temperature_C.size());
    for (std::size_t i = 0; i < out.x_m.size(); ++i) out.x_m[i] = static_cast<double>(i) * h;

    // Reaction R = (K T - F) at a prescribed node is the heat the boundary must
    // supply; its negative is the heat leaving there. Row 0 of the unconstrained
    // K is [ke, -ke], so -R0 = fe + ke (T1 - T0); likewise at x = L.
    const auto& T = out.temperature_C;
    const double ke = elementConductance(p), fe = elementLoad(p);
    const std::size_t n = T.size() - 1;
    out.heat_out_left_W = fe + ke * (T[1] - T[0]);
    out.heat_out_right_W = fe + ke * (T[n - 1] - T[n]);
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
