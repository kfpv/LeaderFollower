// Minimal JS demo that implements the Pixelblaze-derived perlinRidge and render loop
const canvas = document.getElementById('c');
const ctx = canvas.getContext('2d');
let w=800,h=400;
function resize(){ canvas.width = Math.max(200, window.innerWidth-0); canvas.height = Math.max(200, window.innerHeight-60); }
window.addEventListener('resize', ()=>{ resize(); renderFrame(currentT); });
resize();

// UI
const octSlider = document.getElementById('oct');
const offsetSlider = document.getElementById('offset');
const info = document.getElementById('info');
const coordCard = document.getElementById('coordCard');
const resetBtn = document.getElementById('reset');
const ledOverlayChk = document.getElementById('ledOverlay');
const pauseChk = document.getElementById('pause');
const copyBtn = document.getElementById('copyPos');
const pasteBtn = document.getElementById('pastePos');
const mapOffChk = document.getElementById('mapOff');
const patternSelector = document.getElementById('patternSelector');
const branchCard = document.getElementById('branchCard');
const branchHeader = document.getElementById('branchHeader');
const branchBody = document.getElementById('branchBody');
const branchContainer = document.getElementById('branchContainer');
const resetMapBtn = document.getElementById('resetMap');
const applyDefaultsBtn = document.getElementById('applyDefaults');
const speedInput = document.getElementById('speedInput');
const copyBranchesBtn = document.getElementById('copyBranches');
const pasteBranchesBtn = document.getElementById('pasteBranches');
const syncBranchesBtn = document.getElementById('syncBranches');
const calibrateEnable = document.getElementById('calibrateEnable');
const autoCalBtn = document.getElementById('autoCalibrate');
let syncBranches = false;
let syncFirst = 0.0; // scalar distance for first LED on each branch
let syncDelta = 0.05; // scalar incremental distance per subsequent LED

// --- Global Calibration State ---
let calibrationGlobal = { lo: 0.2, hi: 0.8 };
function applyCalibration(v){
  if (!calibrateEnable.checked) return v;
  const { lo, hi } = calibrationGlobal;
  if (hi <= lo) return v;
  if (v <= lo) return 0;
  if (v >= hi) return 1;
  return (v - lo)/(hi - lo);
}
function buildCalibrationUI(parent){
  if (!calibrateEnable.checked) return;
  const wrap = document.createElement('div');
  wrap.style.cssText='margin-top:8px;padding:6px;border:1px solid #333;border-radius:4px;background:#111;display:flex;flex-direction:column;gap:6px;';
  const row = document.createElement('div');
  row.style.cssText='display:flex;flex-wrap:wrap;gap:8px;align-items:center;font-size:12px;';
  row.innerHTML = `<strong style="font-size:12px">Calibration</strong>
    <label style='display:flex;align-items:center;gap:4px;'>min<input id='calLo' type='number' step='0.001' value='${calibrationGlobal.lo.toFixed(3)}' style='width:70px;'></label>
    <label style='display:flex;align-items:center;gap:4px;'>max<input id='calHi' type='number' step='0.001' value='${calibrationGlobal.hi.toFixed(3)}' style='width:70px;'></label>
    <button id='autoCalLocal' style='flex:0 0 auto;'>Auto-Cal (10s)</button>
    <span style='opacity:.55;font-size:10px'>Maps min->0 max->1 (clamped)</span>`;
  wrap.appendChild(row);
  parent.appendChild(wrap);
  const loEl = row.querySelector('#calLo');
  const hiEl = row.querySelector('#calHi');
  function upd(){ const l=parseFloat(loEl.value); const h=parseFloat(hiEl.value); if(!isNaN(l)) calibrationGlobal.lo=l; if(!isNaN(h)) calibrationGlobal.hi=h; renderFrame(currentT); }
  loEl.addEventListener('input', upd); hiEl.addEventListener('input', upd);
  row.querySelector('#autoCalLocal').addEventListener('click', ()=> runAutoCalibration());
}
function runAutoCalibration(){
  if (!calibrateEnable.checked) return;
  const frames=300; // 10s @30fps
  // Build LED positions matching overlay
  let ledPositions=[];
  if (branchModeActive){ ledPositions = branchLEDPositions(); }
  else {
    const ledCount=7; const cx=canvas.width*0.5; const cy=canvas.height*0.5; const spacing=canvas.width*0.04;
    for(let i=0;i<ledCount;i++){ const sx=cx+(i+1)*spacing; const sy=cy; const m=screenToModel(sx,sy); ledPositions.push({x:m.x,y:m.y}); }
  }
  const allSamples=[]; const speed=parseFloat(speedInput.value); const baseNow=Date.now()/1000.0; const oct=parseInt(octSlider.value); const offset=parseFloat(offsetSlider.value);
  const ampSum = 0.5 * (1 - Math.pow(0.75, oct)) / (1 - 0.75);
  const maxRidge = offset*offset * ampSum; const normScale = 1.0 / Math.max(1e-6, maxRidge);
  for(let f=0; f<frames; f++){
    const simT=(baseNow + f/30 - startTime)*speed;
    for (let li=0; li<ledPositions.length; li++){
      const m=ledPositions[li];
      let v=0;
      if (patternSelector && patternSelector.value==='swirl'){
        v = swirlPattern(simT, m.x+0.5, m.y+0.5);
      } else {
        let p=perlinRidge(m.x*3+0.5, m.y*3+0.5, 0, oct, 1.3, 0.75, offset);
        p*=normScale; p=p*p; if(p<=0.1) v=0; else if(p>=1) v=1; else v=(p-0.1)/(0.9); v=v*v;
      }
      allSamples.push(v);
    }
  }
  allSamples.sort((a,b)=>a-b);
  if (allSamples.length){
    const loIdx=Math.floor(allSamples.length*0.05); const hiIdx=Math.floor(allSamples.length*0.95);
    calibrationGlobal.lo = allSamples[loIdx];
    calibrationGlobal.hi = allSamples[hiIdx];
  }
  info.textContent='Global calibration updated';
  buildBranchUI();
  renderFrame(currentT);
}
calibrateEnable.addEventListener('change', ()=>{ buildBranchUI(); renderFrame(currentT); });

