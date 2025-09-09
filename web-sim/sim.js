import createModule from './dist/sim.mjs';

const canvas = document.getElementById('canvas');
const ctx = canvas.getContext('2d');
let Module;
let paused = false;

// Fit canvas to display size
function resizeCanvas() {
  const dpr = window.devicePixelRatio || 1;
  canvas.width = Math.floor(canvas.clientWidth * dpr);
  canvas.height = Math.floor(canvas.clientHeight * dpr);
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
}
window.addEventListener('resize', resizeCanvas);

// UI elements (two trees)
const animA = document.getElementById('animA');
// per-tree specific controls
// const speedA = document.getElementById('speedA');
// const phaseA = document.getElementById('phaseA');
const branchA = document.getElementById('branchA');
const widthA = document.getElementById('widthA');
const levelA = document.getElementById('levelA');

const animB = document.getElementById('animB');
// const speedB = document.getElementById('speedB');
// const phaseB = document.getElementById('phaseB');

// Global controls
const speedGlobal = document.getElementById('speedGlobal');
const phaseGlobal = document.getElementById('phaseGlobal');
const branchB = document.getElementById('branchB');
const widthB = document.getElementById('widthB');
const levelB = document.getElementById('levelB');
const invertA = document.getElementById('invertA');
const invertB = document.getElementById('invertB');
const pauseBtn = document.getElementById('pauseBtn');

pauseBtn.onclick = () => { paused = !paused; pauseBtn.textContent = paused ? 'Resume' : 'Pause'; };

// Map [0..1] to black->purple. We'll choose purple rgb(180, 80, 255)
function toPurple(v) {
  const r = Math.floor(180 * v);
  const g = Math.floor(80 * v);
  const b = Math.floor(255 * v);
  return `rgb(${r},${g},${b})`;
}

// Compute snowflake layout positions (4 branches, 7 LEDs each) with side twigs
function computeLayout(BRANCHES, LEDS_PER_BRANCH, radius = 300) {
  const positions = [];
  const connectors = []; // lines for side twigs
  const branchTips = [];

  // Branch angles: 0, 90, 180, 270 deg
  const angles = Array.from({length: BRANCHES}, (_, b) => (Math.PI/2) * b);

  // If exactly 7 LEDs per branch, use a prettier twig pattern; else fall back to evenly spaced straight line
  const hasPattern = (LEDS_PER_BRANCH === 7);
  const tSeq = hasPattern ? [0.14, 0.26, 0.42, 0.58, 0.74, 0.88, 1.00] : Array.from({length: LEDS_PER_BRANCH}, (_, i) => (i+1)/(LEDS_PER_BRANCH+1));
  const lSeq = hasPattern ? [0.00, 0.14, 0.00, -0.14, 0.00, 0.12, 0.00] : Array.from({length: LEDS_PER_BRANCH}, () => 0.0);

  for (let b = 0; b < BRANCHES; b++) {
    const a = angles[b];
    const ca = Math.cos(a), sa = Math.sin(a);
    const px = -sa, py = ca; // unit perpendicular (left of branch dir)

    for (let i = 0; i < LEDS_PER_BRANCH; i++) {
      const t = tSeq[i];
      const baseR = t * radius;
      const baseX = baseR * ca;
      const baseY = baseR * sa;
      const lateral = lSeq[i] * radius;
      const x = baseX + lateral * px;
      const y = baseY + lateral * py;
      positions.push({x, y});

      if (lateral !== 0) {
        connectors.push({from: {x: baseX, y: baseY}, to: {x, y}});
      }
    }

    // Tip line for main stem (use last t in sequence)
    const tipR = tSeq[tSeq.length - 1] * radius;
    branchTips.push({x: tipR * ca, y: tipR * sa});
  }

  return {positions, connectors, branchTips};
}

function drawLayoutAt(layout, values, cx, cy, scale=1) {
  ctx.save();
  ctx.translate(cx, cy);
  ctx.scale(scale, scale);

  // Draw main stems
  ctx.strokeStyle = 'rgba(160,160,160,0.25)';
  ctx.lineWidth = 2;
  ctx.beginPath();
  for (const tip of layout.branchTips) { ctx.moveTo(0,0); ctx.lineTo(tip.x, tip.y); }
  ctx.stroke();

  // Draw side twig connectors
  if (layout.connectors.length) {
    ctx.beginPath();
    for (const seg of layout.connectors) { ctx.moveTo(seg.from.x, seg.from.y); ctx.lineTo(seg.to.x, seg.to.y); }
    ctx.stroke();
  }

  // Draw LED spheres
  const R = 14; // pixel radius
  for (let i = 0; i < layout.positions.length; i++) {
    const p = layout.positions[i];
    const v = values ? values[i] : 0;
    ctx.fillStyle = toPurple(v);
    ctx.beginPath();
    ctx.arc(p.x, p.y, R, 0, Math.PI*2);
    ctx.fill();
  }

  ctx.restore();
}

