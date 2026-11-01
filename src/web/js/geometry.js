// Geometry view: STL triangles and nodal temperature point cloud in one
// three.js scene. Parsing/validation happens in C++ (/api/geometry/stl,
// /api/results/csv); this module only renders what the backend returns.

import * as THREE from 'three';
import { OrbitControls } from '../vendor/OrbitControls.js';
import { api, setStatus, fmt, facts, el } from './dom.js';

const MAX_UPLOAD_BYTES = 128 * 1024 * 1024;  // matches kMaxInputBytes in geometry_io.h
// Viridis: perceptually uniform and colour-blind safe (unlike a rainbow map).
const VIRIDIS = ['#440154', '#482878', '#3e4989', '#31688e', '#26828e', '#1f9e89', '#35b779', '#6ece58', '#b5de2b', '#fde725']
  .map(c => new THREE.Color(c));

function colormap(t) {
  const x = Math.min(Math.max(t, 0), 1) * (VIRIDIS.length - 1);
  const i = Math.min(Math.floor(x), VIRIDIS.length - 2);
  return VIRIDIS[i].clone().lerp(VIRIDIS[i + 1], x - i);
}

const $ = id => document.getElementById(id);
const state = { meshData: null, nodes: null, meshObj: null, pointsObj: null, marker: null, abort: null };
let scene, camera, renderer, controls;

function initScene() {
  const host = $('viewer');
  scene = new THREE.Scene();
  camera = new THREE.PerspectiveCamera(45, 1, 1e-4, 1e4);
  camera.position.set(0.15, -0.2, 0.15);
  camera.up.set(0, 0, 1);  // CAD convention: z up
  renderer = new THREE.WebGLRenderer({ antialias: true, preserveDrawingBuffer: true });
  renderer.setPixelRatio(window.devicePixelRatio);
  host.append(renderer.domElement);
  controls = new OrbitControls(camera, renderer.domElement);
  controls.enableDamping = !matchMedia('(prefers-reduced-motion: reduce)').matches;
  controls.listenToKeyEvents(host);  // arrow keys pan when the viewer has focus

  scene.add(new THREE.HemisphereLight(0xffffff, 0x8899aa, 1.6));
  const sun = new THREE.DirectionalLight(0xffffff, 1.6);
  sun.position.set(1, -1, 2);
  camera.add(sun);
  scene.add(camera);

  new ResizeObserver(() => {
    const { clientWidth: w, clientHeight: h } = host;
    renderer.setSize(w, h);
    camera.aspect = w / h;
    camera.updateProjectionMatrix();
  }).observe(host);
  renderer.domElement.addEventListener('click', inspectNode);
  (function loop() { controls.update(); renderer.render(scene, camera); requestAnimationFrame(loop); })();
}

function stlScale() { return Number($('stl-units').value); }

function buildMesh() {
  if (state.meshObj) { scene.remove(state.meshObj); state.meshObj.geometry.dispose(); }
  if (!state.meshData) return;
  const s = stlScale();
  const positions = Float32Array.from(state.meshData.vertices, v => v * s);
  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
  geometry.computeVertexNormals();
  const material = new THREE.MeshStandardMaterial({ color: 0x9aa9b9, metalness: 0.15, roughness: 0.6,
    side: THREE.DoubleSide, wireframe: $('show-wire').checked, transparent: true, opacity: state.nodes ? 0.55 : 1 });
  state.meshObj = new THREE.Mesh(geometry, material);
  state.meshObj.visible = $('show-mesh').checked;
  scene.add(state.meshObj);
}

function buildPoints() {
  if (state.pointsObj) { scene.remove(state.pointsObj); state.pointsObj.geometry.dispose(); }
  if (!state.nodes) return;
  const { xyz_m, T_C, min_C, max_C } = state.nodes;
  const span = max_C - min_C || 1;
  const colors = new Float32Array(T_C.length * 3);
  T_C.forEach((t, i) => colormap((t - min_C) / span).toArray(colors, i * 3));
  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute('position', new THREE.BufferAttribute(Float32Array.from(xyz_m), 3));
  geometry.setAttribute('color', new THREE.BufferAttribute(colors, 3));
  const material = new THREE.PointsMaterial({ size: Number($('point-size').value), sizeAttenuation: false, vertexColors: true });
  state.pointsObj = new THREE.Points(geometry, material);
  state.pointsObj.visible = $('show-points').checked;
  scene.add(state.pointsObj);
  drawLegend(min_C, max_C);
}

