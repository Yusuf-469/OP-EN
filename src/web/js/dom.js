// Shared helpers: element builder, API calls, status bar and number formatting.

/** Creates an element. Strings become text nodes, so no HTML injection is possible. */
export function el(tag, attrs = {}, ...children) {
  const node = document.createElement(tag);
  for (const [key, value] of Object.entries(attrs)) {
    if (key === 'class') node.className = value;
    else if (key.startsWith('on')) node.addEventListener(key.slice(2), value);
    else if (value !== false && value != null) node.setAttribute(key, value === true ? '' : value);
  }
  for (const child of children.flat()) {
    if (child != null) node.append(child instanceof Node ? child : String(child));
  }
  return node;
}

/** Calls the C++ API; throws Error(message from server) on any non-2xx reply. */
export async function api(path, { method = 'GET', body, signal } = {}) {
  let res;
  try {
    const headers = body ? { 'Content-Type': 'application/octet-stream' } : {};  // raw file upload
    res = await fetch(path, { method, body, signal, headers });
  } catch (err) {
    if (err.name === 'AbortError') throw err;
    throw new Error('The workbench backend is not reachable. Is Launch.exe still running?');
  }
  const data = await res.json().catch(() => ({ error: `Server replied ${res.status} without a JSON body.` }));
  if (!res.ok) throw new Error(data.error || `Request failed (${res.status}).`);
  return data;
}

const statusEl = () => document.getElementById('status');

export function setStatus(message, isError = false) {
  const node = statusEl();
  node.textContent = `${new Date().toLocaleTimeString()} — ${message}`;
  node.className = isError ? 'err' : '';
}

/** Formats a number with `digits` significant figures, using exponent form for very small/large values. */
export function fmt(value, digits = 4) {
  if (value == null || !Number.isFinite(value)) return '—';
  const abs = Math.abs(value);
  if (abs !== 0 && (abs < 1e-3 || abs >= 1e6)) return value.toExponential(digits - 1);
  return Number(value.toPrecision(digits)).toString();
}

export function table(headers, rows) {
  return el('table', {},
    el('thead', {}, el('tr', {}, headers.map(h => el('th', { scope: 'col' }, h)))),
    el('tbody', {}, rows.map(r => el('tr', {}, r.map(c => el('td', {}, c))))));
}

export function facts(dl, pairs) {
  dl.replaceChildren(...pairs.flatMap(([k, v]) => [el('dt', {}, k), el('dd', {}, v)]));
}