// Compute extents (max |x|, max |y|) including LED radius
function computeExtents(layout) {
  const R = 14;
  let maxX = 0, maxY = 0;
  for (const p of layout.positions) {
    if (Math.abs(p.x) + R > maxX) maxX = Math.abs(p.x) + R;
    if (Math.abs(p.y) + R > maxY) maxY = Math.abs(p.y) + R;
  }
  for (const tip of layout.branchTips) {
    if (Math.abs(tip.x) + R > maxX) maxX = Math.abs(tip.x) + R;
    if (Math.abs(tip.y) + R > maxY) maxY = Math.abs(tip.y) + R;
  }
  return {maxX, maxY};
}

let BRANCHES = 4, LEDS_PER_BRANCH = 7, TOTAL = 28;
let layoutLeft = computeLayout(BRANCHES, LEDS_PER_BRANCH);
let layoutRight = computeLayout(BRANCHES, LEDS_PER_BRANCH);
let valuesA = new Float32Array(TOTAL);
let valuesB = new Float32Array(TOTAL);

function step(timeMs) {
  if (!Module) return requestAnimationFrame(step);
  if (!paused) {
  const t = timeMs / 1000; // synced time for both

  // Left tree
  let a=0,b=0,c=0,d=0;
  const animIdA = Number(animA.value);
  if (animIdA === 0) { a = Number(levelA.value); }
  else if (animIdA === 1) { a = Number(speedGlobal.value); b = Number(phaseGlobal.value); c = branchA.checked ? 1 : 0; d = invertA.checked ? 1 : 0; }
  else if (animIdA === 2) { a = Number(speedGlobal.value); b = Number(phaseGlobal.value); c = branchA.checked ? 1 : 0; }
  else if (animIdA === 3) { a = Number(speedGlobal.value); b = Number(widthA.value); c = branchA.checked ? 1 : 0; }
  Module._anim_eval2(animIdA, t, a, b, c, d);
  for (let i=0;i<TOTAL;i++) valuesA[i] = Module._anim_value_at(i);

  // Right tree
  a=b=c=d=0;
  const animIdB = Number(animB.value);
  if (animIdB === 0) { a = Number(levelB.value); }
  else if (animIdB === 1) { a = Number(speedGlobal.value); b = Number(phaseGlobal.value); c = branchB.checked ? 1 : 0; d = invertB.checked ? 1 : 0; }
  else if (animIdB === 2) { a = Number(speedGlobal.value); b = Number(phaseGlobal.value); c = branchB.checked ? 1 : 0; }
  else if (animIdB === 3) { a = Number(speedGlobal.value); b = Number(widthB.value); c = branchB.checked ? 1 : 0; }
  Module._anim_eval2(animIdB, t, a, b, c, d);
  for (let i=0;i<TOTAL;i++) valuesB[i] = Module._anim_value_at(i);

  // draw side-by-side
  ctx.clearRect(0,0,canvas.width,canvas.height);
    const cw = canvas.clientWidth, ch = canvas.clientHeight;
    const cx = cw/2, cy = ch/2;
    const {maxX, maxY} = computeExtents(layoutLeft);
    const edge = 24, between = 80; // margins
    const desiredDx = maxX + between; // center to center spacing target
    const maxDxAllowed = Math.max(20, cw/2 - maxX - edge);
    let scale = 1.0;
    if (desiredDx > maxDxAllowed) scale = maxDxAllowed / desiredDx;
    // Constrain by vertical space as well
    if (maxY > 0) {
      const maxScaleY = Math.max(0.2, (ch/2 - edge) / maxY);
      scale = Math.min(scale, maxScaleY);
    }
    scale = Math.max(0.2, Math.min(scale, 1.0));
    const dx = desiredDx * scale;

    drawLayoutAt(layoutLeft, valuesA, cx - dx, cy, scale);
    drawLayoutAt(layoutRight, valuesB, cx + dx, cy, scale);
  }
  requestAnimationFrame(step);
}

async function boot() {
  resizeCanvas();
  Module = await createModule();
  // Query constants
  BRANCHES = Module._anim_branches();
  LEDS_PER_BRANCH = Module._anim_leds_per_branch();
  TOTAL = Module._anim_total_leds();
  layoutLeft = computeLayout(BRANCHES, LEDS_PER_BRANCH);
  layoutRight = computeLayout(BRANCHES, LEDS_PER_BRANCH);
  valuesA = new Float32Array(TOTAL);
  valuesB = new Float32Array(TOTAL);

  requestAnimationFrame(step);
}

boot();
