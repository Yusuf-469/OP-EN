// Performance view: starts a benchmark job, polls progress, supports cancel.

import { api, setStatus, fmt, table, facts, el } from './dom.js';
import { lineChart } from './charts.js';

const $ = id => document.getElementById(id);
const POLL_MS = 250;
let jobId = null;

export async function showSystemInfo() {
  try {
    const { system: s, launch_to_server_ready_ms: server, launch_to_ui_ready_ms: ui } = await api('/api/system');
    facts($('sys-info'), [['CPU', s.cpu_model], ['Cores', `${s.physical_cores} physical / ${s.logical_cpus} logical`],
      ['RAM', `${fmt(s.ram_GiB, 3)} GiB`], ['OS', s.os], ['Compiler', s.compiler], ['Flags', s.build_flags],
      ['Launch → server ready', `${fmt(server, 4)} ms`], ['Launch → UI ready', ui > 0 ? `${fmt(ui, 4)} ms` : 'pending']]);
  } catch (err) {
    setStatus(err.message, true);
  }
}

function render(result, id) {
  const b = result.benchmark;
  const rows = b.rows.map(r => [r.variant === 'serial' ? 'Serial loop' : `Worker pool`, r.workers, fmt(r.median_ms, 5),
    r.samples_ms.map(v => fmt(v, 4)).join(', '), fmt(r.speedup, 3), fmt(r.efficiency, 3),
    el('span', { class: r.max_abs_diff_C <= b.agreement_tolerance_C ? 'ok' : 'bad' }, fmt(r.max_abs_diff_C, 2))]);
  $('bench-table').replaceChildren(
    el('p', { class: 'hint' }, `${b.cases} cases × ${b.elements_per_case} elements, seed ${b.seed}; warm-up ${fmt(b.warmup_ms, 4)} ms; `
      + `${b.repetitions} timed repetitions; ${b.logical_cpus} logical CPUs.`),
    table(['Variant', 'p', 'Median (ms)', 'Samples (ms)', 'Speed-up T₁/Tₚ', 'Efficiency', 'Max |ΔT| vs serial (°C)'], rows),
    el('p', { class: b.agreement_ok ? 'ok' : 'bad' }, b.agreement_ok
      ? `Agreement check passed: every pool result is within ${b.agreement_tolerance_C} °C of the serial result.`
      : 'Agreement check FAILED.'));
  $('bench-downloads').replaceChildren(...['benchmark_summary.csv', 'benchmark_samples.csv', 'benchmark.json']
    .map(f => el('a', { class: 'btn btn-ghost', href: `/api/jobs/${id}/files/${f}`, download: f }, f)));
  const pools = b.rows.filter(r => r.variant === 'pool');
  const p = pools.map(r => r.workers);
  lineChart($('chart-speedup'), [
    { name: 'Measured speed-up', x: p, y: pools.map(r => r.speedup), markers: true },
    { name: 'Ideal (= p)', x: p, y: p, dash: '5 4', color: '#8796a5' },
  ], { title: 'Speed-up versus number of workers', xLabel: 'Workers p', yLabel: 'Speed-up T_serial / T_p (–)' });
}

async function poll(id) {
  const job = await api(`/api/jobs/${id}`);
  $('bench-progress').value = job.total ? job.completed / job.total : 0;
  $('bench-stage').textContent = `${job.stage} — ${fmt(job.elapsed_ms / 1000, 3)} s`;
  if (job.state === 'running') return setTimeout(() => poll(id).catch(fail), POLL_MS);
  finish();
  if (job.state === 'done') { render(job.result, id); setStatus('Benchmark finished.'); }
  else if (job.state === 'cancelled') { $('bench-stage').textContent = 'Cancelled — previous results kept.'; setStatus(job.error); }
  else fail(new Error(job.error));
}

function finish() {
  jobId = null;
  $('bench-start').disabled = false;
  $('bench-cancel').disabled = true;
}

function fail(err) {
  finish();
  $('bench-error').textContent = err.message;
  setStatus(err.message, true);
}

async function start(event) {
  event.preventDefault();
  const values = Object.fromEntries(new FormData($('bench-form')));
  const bad = Object.entries(values).find(([, v]) => !/^\d+$/.test(v.trim()));
  if (bad) { $('bench-error').textContent = `${bad[0]} must be a whole number.`; return; }
  $('bench-error').textContent = '';
  try {
    jobId = (await api(`/api/benchmark?${new URLSearchParams(values)}`, { method: 'POST' })).job_id;
    $('bench-start').disabled = true;
    $('bench-cancel').disabled = false;
    setStatus('Benchmark running in C++ worker threads; the UI stays responsive.');
    poll(jobId).catch(fail);
  } catch (err) {
    fail(err);
  }
}

export function initPerformance() {
  $('bench-form').addEventListener('submit', start);
  $('bench-cancel').addEventListener('click', async () => {
    if (!jobId) return;
    $('bench-stage').textContent = 'Cancelling…';
    await api(`/api/jobs/${jobId}/cancel`, { method: 'POST' }).catch(fail);
  });
}
