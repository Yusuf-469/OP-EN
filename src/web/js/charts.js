// Small dependency-free SVG line chart with labelled axes, units, legend and
// optional log scales. Used for every plot in the UI.

import { fmt } from './dom.js';

const NS = 'http://www.w3.org/2000/svg';
export const PALETTE = ['#0b5cad', '#c4421a', '#2e8540', '#7b3fa0', '#a06d00', '#00838f'];

function svg(tag, attrs = {}, text) {
  const node = document.createElementNS(NS, tag);
  for (const [k, v] of Object.entries(attrs)) node.setAttribute(k, v);
  if (text != null) node.textContent = text;
  return node;
}

function niceTicks(min, max, count = 6) {
  const span = max - min || Math.abs(max) || 1;
  const raw = span / count;
  const mag = 10 ** Math.floor(Math.log10(raw));
  const step = [1, 2, 5, 10].find(m => m * mag >= raw) * mag;
  const ticks = [];
  for (let v = Math.ceil(min / step) * step; v <= max + step * 1e-9; v += step) ticks.push(Number(v.toPrecision(12)));
  return ticks;
}

function logTicks(min, max) {
  const ticks = [];
  for (let e = Math.floor(Math.log10(min)); e <= Math.ceil(Math.log10(max)); e++) ticks.push(10 ** e);
  return ticks;
}

function extent(values, log) {
  const finite = values.filter(v => Number.isFinite(v) && (!log || v > 0));
  let lo = Math.min(...finite), hi = Math.max(...finite);
  if (log) return [10 ** Math.floor(Math.log10(lo)), 10 ** Math.ceil(Math.log10(hi))];
  if (lo === hi) { lo -= 1; hi += 1; }
  const pad = (hi - lo) * 0.05;
  return [lo - pad, hi + pad];
}

/**
 * series: [{ name, x: [], y: [], color?, dash?, markers?, line? }]
 * opts:   { title, xLabel, yLabel, logX, logY }
 */
export function lineChart(container, series, opts) {
  const W = 760, H = 380, m = { l: 78, r: 16, t: 34, b: 58 };
  const [x0, x1] = extent(series.flatMap(s => s.x), opts.logX);
  const [y0, y1] = extent(series.flatMap(s => s.y), opts.logY);
  const scale = (v, a, b, p0, p1, log) => {
    const t = log ? (Math.log10(v) - Math.log10(a)) / (Math.log10(b) - Math.log10(a)) : (v - a) / (b - a);
    return p0 + t * (p1 - p0);
  };
  const X = v => scale(v, x0, x1, m.l, W - m.r, opts.logX);
  const Y = v => scale(v, y0, y1, H - m.b, m.t, opts.logY);

  const root = svg('svg', { viewBox: `0 0 ${W} ${H}`, role: 'img', 'aria-label': opts.title });
  root.append(svg('title', {}, opts.title));
  const grid = svg('g', { stroke: '#dde3ea', 'stroke-width': 1 });
  const labels = svg('g', { 'font-size': 13 });
  for (const t of opts.logX ? logTicks(x0, x1) : niceTicks(x0, x1)) {
    grid.append(svg('line', { x1: X(t), x2: X(t), y1: m.t, y2: H - m.b }));
    labels.append(svg('text', { x: X(t), y: H - m.b + 18, 'text-anchor': 'middle' }, fmt(t, 3)));
  }
  for (const t of opts.logY ? logTicks(y0, y1) : niceTicks(y0, y1)) {
    grid.append(svg('line', { x1: m.l, x2: W - m.r, y1: Y(t), y2: Y(t) }));
    labels.append(svg('text', { x: m.l - 8, y: Y(t) + 4, 'text-anchor': 'end' }, fmt(t, 3)));
  }
  root.append(grid, labels,
    svg('rect', { x: m.l, y: m.t, width: W - m.l - m.r, height: H - m.t - m.b, fill: 'none', stroke: '#8796a5' }),
    svg('text', { x: (m.l + W - m.r) / 2, y: H - 14, 'text-anchor': 'middle', 'font-size': 14, 'font-weight': 600 }, opts.xLabel),
    svg('text', { x: 18, y: (m.t + H - m.b) / 2, 'text-anchor': 'middle', 'font-size': 14, 'font-weight': 600,
      transform: `rotate(-90 18 ${(m.t + H - m.b) / 2})` }, opts.yLabel));

  series.forEach((s, i) => {
    const color = s.color || PALETTE[i % PALETTE.length];
    const pts = s.x.map((x, j) => [x, s.y[j]]).filter(([x, y]) => Number.isFinite(x) && Number.isFinite(y)
      && (!opts.logX || x > 0) && (!opts.logY || y > 0));
    if (s.line !== false) {
      root.append(svg('polyline', { points: pts.map(([x, y]) => `${X(x)},${Y(y)}`).join(' '), fill: 'none',
        stroke: color, 'stroke-width': s.width || 2, 'stroke-dasharray': s.dash || '' }));
    }
    if (s.markers) {
      for (const [x, y] of pts) {
        const dot = svg('circle', { cx: X(x), cy: Y(y), r: 3.5, fill: color });
        dot.append(svg('title', {}, `${s.name}: x = ${fmt(x, 5)}, y = ${fmt(y, 5)}`));
        root.append(dot);
      }
    }
    // Legend above the plot area, three entries per row.
    const lx = m.l + 10 + (i % 3) * 210, ly = 13 + Math.floor(i / 3) * 15;
    root.append(svg('line', { x1: lx, x2: lx + 22, y1: ly - 4, y2: ly - 4, stroke: color, 'stroke-width': 3,
      'stroke-dasharray': s.dash || '' }),
    svg('text', { x: lx + 28, y: ly, 'font-size': 12 }, s.name));
  });
  container.replaceChildren(root);
}