// Branch mode state
let branchModeActive = false;
const BRANCH_COUNT = 4;
const LEDS_PER_BRANCH = 7;
// Each branch: { first:{x,y}, delta:{x,y} }
let branches = [];
function initBranches(){
  branches = [
    { name:'Right',  first:{x:0.0, y:0.0}, delta:{x:0.05, y:0.0} },
    { name:'Left',   first:{x:0.0, y:0.0}, delta:{x:-0.05,y:0.0} },
    { name:'Up',     first:{x:0.0, y:0.0}, delta:{x:0.0, y:-0.05} },
    { name:'Down',   first:{x:0.0, y:0.0}, delta:{x:0.0, y:0.05} }
  ];
}
initBranches();

function buildBranchUI(){
  branchContainer.innerHTML = '';
  if(!syncBranches){
    branches.forEach((b, idx)=>{
      const wrap = document.createElement('div');
      wrap.className = 'branchRow';
      wrap.innerHTML = `<header><span>${b.name}</span><span style="opacity:.6">#${idx}</span></header>`;
      const inputs = document.createElement('div'); inputs.className='branchInputs';
      function addField(label, key, obj){
        const id = `b${idx}_${key}`;
        const div = document.createElement('label');
        div.innerHTML = `<span style='width:62px'>${label}</span><input id='${id}x' type='number' step='0.001' value='${obj.x}'><input id='${id}y' type='number' step='0.001' value='${obj.y}'>`;
        inputs.appendChild(div);
        const xEl = div.querySelector(`#${id}x`); const yEl = div.querySelector(`#${id}y`);
        function upd(){ obj.x = parseFloat(xEl.value)||0; obj.y = parseFloat(yEl.value)||0; renderFrame(currentT); }
        xEl.addEventListener('input', upd); yEl.addEventListener('input', upd);
      }
      addField('first', 'first', b.first);
      addField('delta', 'delta', b.delta);
      wrap.appendChild(inputs);
      branchContainer.appendChild(wrap);
    });
  } else {
    // Sync mode: only two scalar inputs (first, delta). Branch directions are fixed.
    const wrap = document.createElement('div'); wrap.className='branchRow';
    wrap.innerHTML = `<header><span>Synced Branches</span><span style='opacity:.6'>axes</span></header>`;
    const inputs = document.createElement('div'); inputs.className='branchInputs';
    inputs.innerHTML = `
      <label><span style='width:62px'>first</span><input id='syncFirst' type='number' step='0.001' value='${syncFirst}'></label>
      <label><span style='width:62px'>delta</span><input id='syncDelta' type='number' step='0.001' value='${syncDelta}'></label>
      <div class='tinyNote'>Positions: ±(first + delta*n) along X (Right/Left) and Y (Up/Down).</div>`;
    wrap.appendChild(inputs); branchContainer.appendChild(wrap);
    const fEl = inputs.querySelector('#syncFirst');
    const dEl = inputs.querySelector('#syncDelta');
    function upd(){ syncFirst = parseFloat(fEl.value)||0; syncDelta = parseFloat(dEl.value)||0; renderFrame(currentT); }
    fEl.addEventListener('input', upd); dEl.addEventListener('input', upd);
  }
  // Append calibration controls if enabled
  buildCalibrationUI(branchContainer);
}

