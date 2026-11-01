#pragma once
// Steady-state 1D thermal finite-element solver for a uniform rod:
//   -d/dx( k A dT/dx ) = q A   on 0 < x < L,   T(0) = T0,  T(L) = TL
// discretised with two-node linear elements (see docs/thermal_fea.md).

#include <vector>

namespace uew {

constexpr double kAbsoluteZeroC = -273.15;
constexpr int kMaxElements = 1'000'000;

struct RodProblem {
    double length_m = 1.0;
    double area_m2 = 1e-4;
    double conductivity_W_per_mK = 10.0;
    double heat_generation_W_per_m3 = 10'000.0;
    double temp_left_C = 100.0;
    double temp_right_C = 20.0;
    int elements = 10;
};

struct RodSolution {
    std::vector<double> x_m;            // node positions
    std::vector<double> temperature_C;  // nodal temperatures
    double heat_out_left_W = 0.0;       // conduction leaving through x = 0 (from nodal reactions)
    double heat_out_right_W = 0.0;      // conduction leaving through x = L
};

struct ErrorMeasures {
    double max_nodal_error_C = 0.0;
    double max_midpoint_error_C = 0.0;  // FE interpolation evaluated at element midpoints
};

// Throws std::invalid_argument with an actionable message for invalid input.
void validate(const RodProblem& problem);

RodSolution solveRod(const RodProblem& problem);

// Same solve, but reuses caller-owned buffers so repeated solves (batch runs)
// do no heap allocation once the buffers have grown to size.
struct SolverWorkspace {
    std::vector<double> diag, off, rhs;
};
void solveTemperatures(const RodProblem& problem, SolverWorkspace& workspace, std::vector<double>& temperatures);

// Analytical solution: T0 + (TL - T0) x / L + q / (2k) * x (L - x).
double exactTemperature(const RodProblem& problem, double x_m);

// Linear FE interpolation of the nodal solution at an arbitrary x in [0, L].
double interpolate(const RodSolution& solution, double x_m);

ErrorMeasures measureError(const RodProblem& problem, const RodSolution& solution);

}  // namespace uew
