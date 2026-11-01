// Backprop view: shows the C++ hand-derived gradients and their numerical check.

import { api, setStatus, fmt, table, el } from './dom.js';
import { lineChart } from './charts.js';

const TOLERANCE = 1e-5;

export async function initBackprop() {
  try {
    const r = await api('/api/backprop');
    const f = r.forward;
    const rows = ['w', 'b', 'v', 'c'].map(p => {
      const err = Math.abs(r.analytic[p] - r.finite_difference[p]);
      return [p, fmt(r.initial[p], 6), fmt(r.analytic[p], 8), fmt(r.finite_difference[p], 8),
        el('span', { class: err <= TOLERANCE ? 'ok' : 'bad' }, `${fmt(err, 2)} ${err <= TOLERANCE ? '✓' : '✗'}`), fmt(r.updated[p], 8)];
    });
    document.getElementById('bp-table').replaceChildren(
      el('p', {}, `z = ${fmt(f.z, 6)}, h = tanh(z) = ${fmt(f.h, 8)}, ŷ = ${fmt(f.y_hat, 8)}, J = ${fmt(f.loss, 8)}`),
      table(['Param', 'Initial', '∂J/∂θ (chain rule)', 'Central diff.', '|error| ≤ 1e-5', 'After 1 step'], rows),
      el('p', { class: 'hint' }, 'All gradients are negative because ŷ < y: increasing any parameter raises ŷ, so gradient descent increases all four.'));
    const steps = r.losses.map((_, i) => i);
    lineChart(document.getElementById('chart-loss'), [{ name: 'Loss J', x: steps, y: r.losses }],
      { title: 'Training loss over 100 steps', xLabel: 'Gradient-descent step', yLabel: 'Loss J (–, log scale)', logY: true });
  } catch (err) {
    setStatus(err.message, true);
  }
}