buildBranchUI();

branchHeader.addEventListener('click', ()=>{
  branchModeActive = !branchModeActive;
  if (branchModeActive){
    branchCard.classList.remove('collapsed'); branchCard.classList.add('expanded','active');
    branchBody.style.display='flex';
    branchCard.querySelector('.modeIndicator').textContent = 'ON';
  } else {
    branchCard.classList.remove('expanded','active'); branchCard.classList.add('collapsed');
    branchBody.style.display='none';
    branchCard.querySelector('.modeIndicator').textContent = 'OFF';
  }
  renderFrame(currentT);
});

applyDefaultsBtn.addEventListener('click', ()=>{ initBranches(); buildBranchUI(); renderFrame(currentT); });
resetMapBtn.addEventListener('click', ()=>{ panX=0; panY=0; zoom=1; renderFrame(currentT); });

// Helper: generate LED positions for branch mode
function branchLEDPositions(){
  if (syncBranches){
    const out = [];
    for (let i=0;i<LEDS_PER_BRANCH;i++){
      const offs = syncFirst + syncDelta * i;
      // Right (+x)
      out.push({ x: +offs, y: 0 });
      // Left (-x)
      out.push({ x: -offs, y: 0 });
      // Up (+y)
      out.push({ x: 0, y: +offs });
      // Down (-y)
      out.push({ x: 0, y: -offs });
    }
    return out;
  } else {
    const out = [];
    branches.forEach(b=>{
      for(let i=0;i<LEDS_PER_BRANCH;i++){
        out.push({ x: b.first.x + b.delta.x * i, y: b.first.y + b.delta.y * i });
      }
    });
    return out;
  }
}

// View transform
let zoom = 1.0; let panX=0, panY=0; let dragging=false, lastX=0, lastY=0;
canvas.addEventListener('wheel', (e)=>{ e.preventDefault(); let s = Math.exp(-e.deltaY*0.001); zoom *= s; const tnow = currentT; renderFrame(tnow); });
canvas.addEventListener('mousedown',(e)=>{ dragging=true; lastX=e.clientX; lastY=e.clientY; });
window.addEventListener('mouseup',()=>{ dragging=false; });
window.addEventListener('mousemove',(e)=>{ if(dragging){ panX += (e.clientX-lastX)/zoom; panY += (e.clientY-lastY)/zoom; lastX=e.clientX; lastY=e.clientY; const tnow = currentT; renderFrame(tnow); } const r = screenToModel(e.offsetX,e.offsetY); coordCard.textContent = `model: ${r.x.toFixed(4)}, ${r.y.toFixed(4)}`; });
resetBtn.addEventListener('click', ()=>{ zoom=1; panX=0; panY=0; /* re-render current frame */ renderFrame(currentT); });
copyBtn.addEventListener('click', async ()=>{
  // copy only position-related state (no branches, no sync, no calibration)
  const state = {
    panX,
    panY,
    zoom,
    t: currentT,
    speed: parseFloat(speedInput.value),
    oct: parseInt(octSlider.value),
    offset: parseFloat(offsetSlider.value),
    version: 4
  };
  try { await navigator.clipboard.writeText(JSON.stringify(state)); info.textContent = 'Position copied'; } catch(e){ info.textContent = 'Copy failed'; }
});
pasteBtn.addEventListener('click', async ()=>{
  try {
    const txt = await navigator.clipboard.readText();
    const s = JSON.parse(txt);
    if (s && typeof s === 'object'){
      // restore only position-related state
      panX = 'panX' in s ? s.panX : panX;
      panY = 'panY' in s ? s.panY : panY;
      zoom = 'zoom' in s ? s.zoom : zoom;
      currentT = 't' in s ? s.t : currentT;
      const sp = ('speed' in s) ? s.speed : parseFloat(speedInput.value);
      speedInput.value = sp.toString();
      if ('offset' in s) offsetSlider.value = parseFloat(s.offset).toString();
      if ('oct' in s) octSlider.value = parseInt(s.oct).toString();
      // do not modify branches, sync state, pattern or calibration
      const now = Date.now()/1000.0;
      if (pauseChk.checked) { pausedTime = currentT; } else { startTime = now - currentT / Math.max(0.0001, sp); }
      renderFrame(currentT);
      info.textContent = 'Pasted position';
    }
  } catch(e){ info.textContent = 'Paste failed'; }
});

