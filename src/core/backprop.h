#pragma once
// One-hidden-unit regression network trained by hand-derived backpropagation:
//   z = w x + b,  h = tanh(z),  y_hat = v h + c,  J = 0.5 (y_hat - y)^2
// No automatic differentiation: every derivative is written out below.

#include <vector>

namespace uew {

struct Params { double w = 0.5, b = 0.0, v = 1.0, c = 0.0; };
struct Sample { double x = 1.0, y = 2.0; };

struct ForwardPass { double z, h, y_hat, loss; };
struct Gradients { double dw, db, dv, dc; };

ForwardPass forward(const Params& p, const Sample& s);

// Chain rule: dJ/dy_hat = (y_hat - y); dJ/dc = that; dJ/dv = that * h;
// dJ/dz = dJ/dy_hat * v * (1 - h^2); dJ/dw = dJ/dz * x; dJ/db = dJ/dz.
Gradients backward(const Params& p, const Sample& s);

// Central differences: (J(theta + eps) - J(theta - eps)) / (2 eps), one parameter at a time.
Gradients finiteDifference(const Params& p, const Sample& s, double eps);

// Simultaneous update: all four gradients come from the same (old) parameters.
Params gradientStep(const Params& p, const Gradients& g, double learningRate);

// Loss before step 0 and after each of `steps` updates (steps + 1 values).
std::vector<double> lossCurve(Params p, const Sample& s, double learningRate, int steps);

}  // namespace uew
