// Hardware view: drives the C++ mock adapters (all behaviour is SIMULATED).

import { api, setStatus, fmt, el } from './dom.js';

const $ = id => document.getElementById(id);

function log(message, kind = '') {
  $('hw-log').prepend(el('li', { class: kind }, `${new Date().toLocaleTimeString()}  ${message}`));
}

function deviceCard(d) {
  const act = (label, action, cls = 'btn btn-ghost') =>
    el('button', { class: cls, onclick: () => run(d.id, action) }, label);
  return el('article', { class: 'device', id: `dev-${d.id}` },
    el('div', { class: 'row between' }, el('h3', {}, `${d.id} · ${d.kind}`),
      el('span', { class: `state state-${d.state}` }, `${d.state} (simulated)`)),
    el('p', { class: 'meta' }, d.model, el('br'), `Link: ${d.host_link}`, el('br'), `I/O: ${d.io_voltage}`),
    el('ul', {}, d.capabilities.map(c => el('li', {}, el('b', {}, c.name), ` — ${c.detail}`))),
    el('div', { class: 'row' },
      d.state === 'connected' ? act('Disconnect', 'disconnect') : act(d.state === 'error' ? 'Reconnect' : 'Connect', 'connect', 'btn'),
      act('Read telemetry', 'telemetry'),
      act('Simulate timeout', 'inject-timeout'),
      act('Simulate persistent fault', 'inject-persistent-timeout')));
}

function replaceCard(device) {
  $(`dev-${device.id}`).replaceWith(deviceCard(device));
}

async function run(id, action) {
  try {
    const r = await api(`/api/hardware/${id}/${action}`, { method: 'POST' });
    replaceCard(r.device);
    if (action === 'telemetry') {
      for (const a of r.attempts) log(`${id} attempt ${a.attempt}: ${a.message}`, a.timed_out ? 'err' : 'okl');
      if (r.telemetry_ok) {
        log(`${id} telemetry #${r.frame.sequence}: ` + r.frame.fields.map(f => `${f.name}=${fmt(f.value, 4)} ${f.unit}`).join(', '), 'okl');
      }
    } else if (action.startsWith('inject')) {
      log(`${id}: fault armed (${action === 'inject-timeout' ? 'next read times out once' : 'next reads keep timing out'}). Now read telemetry.`);
    } else {
      log(`${id}: ${action} → ${r.device.state}`);
    }
    setStatus(`${id}: ${action} done (simulated).`);
  } catch (err) {
    log(`${id}: ${err.message}`, 'err');
    setStatus(err.message, true);
  }
}

async function discover() {
  try {
    const r = await api('/api/hardware');
    $('hw-devices').replaceChildren(...r.devices.map(deviceCard));
    log(`Discovery found ${r.devices.length} simulated devices: ${r.devices.map(d => d.id).join(', ')}.`);
  } catch (err) {
    setStatus(err.message, true);
  }
}

export function initHardware() {
  $('hw-discover').addEventListener('click', discover);
  discover();
}
