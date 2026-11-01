# Task F: reference values for checking the handwritten derivation

The handwritten derivation (appendix, max two pages) must be done by hand by the candidate. This page lists the
values computed by the C++ implementation (`results/backprop/backprop.json`) so the hand calculation can be
checked afterwards. It is a checking aid, not a substitute for the handwritten work.

Model: `z = wx + b`, `h = tanh(z)`, `ŷ = vh + c`, `J = ½(ŷ − y)²`; `x = 1`, `y = 2`; start `w = 0.5, b = 0, v = 1, c = 0`; learning rate `η = 0.1`.

## Forward pass

| Quantity | Value |
|---|---|
| z = 0.5·1 + 0 | 0.5 |
| h = tanh(0.5) | 0.462117157 |
| ŷ = 1·h + 0 | 0.462117157 |
| ŷ − y | −1.537882843 |
| J = ½(ŷ − y)² | 1.182541819 |

## Chain rule (all evaluated at the original parameters)

| Derivative | Expression | Value |
|---|---|---|
| ∂J/∂ŷ | ŷ − y | −1.537882843 |
| ∂J/∂c | ∂J/∂ŷ · 1 | −1.537882843 |
| ∂J/∂v | ∂J/∂ŷ · h | −0.710682047 |
| ∂J/∂h | ∂J/∂ŷ · v | −1.537882843 |
| ∂h/∂z | 1 − tanh²(z) = 1 − 0.213552 | 0.786447733 |
| ∂J/∂z | ∂J/∂h · ∂h/∂z | −1.209464475 |
| ∂J/∂w | ∂J/∂z · x | −1.209464475 |
| ∂J/∂b | ∂J/∂z · 1 | −1.209464475 |

Central differences (ε = 1e−6) agree with every analytic gradient to about 1e−10 (requirement ≤ 1e−5).

## One simultaneous update, θ ← θ − η·∂J/∂θ

| Parameter | Before | After |
|---|---|---|
| w | 0.5 | 0.620946448 |
| b | 0 | 0.120946448 |
| v | 1 | 1.071068205 |
| c | 0 | 0.153788284 |

Loss after this step: 0.685773765 (down from 1.182541819).

## Gradient direction

All four gradients are negative because the prediction is too low (ŷ ≈ 0.46 < y = 2): increasing any parameter
increases ŷ (h > 0 and v > 0, and tanh is increasing), so it reduces the error. Gradient descent moves against the
gradient, so it increases all four parameters. c moves most because ∂ŷ/∂c = 1; w and b move less because the
signal is scaled by v·(1 − h²) ≈ 0.79 through the tanh.

## Loss over 100 further steps (`results/backprop/backprop_loss.csv`, `results/plots/backprop_loss.svg`)

Step 0: 1.18; step 1: 0.686; step 10: 1.0e−2; step 50: 1.3e−10; step 100: 1.7e−20. The loss falls by a roughly
constant factor (~0.63) per step, i.e. linear convergence, visible as a straight line on the log axis. It goes to
(numerically) zero because a single sample with four free parameters can be fitted exactly; this says nothing
about generalisation.