// Speed number input sync
speedInput.addEventListener('input', ()=>{
  const v = parseFloat(speedInput.value) || 0;
  const s = Math.max(0.0000001, v); // allow extremely low speeds
  const now = Date.now()/1000.0; const curT = currentT;
  if (!pauseChk.checked) startTime = now - curT / s;
});

// Branch copy/paste buttons
copyBranchesBtn.addEventListener('click', async ()=>{
  try { await navigator.clipboard.writeText(JSON.stringify({ branches, tag:'branches' })); info.textContent='Branches copied'; } catch(e){ info.textContent='Copy fail'; }
});

pasteBranchesBtn.addEventListener('click', async ()=>{
  try { const txt = await navigator.clipboard.readText(); const obj = JSON.parse(txt); if (obj && Array.isArray(obj.branches)){ branches = obj.branches; buildBranchUI(); renderFrame(currentT); info.textContent='Branches pasted'; } else { info.textContent='No branches found'; } } catch(e){ info.textContent='Paste fail'; }
});

// Sync mode: editing first/delta of first branch propagates to others with sign adjustments
syncBranchesBtn.addEventListener('click', ()=>{
  syncBranches = !syncBranches; syncBranchesBtn.textContent = syncBranches ? 'Sync On' : 'Sync Off';
  buildBranchUI();
  renderFrame(currentT);
});

// (Removed legacy override-based sync handling; integrated directly in buildBranchUI)

// Hash and noise (port of C++ helpers)
function hash(x,y,z){ let h = (x*374761393 + y*668265263) ^ (z*362437); h = (h ^ (h>>>13))*1274126177; h = h ^ (h>>>16); return h >>> 0; }
function noise(x,y,z){ return (hash(x|0,y|0,z|0) & 0xFFFFFF) / 16777216.0; }
function fade(t){ return t*t*(3-2*t); }
function lerp(a,b,t){ return a + (b-a)*t; }
function valueNoise(x,y,z){ let X=Math.floor(x),Y=Math.floor(y),Z=Math.floor(z); let fx=x-X, fy=y-Y, fz=z-Z; let u=fade(fx), v=fade(fy), w=fade(fz);
  let n000=noise(X,Y,Z), n100=noise(X+1,Y,Z); let n010=noise(X,Y+1,Z), n110=noise(X+1,Y+1,Z);
  let n001=noise(X,Y,Z+1), n101=noise(X+1,Y,Z+1); let n011=noise(X,Y+1,Z+1), n111=noise(X+1,Y+1,Z+1);
  let nx00=lerp(n000,n100,u), nx10=lerp(n010,n110,u); let nx01=lerp(n001,n101,u), nx11=lerp(n011,n111,u);
  let nxy0=lerp(nx00,nx10,v), nxy1=lerp(nx01,nx11,v); return lerp(nxy0,nxy1,w);
}
function perlinRidge(x,y,z,octaves,lacunarity,gain,offset){ let sum=0; let freq=1; let amp=0.5; for(let i=0;i<octaves;i++){ let v=valueNoise(x*freq,y*freq,z*freq); v = v*2-1; v = offset - Math.abs(v); v = v*v; sum += v*amp; freq *= lacunarity; amp *= gain; } return sum; }

