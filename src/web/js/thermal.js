// Thermal Analysis view: parameter form -> /api/thermal/study -> plots + table.

import { api, setStatus, fmt, table } from './dom.js';
import { lineChart, PALETTE } from './charts.js';

const $ = id => document.getElementById(id);
const FIELDS = ['L', 'A', 'k', 'q', 'T0', 'TL', 'N'];
const DEFAULTS = { L: '1', A: '1e-4', k: '10', q: '10000', T0: '100', TL: '20', N: '10,20,40,80' };

/** Client-side checks for instant feedback; the C++ backend validates again authoritatively. */
function readForm() {
  const values = {}, errors = [];
  for (const name of FIELDS) {
    const input = $(`p-${name}`);
    const raw = input.value.trim();
    let problem = '';
    if (name === 'N') {
      const parts = raw.split(',').map(s => s.trim());
      if (!parts.every(p => /^\d+$/.test(p) && +p >= 1 && +p <= 100000)) problem = 'Meshes N must be whole numbers from 1 to 100000, separated by commas.';
    } else if (raw === '' || !Number.isFinite(Number(raw))) {
      problem = `${input.labels[0].firstChild.textContent.trim()} must be a number.`;
    } else if (['L', 'A', 'k'].includes(name) && Number(raw) <= 0) {
      problem = `${input.labels[0].firstChild.textContent.trim()} must be greater than zero.`;
    } else if (['T0', 'TL'].includes(name) && Number(raw) < -273.15) {
      problem = 'Temperatures cannot be below absolute zero (−273.15 °C).';
    }
    input.setAttribute('aria-invalid', problem ? 'true' : 'false');
    if (problem) errors.push(problem);
    values[name] = name === 'N' ? raw.replace(/\s/g, '') : raw;
  }
  return { values, errors };
}

function render(study) {
  const exact = study.exact;
  const series = [{ name: 'Exact T(x)', x: exact.x_m, y: exact.T_C, color: '#17202b', width: 2.5 }];
  study.meshes.forEach((m, i) => series.push({ name: `FE, N = ${m.N}`, x: m.x_m, y: m.T_C,
    color: PALETTE[(i + 1) % PALETTE.length], dash: '6 3', markers: m.N <= 40 }));
  lineChart($('chart-T'), series, { title: 'Temperature along the rod: FE vs exact', xLabel: 'Position x (m)', yLabel: 'Temperature T (°C)' });

  const h = study.meshes.map(m => m.h_m), e = study.meshes.map(m => m.max_midpoint_error_C);
  const ref = h.map(v => e[0] * (v / h[0]) ** 2);
  lineChart($('chart-conv'), [
    { name: 'Max midpoint error', x: h, y: e, markers: true },
    { name: 'Reference slope 2 (∝ h²)', x: h, y: ref, dash: '4 4', color: '#8796a5' },
  ], { title: 'Mesh convergence of midpoint error', xLabel: 'Element size h (m)', yLabel: 'Max |T_FE − T_exact| (°C)', logX: true, logY: true });

  $('thermal-table').replaceChildren(table(['N', 'h (m)', 'Max nodal err (°C)', 'Max midpoint err (°C)', 'Order'],
    study.meshes.map(m => [m.N, fmt(m.h_m, 4), fmt(m.max_nodal_error_C, 3), fmt(m.max_midpoint_error_C, 5), fmt(m.observed_order, 4)])));
  const last = study.meshes[study.meshes.length - 1];
  $('energy').textContent = `Energy balance (N = ${last.N}): heat out at x=0 ${fmt(last.heat_out_left_W, 5)} W + at x=L `
    + `${fmt(last.heat_out_right_W, 5)} W = ${fmt(last.heat_out_left_W + last.heat_out_right_W, 6)} W (should equal qAL).`;
}

async function solve(event) {
  event?.preventDefault();
  const { values, errors } = readForm();
  $('thermal-error').textContent = errors.join(' ');
  if (errors.length) return setStatus('Fix the highlighted thermal parameters.', true);
  const query = new URLSearchParams(values).toString();
  for (const [id, file] of [['exp-nodes', 'nodes'], ['exp-mid', 'midpoints'], ['exp-conv', 'convergence']])
    $(id).href = `/api/thermal/export?${query}&file=${file}`;
  try {
    setStatus('Solving in C++…');
    render(await api(`/api/thermal/study?${query}`));
    setStatus(`Solved meshes N = ${values.N}.`);
  } catch (err) {
    $('thermal-error').textContent = err.message;
    setStatus(err.message, true);
  }
}

export function initThermal() {
  $('thermal-form').addEventListener('submit', solve);
  $('thermal-reset').addEventListener('click', () => {
    for (const [k, v] of Object.entries(DEFAULTS)) $(`p-${k}`).value = v;
    solve();
  });
  solve();
}
