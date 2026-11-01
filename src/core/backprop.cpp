#include "backprop.h"

#include <cmath>

namespace uew {

ForwardPass forward(const Params& p, const Sample& s) {
    const double z = p.w * s.x + p.b;
    const double h = std::tanh(z);
    const double yHat = p.v * h + p.c;
    const double residual = yHat - s.y;
    return {z, h, yHat, 0.5 * residual * residual};
}

Gradients backward(const Params& p, const Sample& s) {
    const ForwardPass f = forward(p, s);
    const double dJ_dyhat = f.y_hat - s.y;
    const double dJ_dz = dJ_dyhat * p.v * (1.0 - f.h * f.h);  // d tanh(z)/dz = 1 - tanh^2(z)
    return {dJ_dz * s.x, dJ_dz, dJ_dyhat * f.h, dJ_dyhat};
}

Gradients finiteDifference(const Params& p, const Sample& s, double eps) {
    auto central = [&](double Params::*member) {
        Params plus = p, minus = p;
        plus.*member += eps;
        minus.*member -= eps;
        return (forward(plus, s).loss - forward(minus, s).loss) / (2.0 * eps);
    };
    return {central(&Params::w), central(&Params::b), central(&Params::v), central(&Params::c)};
}

Params gradientStep(const Params& p, const Gradients& g, double lr) {
    return {p.w - lr * g.dw, p.b - lr * g.db, p.v - lr * g.dv, p.c - lr * g.dc};
}

std::vector<double> lossCurve(Params p, const Sample& s, double lr, int steps) {
    std::vector<double> losses{forward(p, s).loss};
    for (int i = 0; i < steps; ++i) {
        p = gradientStep(p, backward(p, s), lr);
        losses.push_back(forward(p, s).loss);
    }
    return losses;
}

}  // namespace uew