// Convert screen coords -> model coords used by shader
function screenToModel(sx,sy){ let mx = (sx - canvas.width*0.5)/Math.min(canvas.width,canvas.height) / zoom - panX/100; let my = (sy - canvas.height*0.5)/Math.min(canvas.width,canvas.height) / zoom - panY/100; return {x: mx, y: my}; }

// Rendering function that draws using a supplied time t
// Helpers replicating Pixelblaze utilities
function triangle(x){ const f = x - Math.floor(x); return Math.abs(f*2 - 1); }
function wave(x){ return 0.5 + 0.5 * Math.sin(x * Math.PI * 2); }
function timef(t, freq){ return (t * freq) % 1.0; }

function renderFrame(t){ const speed = parseFloat(speedInput.value); const oct = parseInt(octSlider.value); const offset = parseFloat(offsetSlider.value);
  ctx.clearRect(0,0,canvas.width,canvas.height);
  const img = ctx.createImageData(canvas.width, canvas.height);
  // precompute normalization scale for current params
  const ampSum = 0.5 * (1 - Math.pow(0.75, oct)) / (1 - 0.75);
  const maxRidge = offset*offset * ampSum; const scale = 1.0 / Math.max(1e-6, maxRidge);
  // Animated transform parameters (Pixelblaze style)
  const tScale = 0.15 + 0.15 * triangle(timef(t, 1.0));
  const rz = wave(timef(t, 0.416)) * Math.PI * 2; // rotate Z
  const ry = wave(timef(t, 0.515)) * Math.PI * 2; // rotate Y
  const rx = wave(timef(t, 0.359)) * Math.PI * 2; // rotate X
  const cosZ = Math.cos(rz), sinZ = Math.sin(rz);
  const cosY = Math.cos(ry), sinY = Math.sin(ry);
  const cosX = Math.cos(rx), sinX = Math.sin(rx);

  const spatialFreq = 3.0; // global spatial frequency for pattern (accessible to overlay)
  if (!mapOffChk.checked) {
    for(let y=0;y<canvas.height;y++){
      for(let x=0;x<canvas.width;x++){
        const m = screenToModel(x,y);
        // Base centered coords already ~[-0.5,0.5] scale them then apply rotations
        let col = 0;
        if (patternSelector && patternSelector.value === 'swirl') {
          // swirlPattern expects normalized [0..1] coordinates with center at 0.5
          const mx = m.x + 0.5;
          const my = m.y + 0.5;
          const v = swirlPattern(t, mx, my);
          col = Math.round(255 * Math.max(0, Math.min(1, v)));
        } else {
          let x0 = m.x; let y0 = m.y; let z0 = 0.0;
          // Scale around origin
          x0 *= tScale; y0 *= tScale; z0 *= tScale;
          // Rotate Z
          let x1 = x0 * cosZ - y0 * sinZ;
          let y1 = x0 * sinZ + y0 * cosZ;
          let z1 = z0;
          // Rotate Y
          let x2 = x1 * cosY + z1 * sinY;
          let y2 = y1;
          let z2 = -x1 * sinY + z1 * cosY;
          // Rotate X
          let y3 = y2 * cosX - z2 * sinX;
          let z3 = y2 * sinX + z2 * cosX;
          let x3 = x2;
          // Sample ridge at transformed coords (optionally scale space for more detail)
          let p = perlinRidge(x3 * spatialFreq + 0.5, y3 * spatialFreq + 0.5, z3 * spatialFreq, oct, 1.3, 0.75, offset);
          p *= scale; p = p*p;
          // smoothstep(0.1,1.0,p) approx
          let v = 0.0;
          if (p <= 0.1) v = 0.0; else if (p >= 1.0) v = 1.0; else v = (p - 0.1) / (1.0 - 0.1);
          v = v*v;
          col = Math.round(255 * Math.min(1, Math.max(0, v)));
        }
        const idx = (y*canvas.width + x)*4; img.data[idx]=col; img.data[idx+1]=col; img.data[idx+2]=col; img.data[idx+3]=255;
      }
    }
    ctx.putImageData(img,0,0);
  } else {
    // Map Off: fill canvas black
    ctx.fillStyle = '#000';
    ctx.fillRect(0,0,canvas.width,canvas.height);
  }

  // LED overlay: standard row or branch mode
  if (ledOverlayChk.checked) {
    ctx.lineWidth = 1;
    ctx.textAlign = 'center';
    ctx.textBaseline = 'top';
    ctx.font = '12px system-ui, Arial';
    let leds = [];
    if (branchModeActive) {
      leds = branchLEDPositions(); // model-space positions
    } else {
      // default 7 to the right of center (screen anchored)
      const ledCount = 7;
      const cx = canvas.width * 0.5;
      const cy = canvas.height * 0.5;
      const spacing = canvas.width * 0.04;
      for (let i=0;i<ledCount;i++) {
        const sx = cx + (i+1) * spacing;
        const sy = cy;
        const m = screenToModel(sx, sy);
        leds.push({ x: m.x, y: m.y });
      }
    }
    leds.forEach((pos, idx)=>{
      const m = pos; // model coords
      // transform to screen (inverse of screenToModel approx) for drawing when in branch mode
      let sx, sy;
      if (branchModeActive) {
        const scale = Math.min(canvas.width, canvas.height);
        sx = (m.x + panX/100) * zoom * scale + canvas.width*0.5;
        sy = (m.y + panY/100) * zoom * scale + canvas.height*0.5;
      } else {
        // original already in screenToModel inverse via earlier path
        const scale = Math.min(canvas.width, canvas.height);
        sx = (m.x + panX/100) * zoom * scale + canvas.width*0.5;
        sy = (m.y + panY/100) * zoom * scale + canvas.height*0.5;
      }
      // sample brightness
      let vLED = 0.0;
      if (patternSelector && patternSelector.value === 'swirl') {
        const mx = m.x + 0.5;
        const my = m.y + 0.5;
        vLED = swirlPattern(t, mx, my);
      } else {
        let x0 = m.x, y0 = m.y, z0 = 0.0;
        x0 *= tScale; y0 *= tScale; z0 *= tScale;
        let x1 = x0 * cosZ - y0 * sinZ;
        let y1 = x0 * sinZ + y0 * cosZ; let z1 = z0;
        let x2 = x1 * cosY + z1 * sinY; let y2 = y1; let z2 = -x1 * sinY + z1 * cosY;
        let y3 = y2 * cosX - z2 * sinX; let z3 = y2 * sinX + z2 * cosX; let x3 = x2;
        let pLED = perlinRidge(x3 * spatialFreq + 0.5, y3 * spatialFreq + 0.5, z3 * spatialFreq, oct, 1.3, 0.75, offset);
        pLED *= scale; pLED = pLED * pLED;
        vLED = 0.0; if (pLED <= 0.1) vLED = 0.0; else if (pLED >= 1.0) vLED = 1.0; else vLED = (pLED - 0.1)/(1.0-0.1); vLED = vLED * vLED;
      }
  // Apply calibration remap (per LED)
  vLED = applyCalibration(vLED);
      const coreR = 10;
      const glowR = 28;
      const grad = ctx.createRadialGradient(sx, sy, coreR*0.2, sx, sy, glowR);
      grad.addColorStop(0.0, `rgba(255,255,255,${Math.min(1.0, vLED*1.2)})`);
      grad.addColorStop(0.4, `rgba(255,255,255,${Math.min(0.8, vLED*0.6)})`);
      grad.addColorStop(1.0, 'rgba(0,0,0,0)');
      ctx.beginPath(); ctx.arc(sx, sy, glowR, 0, Math.PI*2); ctx.fillStyle = grad; ctx.fill();
      ctx.beginPath(); ctx.arc(sx, sy, coreR, 0, Math.PI*2);
      ctx.fillStyle = `rgba(255,255,255,${Math.min(1.0, Math.max(0.05, vLED))})`;
      ctx.fill();
      ctx.strokeStyle = `rgba(255,255,255,${Math.min(0.9, Math.max(0.15, vLED))})`;
      ctx.stroke();
  // Keep LED brightness label always white for visibility
  ctx.fillStyle = '#fff';
      ctx.fillText(vLED.toFixed(2), sx, sy + coreR + 4);
    });
  }
}

