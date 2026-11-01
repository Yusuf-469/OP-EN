# Task C: 1D steady thermal FEA - formulation, verification and what I learned

Code: `src/core/thermal_fea.cpp`. Tests: `thermal_*` in `tests/test_core.cpp`. Data: `results/thermal/`.

## Problem and assumptions

Uniform rod, `0 < x < L`, steady state, conduction only:

    -d/dx ( k A dT/dx ) = q A,     T(0) = T0,  T(L) = TL

| Symbol | Value | Unit | Meaning |
|---|---|---|---|
| L | 1 | m | length |
| A | 1e-4 | m² | cross-section area |
| k | 10 | W/(m·K) | thermal conductivity (constant) |
| q | 10 000 | W/m³ | uniform volumetric heat generation |
| T0, TL | 100, 20 | °C | prescribed end temperatures (Dirichlet) |

Assumptions: constant k, A, q; insulated lateral surface (all heat leaves through the ends); 1D (temperature
uniform over the cross-section); steady state. Units are SI throughout; °C is used because only temperature
*differences* enter the equation, so °C and K give identical results.

Exact solution: `T(x) = T0 + (TL - T0) x/L + q/(2k) x (L - x)`. Peak 188.2 °C at x = 0.42 m (where dT/dx = -80 + 500(1 - 2x) = 0).

## Finite-element formulation

1. **Weak form.** Multiply by a test function `v` (zero where T is prescribed) and integrate by parts:
   `∫ k A T' v' dx = ∫ q A v dx`.
2. **Elements.** N two-node linear elements of length `h = L/N`; on each element T is linear between its nodes.
3. **Element matrices** (from the shape functions `N1 = 1 - ξ`, `N2 = ξ`):

       k_e = (k A / h) [  1  -1 ]      f_e = (q A h / 2) [ 1 ]
                       [ -1   1 ]                        [ 1 ]

   i.e. half of each element's generated heat goes to each node.
4. **Assembly.** Add `k_e` into rows/columns (e, e+1) of the global matrix and `f_e` into the load vector. The
   global matrix is symmetric tridiagonal, stored as two arrays.
5. **Boundary conditions.** For each prescribed node p with value Tp: subtract `K(j,p)·Tp` from the right-hand
   side of its neighbour j, zero row and column p, set `K(p,p) = 1`, `F(p) = Tp`. This keeps the matrix symmetric
   and the prescribed values are reproduced exactly (`thermal_boundary_conditions`, error < 1e-12).
6. **Solve.** Thomas algorithm (tridiagonal Gaussian elimination), O(N).

## Verification

| N | h (m) | max nodal error (°C) | max midpoint error (°C) | observed order |
|---|---|---|---|---|
| 10 | 0.1 | 5.7e-14 | 1.250000 | - |
| 20 | 0.05 | 2.6e-13 | 0.312500 | 2.000 |
| 40 | 0.025 | 7.7e-13 | 0.078125 | 2.000 |
| 80 | 0.0125 | 1.1e-12 | 0.019531 | 2.000 |

* Midpoint error is evaluated with the **FE interpolation** `(T_e + T_{e+1})/2` against `T_exact(x_mid)`.
* It equals the theory exactly: linear interpolation of a parabola with curvature `-q/k` errs by
  `(q/k)·h²/8` at the midpoint, i.e. `10000 × 0.1² / (8 × 10) = 1.25 °C` for N = 10. Halving h divides it by 4,
  giving order 2 (test `thermal_midpoint_error_theory`, `thermal_convergence_order`).
* **Energy balance** (`thermal_energy_balance`): from the nodal reactions, 0.42 W leaves at x = 0 and 0.58 W at
  x = L; the sum 1.000 W equals `q A L`. These match `±kA dT/dx` of the exact solution.
* A zero-source case reproduces the straight line exactly (`thermal_no_source_linear`).
* Invalid input (k ≤ 0, N = 0, NaN, T below -273.15 °C) is rejected with a message naming the parameter.

## Debugging insight

Comparing FE and exact values only **at the nodes** gives an error of ~1e-13 for every mesh, so a nodal
"convergence plot" is flat round-off noise and looks like a bug (or like a test that compares the exact solution
with itself). The explanation: for this 1D problem with
linear elements, the FE solution is *nodally exact* (the Green's function of `-d²/dx²` is piecewise linear, so it
lies in the FE space). All the discretisation error lives **between** the nodes, which is why the assignment asks
for midpoint errors. The pointwise error plot (`results/plots/thermal_solution.svg` b) shows it: arches that are
zero at every node and peak at qh²/8k at each midpoint. Note the nodal error actually *grows* with N (5.7e-14 to
1.1e-12): that is floating-point round-off accumulating over more unknowns, not discretisation error.

## Learning notes (interdisciplinary adoption)

From a software/data background, a useful mental model is that FEA is
"write the physics as an integral, approximate it piecewise, assemble a sparse linear system, apply constraints,
solve" - the same shape as many numerical methods. The parts that need real understanding rather than code are: why the weak form
moves a derivative onto the test function (it lets linear elements, whose second derivative is zero, still
represent the problem); why Dirichlet conditions are imposed on the matrix rather than the weak form; and that
"exact at the nodes" does not mean "exact", so the verification metric must be chosen deliberately.
