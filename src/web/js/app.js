// Entry point: tab switching, backend health check and view initialisation.

import { api, setStatus } from './dom.js';
import { initGeometry } from './geometry.js';
import { initThermal } from './thermal.js';
import { initPerformance, showSystemInfo } from './performance.js';
import { initHardware } from './hardware.js';
import { initBackprop } from './backprop.js';

const tabs = [...document.querySelectorAll('[role=tab]')];

function selectTab(tab, focus = false) {
  for (const t of tabs) {
    const selected = t === tab;
    t.setAttribute('aria-selected', String(selected));
    t.tabIndex = selected ? 0 : -1;
    document.getElementById(t.getAttribute('aria-controls')).hidden = !selected;
  }
  if (focus) tab.focus();
  history.replaceState(null, '', `#${tab.id.replace('tab-', '')}`);
}

function initTabs() {
  tabs.forEach((tab, i) => {
    tab.addEventListener('click', () => selectTab(tab));
    // WAI-ARIA tabs pattern: arrow keys move between tabs.
    tab.addEventListener('keydown', e => {
      const step = { ArrowRight: 1, ArrowLeft: -1 }[e.key];
      if (step) selectTab(tabs[(i + step + tabs.length) % tabs.length], true);
    });
  });
  const fromHash = document.getElementById(`tab-${location.hash.slice(1)}`);
  if (fromHash) selectTab(fromHash);
}

async function main() {
  initTabs();
  const pill = document.getElementById('server-status');
  try {
    const health = await api('/api/health');
    pill.textContent = `C++ backend ${health.version} connected`;
    pill.className = 'pill pill-ok';
  } catch (err) {
    pill.textContent = 'Backend unreachable';
    pill.className = 'pill pill-bad';
    setStatus(err.message, true);
    return;
  }
  initGeometry();
  initThermal();
  initPerformance();
  initHardware();
  await initBackprop();
  await api('/api/ui-ready', { method: 'POST' });  // launch-to-ready timing
  showSystemInfo();
  setStatus('Workbench ready.');
}

main();