// Animation state
let startTime = Date.now()/1000.0; // wall clock seconds so time advances over system sleep
let pausedTime = 0;
let currentT = 0;

// changing speed should keep the currently visible t intact
// speedSlider removed -> handled by speedInput only

// pause toggle handler
pauseChk.addEventListener('change', ()=>{
  const now = Date.now()/1000.0;
  const s = parseFloat(speedInput.value);
  if (pauseChk.checked) {
    pausedTime = currentT;
  } else {
    startTime = now - pausedTime / Math.max(0.0001, s);
  }
});

// continuously render using requestAnimationFrame
function animate(){
  requestAnimationFrame(animate);
  const now = Date.now()/1000.0;
  const speed = parseFloat(speedInput.value);
  // compute the current time value; if paused, keep frozen time
  let t;
  if (pauseChk.checked) {
    t = pausedTime;
  } else {
    t = (now - startTime) * speed;
  }
  currentT = t;
  renderFrame(t);
}
animate();

// click -> show model coords & brightness in info (no popup)
canvas.addEventListener('click',(e)=>{
  const r = screenToModel(e.offsetX,e.offsetY);
  // compute brightness at this point using current parameters
  const t = currentT; const oct = parseInt(octSlider.value); const offset = parseFloat(offsetSlider.value);
  if (patternSelector && patternSelector.value === 'swirl') {
    const mx = r.x + 0.5; const my = r.y + 0.5;
    const v = swirlPattern(t, mx, my);
    info.textContent = `model: ${r.x.toFixed(4)},${r.y.toFixed(4)}  brightness=${v.toFixed(3)}`;
  } else {
    let nx = r.x*2 + 0.5; let ny = r.y*2 + 0.5; let nz = 0.0;
    const ampSum = 0.5 * (1 - Math.pow(0.75, oct)) / (1 - 0.75);
    const maxRidge = offset*offset * ampSum; const scale = 1.0 / Math.max(1e-6, maxRidge);
    let p = perlinRidge(nx, ny, nz, oct, 1.3, 0.75, offset);
    p *= scale; p = p*p; let v = 0.0; if (p<=0.1) v=0.0; else if (p>=1.0) v=1.0; else v=(p-0.1)/(1.0-0.1); v = v*v;
    info.textContent = `model: ${r.x.toFixed(4)},${r.y.toFixed(4)}  brightness=${v.toFixed(3)}`;
  }
});