function drawLegend(min, max) {
  const canvas = $('legend-bar');
  const ctx = canvas.getContext('2d');
  for (let y = 0; y < canvas.height; y++) {
    ctx.fillStyle = '#' + colormap(1 - y / (canvas.height - 1)).getHexString();
    ctx.fillRect(0, y, canvas.width, 1);
  }
  const ticks = [0, 0.25, 0.5, 0.75, 1].map(f => el('span', { style: `top:${(1 - f) * 100}%` }, fmt(min + f * (max - min), 4)));
  $('legend-ticks').replaceChildren(...ticks);
  $('legend-ticks').style.height = `${canvas.height}px`;
  $('legend').hidden = false;
}

function fitView() {
  const box = new THREE.Box3();
  for (const obj of [state.meshObj, state.pointsObj]) if (obj?.visible) box.expandByObject(obj);
  if (box.isEmpty()) return;
  const sphere = box.getBoundingSphere(new THREE.Sphere());
  const dist = sphere.radius / Math.sin(THREE.MathUtils.degToRad(camera.fov / 2)) * 1.15;
  const dir = new THREE.Vector3(1, -1.3, 0.9).normalize();
  camera.position.copy(sphere.center).addScaledVector(dir, dist);
  camera.near = dist / 1000;
  camera.far = dist * 100;
  camera.updateProjectionMatrix();
  controls.target.copy(sphere.center);
  controls.update();
}

// Keyboard/button alternative to pointer orbiting (accessibility fallback for the 3D view).
function cameraStep(action) {
  if (action === 'fit') return fitView();
  const offset = camera.position.clone().sub(controls.target);
  if (action === 'in' || action === 'out') offset.multiplyScalar(action === 'in' ? 0.8 : 1.25);
  else offset.applyAxisAngle(camera.up, (action === 'left' ? 1 : -1) * Math.PI / 12);
  camera.position.copy(controls.target).add(offset);
  controls.update();
}

function extremeNode(n, hottest) {
  let i = 0;
  for (let j = 1; j < n.T_C.length; j++) if (hottest ? n.T_C[j] > n.T_C[i] : n.T_C[j] < n.T_C[i]) i = j;
  return `node ${n.id[i]} at (${[0, 1, 2].map(k => fmt(n.xyz_m[3 * i + k], 3)).join(', ')}) m`;
}

function inspectNode(event) {
  if (!state.pointsObj?.visible) return;
  const rect = renderer.domElement.getBoundingClientRect();
  const mouse = new THREE.Vector2(((event.clientX - rect.left) / rect.width) * 2 - 1, -((event.clientY - rect.top) / rect.height) * 2 + 1);
  const raycaster = new THREE.Raycaster();
  const radius = state.pointsObj.geometry.boundingSphere?.radius ?? 1;
  raycaster.params.Points.threshold = radius * 0.015;
  raycaster.setFromCamera(mouse, camera);
  const hit = raycaster.intersectObject(state.pointsObj)[0];
  if (!hit) return;
  const i = hit.index, n = state.nodes;
  const box = $('inspect');
  box.replaceChildren(el('b', {}, `Node ${n.id[i]}`), el('br'),
    `x = ${fmt(n.xyz_m[3 * i], 5)} m, y = ${fmt(n.xyz_m[3 * i + 1], 5)} m, z = ${fmt(n.xyz_m[3 * i + 2], 5)} m`, el('br'),
    el('b', {}, `T = ${fmt(n.T_C[i], 5)} °C`));
  box.hidden = false;
  if (!state.marker) {
    state.marker = new THREE.Mesh(new THREE.SphereGeometry(1, 16, 12), new THREE.MeshBasicMaterial({ color: 0xff3b30 }));
    scene.add(state.marker);
  }
  state.marker.scale.setScalar(radius * 0.012);
  state.marker.position.copy(hit.point);
}

async function upload(route, bytes, label) {
  if (bytes.byteLength > MAX_UPLOAD_BYTES) throw new Error(`${label} is larger than the tested 128 MB limit.`);
  state.abort = new AbortController();
  $('cancel-upload').hidden = false;
  setStatus(`Parsing ${label} in C++…`);
  try {
    return await api(route, { method: 'POST', body: bytes, signal: state.abort.signal });
  } finally {
    $('cancel-upload').hidden = true;
    state.abort = null;
  }
}

