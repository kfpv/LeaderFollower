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

// UI elements
const animSel = document.getElementById('anim');
const speedEl = document.getElementById('speed');
const phaseEl = document.getElementById('phase');
const branchEl = document.getElementById('branch');
const widthEl = document.getElementById('width');
const levelEl = document.getElementById('level');
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

function drawLayout(layout, values) {
  // Clear
  ctx.clearRect(0, 0, canvas.width, canvas.height);

  // Translate origin to center
  ctx.save();
  ctx.translate(canvas.clientWidth/2, canvas.clientHeight/2);

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

let BRANCHES = 4, LEDS_PER_BRANCH = 7, TOTAL = 28;
let layout = computeLayout(BRANCHES, LEDS_PER_BRANCH);
// No direct heap usage anymore; we’ll call anim_eval2 and sample values via anim_value_at

function step(timeMs) {
  if (!Module) return requestAnimationFrame(step);
  if (!paused) {
    const t = timeMs / 1000;
    const animId = Number(animSel.value);

    // Params mapping per wasm_shim
    let a=0,b=0,c=0,d=0;
    if (animId === 0) { a = Number(levelEl.value); }
    else if (animId === 1 || animId === 2) {
      a = Number(speedEl.value);
      b = Number(phaseEl.value);
      c = branchEl.checked ? 1 : 0;
    } else if (animId === 3) {
      a = Number(speedEl.value);
      b = Number(widthEl.value);
      c = branchEl.checked ? 1 : 0;
    }

  Module._anim_eval2(animId, t, a, b, c, d);
  // Pull values on demand to a JS array for drawing
  const values = tmpValues;
  for (let i = 0; i < TOTAL; i++) values[i] = Module._anim_value_at(i);
  drawLayout(layout, values);
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
  layout = computeLayout(BRANCHES, LEDS_PER_BRANCH);

  // Prepare a local JS buffer for drawing
  tmpValues = new Float32Array(TOTAL);

  requestAnimationFrame(step);
}

let tmpValues = new Float32Array(TOTAL);
boot();