// Swirl pattern port (grayscale-only)
function swirlPattern(t, mx, my) {
  // parameters (mapped from Pixelblaze):
  const twistSpeed = 0.015;
  const rotateSpeed = 0.002;
  const startingColor = 0.3; // unused for grayscale
  const colorSpeed = 0.015; // unused for grayscale
  const arms = 3; // default symmetry

  // helper functions
  function wave(x){ return 0.5 + 0.5 * Math.sin(x * Math.PI * 2); }
  function timef(t, freq){ return (t * freq) % 1.0; }
  function arctan2(y,x){ if (x > 0) return Math.atan(y/x); if (y > 0) return Math.PI/2 - Math.atan(x/y); if (y < 0) return -Math.PI/2 - Math.atan(x/y); if (x < 0) return Math.PI + Math.atan(y/x); return 1.0; }
  function floor(v){ return Math.floor(v); }
  function hsvToGray(h,s,v){ // ignore color, return grayscale brightness in [0,1]
    return v; // approximate: use v directly
  }

  // compute params
  const twist = wave(timef(t, twistSpeed)) * 2 - 1;
  const rotation = timef(t, rotateSpeed);
  const colorShift = timef(t, colorSpeed);

  // normalize coordinates to [-1,1]
  const xNorm = (mx - 0.5) * 2;
  const yNorm = (my - 0.5) * 2;
  const dist = Math.sqrt(xNorm*xNorm + yNorm*yNorm);
  let angle = (arctan2(yNorm, xNorm) + Math.PI) / Math.PI / 2;
  const tval = twist < 0 ? dist * twist : dist * twist;
  angle += tval / 2;

  // setColor -> compute h,s,v then return grayscale v
  let h = angle * arms - rotation + 10;
  h = h - floor(h);
  let s = 1.0;
  let v = (1.01 - dist) * (h < 0.5 ? h*h*h : h);
  // map v into [0,1]
  v = Math.max(0, Math.min(1, v));
  return v;
}