async function loadStl(bytes, name) {
  try {
    const t0 = performance.now();
    const mesh = await upload('/api/geometry/stl', bytes, name);
    state.meshData = mesh;
    buildMesh();
    showStlFacts(mesh, name);
    fitView();
    $('viewer-empty').hidden = true;
    setStatus(`Loaded ${name}: ${mesh.triangles} triangles in ${fmt(performance.now() - t0, 3)} ms.`);
  } catch (err) {
    if (err.name !== 'AbortError') { setStatus(`${name}: ${err.message}`, true); facts($('stl-info'), [['Error', err.message]]); }
    else setStatus('Loading cancelled; previous geometry kept.');
  }
}

function showStlFacts(mesh, name) {
  const u = $('stl-units').selectedOptions[0].textContent.match(/\((.+)\)/)[1];
  const dims = mesh.max.map((v, i) => fmt(v - mesh.min[i], 4)).join(' × ');
  facts($('stl-info'), [['File', name], ['Solid', mesh.solid_name || '(unnamed)'], ['Triangles', mesh.triangles.toLocaleString()],
    ['Size', `${dims} ${u}`], ['Area', `${fmt(mesh.surface_area, 4)} ${u}²`], ['Degenerate', String(mesh.degenerate_triangles)]]);
}

async function loadCsv(bytes, name) {
  try {
    const nodes = await upload('/api/results/csv', bytes, name);
    state.nodes = nodes;
    buildPoints();
    buildMesh();  // re-apply transparency so nodes inside the solid stay visible
    facts($('csv-info'), [['File', name], ['Nodes', nodes.count.toLocaleString()], ['T min', `${fmt(nodes.min_C, 5)} °C`],
      ['T max', `${fmt(nodes.max_C, 5)} °C`], ['T mean', `${fmt(nodes.mean_C, 5)} °C`],
      ['Hottest', extremeNode(nodes, true)], ['Coldest', extremeNode(nodes, false)]]);
    fitView();
    $('viewer-empty').hidden = true;
    setStatus(`Loaded ${name}: ${nodes.count} nodes.`);
  } catch (err) {
    if (err.name !== 'AbortError') { setStatus(`${name}: ${err.message}`, true); facts($('csv-info'), [['Error', err.message]]); }
    else setStatus('Loading cancelled; previous nodes kept.');
  }
}

async function loadSample(path, loader) {
  const res = await fetch(path);
  if (!res.ok) return setStatus(`Sample ${path} is missing from the package (data/ folder).`, true);
  await loader(await res.arrayBuffer(), path.split('/').pop());
}

export function initGeometry() {
  initScene();
  $('stl-file').addEventListener('change', async e => {
    const f = e.target.files[0];
    if (f) await loadStl(await f.arrayBuffer(), f.name);
    e.target.value = '';
  });
  $('csv-file').addEventListener('change', async e => {
    const f = e.target.files[0];
    if (f) await loadCsv(await f.arrayBuffer(), f.name);
    e.target.value = '';
  });
  $('stl-sample').addEventListener('click', () => loadSample('/samples/heat_sink.stl', loadStl));
  $('csv-sample').addEventListener('click', () => loadSample('/samples/heat_sink_temperature_synthetic.csv', loadCsv));
  $('stl-units').addEventListener('change', () => { buildMesh(); if (state.meshData) showStlFacts(state.meshData, $('stl-info').querySelector('dd')?.textContent); fitView(); });
  $('show-mesh').addEventListener('change', e => { if (state.meshObj) state.meshObj.visible = e.target.checked; });
  $('show-wire').addEventListener('change', e => { if (state.meshObj) state.meshObj.material.wireframe = e.target.checked; });
  $('show-points').addEventListener('change', e => { if (state.pointsObj) state.pointsObj.visible = e.target.checked; });
  $('point-size').addEventListener('input', e => {
    $('point-size-out').textContent = e.target.value;
    if (state.pointsObj) state.pointsObj.material.size = Number(e.target.value);
  });
  $('fit-view').addEventListener('click', fitView);
  for (const b of document.querySelectorAll('[data-cam]')) b.addEventListener('click', () => cameraStep(b.dataset.cam));
  $('cancel-upload').addEventListener('click', () => state.abort?.abort());
}
