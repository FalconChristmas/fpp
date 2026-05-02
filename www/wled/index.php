<?php
// FPP Overlay UI — WLED-styled, mobile-first.
// Three tabs:
//   Color   — iro.js wheel + 3 color slots (FG/BG/Tertiary) + palette picker
//   Effects — searchable effect list + per-effect dynamic argument form
//   Info    — device + running effects
//
// All effect invocation goes through POST /api/command with the
// `Overlay Model Effect` command, args ordered to match the per-effect
// CommandArg list returned by /api/overlays/effects?full=true.
?>
<!DOCTYPE html>
<html lang="en">
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, minimum-scale=1, viewport-fit=cover">
<meta charset="utf-8">
<meta name="theme-color" content="#222222">
<meta name="apple-mobile-web-app-capable" content="yes">
<title>FPP Overlay</title>
<style>
:root{
  --c-1:#111;--c-2:#222;--c-3:#333;--c-4:#444;--c-5:#555;--c-6:#666;--c-8:#888;--c-b:#bbb;--c-d:#ddd;--c-f:#fff;
  --c-acc:#48a;--c-ok:#2c1;--c-warn:#a90;--c-err:#c32;
  --th:64px;--bh:60px;
}
*{box-sizing:border-box;}
html,body{margin:0;padding:0;height:100%;background:var(--c-1);color:var(--c-f);
  font-family:Helvetica,Verdana,sans-serif;font-size:17px;-webkit-tap-highlight-color:transparent;
  -webkit-user-select:none;user-select:none;}
body{overscroll-behavior:none;}
a,a:visited{color:var(--c-acc);text-decoration:none;}

.tab.top{position:fixed;top:0;left:0;right:0;height:var(--th);background:var(--c-2);
  display:flex;align-items:center;justify-content:space-between;padding:0 12px;z-index:10;
  border-bottom:1px solid var(--c-3);}
.tab.bot{position:fixed;bottom:0;left:0;right:0;height:var(--bh);background:var(--c-2);
  display:flex;border-top:1px solid var(--c-3);z-index:10;}
.tab.bot button{flex:1;background:transparent;border:0;color:var(--c-d);font-size:15px;padding:6px 0;}
.tab.bot button.active{color:var(--c-acc);border-top:2px solid var(--c-acc);}

.container{padding:calc(var(--th) + 8px) 14px calc(var(--bh) + 16px);max-width:560px;margin:0 auto;}
.tabcontent{display:none;}
.tabcontent.active{display:block;}

.btn{background:var(--c-3);border:1px solid var(--c-4);color:var(--c-f);padding:8px 14px;border-radius:6px;
  font-size:15px;cursor:pointer;transition:background .15s;}
.btn:hover{background:var(--c-4);}
.btn.primary{background:var(--c-acc);border-color:var(--c-acc);}
.btn.primary:hover{background:#5b9cdf;}
.btn.danger{background:var(--c-err);border-color:var(--c-err);}
.btn.power{width:46px;height:46px;border-radius:50%;font-size:22px;padding:0;display:flex;align-items:center;justify-content:center;}
.btn.power.on{background:var(--c-ok);border-color:var(--c-ok);}

select,input[type=text],input[type=number]{background:var(--c-3);color:var(--c-f);
  border:1px solid var(--c-4);border-radius:6px;padding:8px 10px;font-size:15px;width:100%;}
select{appearance:none;-webkit-appearance:none;background-image:linear-gradient(45deg,transparent 50%,var(--c-d) 50%),
  linear-gradient(135deg,var(--c-d) 50%,transparent 50%);background-position:calc(100% - 18px) center,calc(100% - 13px) center;
  background-size:5px 5px,5px 5px;background-repeat:no-repeat;padding-right:32px;}

.row{display:flex;align-items:center;gap:10px;margin:10px 0;}
.label{font-size:13px;color:var(--c-8);margin:14px 0 4px;text-transform:uppercase;letter-spacing:.5px;}

.slider{display:flex;align-items:center;gap:10px;margin:8px 0;}
.slider input[type=range]{flex:1;-webkit-appearance:none;appearance:none;height:6px;border-radius:3px;
  background:var(--c-3);outline:none;}
.slider input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;appearance:none;width:22px;height:22px;
  border-radius:50%;background:var(--c-acc);cursor:pointer;border:2px solid var(--c-2);}
.slider input[type=range]::-moz-range-thumb{width:22px;height:22px;border-radius:50%;background:var(--c-acc);
  cursor:pointer;border:2px solid var(--c-2);}
.slider .val{min-width:46px;text-align:right;color:var(--c-d);font-variant-numeric:tabular-nums;}
.slider .icon{width:22px;text-align:center;color:var(--c-b);font-size:14px;}
.slider .lbl{min-width:80px;font-size:13px;color:var(--c-d);}

/* color picker */
#picker{display:flex;justify-content:center;padding:10px 0 6px;}
.csl{display:flex;justify-content:center;gap:14px;margin:6px 0 12px;}
.csl button{width:60px;height:36px;border-radius:6px;border:2px solid var(--c-4);font-weight:600;
  font-size:13px;color:transparent;cursor:pointer;text-shadow:0 0 3px rgba(0,0,0,.7);position:relative;}
.csl button.sel{border-color:var(--c-f);}
.csl button .num{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);color:#fff;
  text-shadow:0 0 3px rgba(0,0,0,.9),0 0 2px rgba(0,0,0,.9);}
#hexw{display:flex;align-items:center;gap:8px;margin:8px 0;}
#hexw input{font-family:monospace;text-transform:uppercase;flex:1;}
#hexw .hash{color:var(--c-8);font-size:18px;}
.qcs-w{display:grid;grid-template-columns:repeat(6,1fr);gap:8px;margin:10px 0;}
.qcs{aspect-ratio:1/1;border-radius:50%;border:2px solid var(--c-4);cursor:pointer;}
.qcs.sel{border-color:var(--c-f);}

/* effect / palette list */
.list{background:var(--c-2);border-radius:8px;border:1px solid var(--c-3);max-height:50vh;overflow-y:auto;
  -webkit-overflow-scrolling:touch;}
.list .item{padding:10px 14px;border-bottom:1px solid var(--c-3);cursor:pointer;font-size:15px;
  display:flex;justify-content:space-between;align-items:center;}
.list .item:last-child{border-bottom:0;}
.list .item:hover{background:var(--c-3);}
.list .item.sel{background:var(--c-acc);color:#fff;}
.list .item .badge{font-size:11px;color:var(--c-8);}
.list .item.sel .badge{color:#cce;}
.search{position:sticky;top:0;background:var(--c-2);padding:8px;border-bottom:1px solid var(--c-3);}
.search input{padding:6px 10px;font-size:14px;}

/* per-effect dynamic form */
#effectForm{background:var(--c-2);border:1px solid var(--c-3);border-radius:8px;padding:10px 12px;margin-top:10px;}
#effectForm.hide{display:none;}
#effectForm .lbl{font-size:13px;color:var(--c-d);margin-top:6px;}
#effectForm select{margin-top:4px;}
.checkrow{display:flex;align-items:center;gap:8px;margin:8px 0;}
.checkrow input[type=checkbox]{width:18px;height:18px;}

/* Mini color editor on the Effects tab — slot row is regenerated per
   effect (count and labels driven by the selected effect's color args).
   Hidden entirely when an effect declares no colors. */
#colorSection.hide{display:none;}
.csl.compact{flex-wrap:wrap;justify-content:flex-start;}
.csl.compact button{width:42px;height:32px;font-size:11px;}
#mini-color{margin-top:8px;display:none;background:var(--c-3);border-radius:6px;padding:8px;}
#mini-color.show{display:block;}
#mini-color .row{margin:6px 0;}
#mini-color .qcs-w{margin:4px 0;}
#mini-color .lbl{font-size:11px;color:var(--c-8);text-transform:uppercase;letter-spacing:.5px;margin-bottom:4px;}

/* Presets list */
.preset-item{padding:10px 14px;border-bottom:1px solid var(--c-3);font-size:15px;display:flex;align-items:center;gap:10px;}
.preset-item:last-child{border-bottom:0;}
.preset-item .name{flex:1;font-weight:500;}
.preset-item .meta{font-size:12px;color:var(--c-8);}
.preset-item button{padding:6px 10px;font-size:13px;}
.preset-actions{display:flex;gap:8px;margin-bottom:10px;}
.preset-actions input{flex:1;}

#status{position:fixed;top:calc(var(--th) + 4px);left:50%;transform:translateX(-50%);
  background:rgba(0,0,0,.85);color:var(--c-f);padding:6px 14px;border-radius:14px;font-size:13px;
  z-index:50;opacity:0;transition:opacity .25s;pointer-events:none;}
#status.show{opacity:1;}

.brand{font-weight:600;display:flex;align-items:center;gap:8px;}
.brand .sub{font-size:12px;color:var(--c-8);font-weight:400;}
.empty{text-align:center;padding:24px 10px;color:var(--c-8);}

@media (max-width:380px){body{font-size:15px;}.csl button{width:50px;}}
</style>
</head>
<body>

<div class="tab top">
  <div class="brand">
    <div>
      <div>FPP Overlay</div>
      <div class="sub" id="modelSummary">Loading...</div>
    </div>
  </div>
  <button class="btn power" id="powerBtn" title="Toggle">⏻</button>
</div>

<div id="status"></div>

<div class="container">

  <!-- Color tab -->
  <div id="tab-color" class="tabcontent active">
    <div class="label">Model</div>
    <select id="modelSelectC"></select>

    <div class="label">Color picker</div>
    <div id="picker"></div>

    <!-- Three color slots: clicking selects which one the wheel edits -->
    <div class="csl">
      <button id="csl0" data-slot="0" class="sel"><span class="num">1</span></button>
      <button id="csl1" data-slot="1"><span class="num">2</span></button>
      <button id="csl2" data-slot="2"><span class="num">3</span></button>
    </div>

    <div id="hexw">
      <span class="hash">#</span>
      <input type="text" id="hexc" maxlength="6" placeholder="ff8000" autocomplete="off">
    </div>

    <div class="qcs-w" id="quickColors"></div>

    <div class="label">Apply</div>
    <div class="row">
      <button class="btn primary" style="flex:1;" id="fillBtn">Fill model with color 1</button>
      <button class="btn" id="clearBtn">Clear</button>
    </div>

    <div class="label">Palette</div>
    <div class="list" id="paletteList">
      <div class="search">
        <input type="text" id="palSearch" placeholder="Search palettes..." autocomplete="off">
      </div>
    </div>
  </div>

  <!-- Effects tab -->
  <div id="tab-fx" class="tabcontent">
    <div class="label">Model</div>
    <select id="modelSelectE"></select>

    <!-- Color slot row regenerated per-effect from the effect's color args -->
    <div id="colorSection" class="hide">
      <div class="label" id="colorSectionLabel">Colors</div>
      <div class="csl compact" id="cslE"></div>
      <div id="mini-color">
        <div class="lbl">Hex</div>
        <input type="text" id="hexcE" maxlength="6" placeholder="ff8000" autocomplete="off"
               style="background:var(--c-2);color:var(--c-f);border:1px solid var(--c-4);border-radius:6px;padding:6px 10px;font-family:monospace;text-transform:uppercase;width:100%;">
        <div class="lbl" style="margin-top:8px;">Quick</div>
        <div class="qcs-w" id="quickColorsE"></div>
      </div>
    </div>

    <div class="label">Effect</div>
    <div class="list" id="effectList">
      <div class="search">
        <input type="text" id="fxSearch" placeholder="Search effects..." autocomplete="off">
      </div>
    </div>

    <div id="effectForm" class="hide"></div>

    <div class="row" style="margin-top:14px;gap:8px;">
      <button class="btn primary" style="flex:1;" id="runBtn">▶ Run effect</button>
      <button class="btn danger" id="stopBtn">■ Stop</button>
    </div>
  </div>

  <!-- Presets tab -->
  <div id="tab-presets" class="tabcontent">
    <div class="label">Save current setup</div>
    <div class="preset-actions">
      <input type="text" id="presetName" placeholder="Preset name (e.g. Christmas Twinkle)" autocomplete="off">
      <button class="btn primary" id="savePresetBtn">Save</button>
    </div>
    <div class="label">Saved presets</div>
    <div class="list" id="presetList">
      <div class="empty">Loading...</div>
    </div>
  </div>

  <!-- Info tab -->
  <div id="tab-info" class="tabcontent">
    <div class="label">Device</div>
    <div id="infoBlock" class="empty">Loading...</div>
    <div class="label">Running effects</div>
    <div id="runningBlock" class="empty">None</div>
    <div class="label">Links</div>
    <div class="row" style="flex-direction:column;align-items:stretch;gap:6px;">
      <a class="btn" href="/pixeloverlaymodels.php">Configure overlay models</a>
      <a class="btn" href="/effects.php">Sequence effects (legacy)</a>
      <a class="btn" href="/">Full FPP UI</a>
    </div>
  </div>
</div>

<div class="tab bot">
  <button class="tablink active" data-tab="tab-color">🎨 Color</button>
  <button class="tablink" data-tab="tab-fx">⚡ Effects</button>
  <button class="tablink" data-tab="tab-presets">★ Presets</button>
  <button class="tablink" data-tab="tab-info">ⓘ Info</button>
</div>

<script src="iro.js"></script>
<script>
(function(){
  const $ = s => document.querySelector(s);
  const $$ = s => document.querySelectorAll(s);

  // ---------- shared client state ----------
  // colors is a sparse-but-extensible array — index N corresponds to
  // the effect's "color{N+1}" arg. The Color tab always shows the
  // first 3 slots; the Effects tab regenerates its slot row to match
  // however many color args the selected effect actually declares
  // (some effects use 0, some use 5).
  const state = {
    selectedModel: null,
    models: [],
    colors: [{r:255,g:0,b:0}, {r:0,g:0,b:255}, {r:0,g:0,b:0}],
    selectedSlot: 0,
    selectedPalette: 'Default',
    paletteCatalog: [],
    effects: [],          // [{name, args:[...]}, ...]
    selectedEffect: null,
    effectArgValues: {},  // per-effect, per-arg user-set values
    effectSlotCount: 3,   // how many color slots the selected effect needs
  };

  // Ensure state.colors has at least n entries.
  function ensureColorSlots(n) {
    while (state.colors.length < n) {
      state.colors.push({r:0, g:0, b:0});
    }
  }

  // Return slot index (0-based) for a color arg name like "color1", "color2".
  // Falls back to slot 0 for unknown names.
  function colorArgSlot(name) {
    const m = /^color(\d+)$/i.exec(name || '');
    return m ? Math.max(0, parseInt(m[1], 10) - 1) : 0;
  }

  function rgbToHex({r,g,b}){
    return ((r<<16) | (g<<8) | b).toString(16).padStart(6,'0').toUpperCase();
  }
  function hexToRgb(h){
    h = h.replace('#','').padStart(6,'0').slice(0,6);
    const n = parseInt(h, 16);
    if (isNaN(n)) return null;
    return {r:(n>>16)&255, g:(n>>8)&255, b:n&255};
  }

  function toast(msg, isErr){
    const el = $('#status');
    el.textContent = msg;
    el.style.background = isErr ? 'rgba(204,51,34,.95)' : 'rgba(0,0,0,.85)';
    el.classList.add('show');
    clearTimeout(toast._t);
    toast._t = setTimeout(()=>el.classList.remove('show'), 1800);
  }

  async function fetchJSON(url, opts){
    const r = await fetch(url, opts);
    if (!r.ok) throw new Error(r.status + ' ' + r.statusText);
    const t = await r.text();
    try { return JSON.parse(t); } catch(e){ return t; }
  }

  // ---------- top bar ----------
  function updateModelSummary(){
    const m = state.models.find(x => (x.Name||x.name) === state.selectedModel);
    if (!m) { $('#modelSummary').textContent = '—'; return; }
    const running = m.effectRunning ? ' • ' + (m.effectName||'effect') : '';
    $('#modelSummary').textContent = (m.Name||m.name) + running;
    $('#powerBtn').classList.toggle('on', !!m.effectRunning || (m.isActive>0));
  }

  // ---------- color picker ----------
  let colorPicker = null;

  function buildPicker(){
    // iro.js exposes the constructor as iro.ColorPicker
    colorPicker = new iro.ColorPicker('#picker', {
      width: Math.min(260, window.innerWidth - 60),
      color: '#' + rgbToHex(state.colors[0]),
      borderWidth: 2,
      borderColor: '#444',
      layout: [
        { component: iro.ui.Wheel },
        { component: iro.ui.Slider, options: { sliderType: 'value' } },
      ],
    });
    colorPicker.on('color:change', c => {
      const rgb = c.rgb;
      state.colors[state.selectedSlot] = {r:rgb.r|0, g:rgb.g|0, b:rgb.b|0};
      $('#hexc').value = rgbToHex(state.colors[state.selectedSlot]);
      paintSlots();
    });
  }

  // Paint both slot rows (Color tab fixed-3 + Effects tab dynamic) —
  // they share state. Buttons may not exist for indices beyond what the
  // current effect needs; skip cleanly in that case.
  function paintSlots(){
    state.colors.forEach((c, i) => {
      const hex = '#' + rgbToHex(c);
      [$('#csl' + i), $('#cslE' + i)].forEach(btn => {
        if (!btn) return;
        btn.style.background = hex;
        btn.classList.toggle('sel', i === state.selectedSlot);
      });
    });
    // Mirror the active slot's hex into both inputs.
    if (state.colors[state.selectedSlot]) {
      const active = rgbToHex(state.colors[state.selectedSlot]);
      if ($('#hexc'))  $('#hexc').value  = active;
      if ($('#hexcE')) $('#hexcE').value = active;
    }
  }

  function selectSlot(i, openMini){
    ensureColorSlots(i + 1);
    state.selectedSlot = i;
    paintSlots();
    if (colorPicker) colorPicker.color.set('#' + rgbToHex(state.colors[i]));
    if (openMini) $('#mini-color').classList.add('show');
  }

  // Build the Effects-tab compact slot row from the current effect's
  // color args. Hides the whole color section if the effect uses none.
  function renderEffectColorSlots() {
    const eff = state.effects.find(e => e.name === state.selectedEffect);
    const sec = $('#colorSection');
    const row = $('#cslE');
    row.innerHTML = '';

    const args = (eff && Array.isArray(eff.args)) ? eff.args : [];
    const colorArgs = args.filter(a => a.type === 'color');
    state.effectSlotCount = colorArgs.length;
    if (!colorArgs.length) {
      sec.classList.add('hide');
      $('#mini-color').classList.remove('show');
      return;
    }
    sec.classList.remove('hide');
    ensureColorSlots(colorArgs.length);

    // If the previously-selected slot is now out of range for this
    // effect, bump back to slot 0 so paintSlots highlights something
    // visible.
    if (state.selectedSlot >= colorArgs.length) state.selectedSlot = 0;

    colorArgs.forEach((a, idx) => {
      const slot = colorArgSlot(a.name);
      const b = document.createElement('button');
      b.id = 'cslE' + slot;
      b.dataset.slot = String(slot);
      const label = a.description && a.description.length <= 12
        ? a.description : String(idx + 1);
      b.innerHTML = '<span class="num">' + escapeHtml(label) + '</span>';
      b.addEventListener('click', () => {
        const wasSel = state.selectedSlot === slot && $('#mini-color').classList.contains('show');
        if (wasSel) { $('#mini-color').classList.remove('show'); return; }
        selectSlot(slot, true);
      });
      row.appendChild(b);
    });

    paintSlots();
  }

  // ---------- API helpers ----------
  async function runCommand(cmdName, args){
    return await fetchJSON('/api/command', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({
        command: cmdName,
        multisyncCommand: false,
        multisyncHosts: '',
        args: args,
      }),
    });
  }

  // ---------- models ----------
  async function loadModels(){
    state.models = await fetchJSON('/api/overlays/models');
    const opts = state.models.length
      ? state.models.map(m => {
          const n = m.Name || m.name;
          const dim = (m.width && m.height) ? ' (' + m.width + '×' + m.height + ')' : '';
          return `<option value="${n}">${n}${dim}</option>`;
        }).join('')
      : '<option>(no overlay models configured)</option>';
    $('#modelSelectC').innerHTML = opts;
    $('#modelSelectE').innerHTML = opts;

    if (!state.models.length){
      $('#modelSummary').textContent = 'No models — configure first';
      return;
    }
    const remembered = localStorage.getItem('fpp.overlay.model');
    state.selectedModel = (remembered && state.models.find(m => (m.Name||m.name) === remembered))
      ? remembered : (state.models[0].Name || state.models[0].name);
    $('#modelSelectC').value = state.selectedModel;
    $('#modelSelectE').value = state.selectedModel;
    updateModelSummary();
  }

  function setModel(name){
    state.selectedModel = name;
    localStorage.setItem('fpp.overlay.model', name);
    $('#modelSelectC').value = name;
    $('#modelSelectE').value = name;
    updateModelSummary();
  }

  // ---------- effects ----------
  async function loadEffects(){
    state.effects = await fetchJSON('/api/overlays/effects?full=true');

    // Pull the palette catalog from any effect that has one (they all share the same list).
    for (const e of state.effects){
      const pa = (e.args || []).find(a => a.name === 'palette');
      if (pa && pa.contents){
        state.paletteCatalog = pa.contents;
        state.selectedPalette = localStorage.getItem('fpp.overlay.palette') || pa.contents[0];
        break;
      }
    }

    state.selectedEffect = localStorage.getItem('fpp.overlay.effect');
    renderEffects('');
    renderPalettes('');
    if (state.selectedEffect) {
      renderEffectForm();
      renderEffectColorSlots();
    }
  }

  function renderEffects(filter){
    const list = $('#effectList');
    const search = list.querySelector('.search');
    list.innerHTML = '';
    list.appendChild(search);
    const f = (filter || '').trim().toLowerCase();
    let added = 0;
    for (const e of state.effects){
      const n = e.name;
      if (f && !n.toLowerCase().includes(f)) continue;
      const div = document.createElement('div');
      div.className = 'item' + (n === state.selectedEffect ? ' sel' : '');
      div.dataset.fx = n;
      const ns = document.createElement('span');
      ns.textContent = n;
      div.appendChild(ns);
      if (n.startsWith('WLED')) {
        const b = document.createElement('span');
        b.className = 'badge';
        b.textContent = n.startsWith('WLED♪') ? '♪' : 'WLED';
        div.appendChild(b);
      }
      div.addEventListener('click', () => selectEffect(n));
      list.appendChild(div);
      added++;
    }
    if (added === 0){
      const e = document.createElement('div');
      e.className = 'empty';
      e.textContent = 'No matches';
      list.appendChild(e);
    }
  }

  function selectEffect(name){
    state.selectedEffect = name;
    localStorage.setItem('fpp.overlay.effect', name);
    $$('#effectList .item').forEach(el => el.classList.toggle('sel', el.dataset.fx === name));
    renderEffectForm();
    renderEffectColorSlots();
  }

  // Build a per-effect dynamic argument form. Each effect arg becomes an
  // input. Slider values land in state.effectArgValues[effectName][argName];
  // palette and color args are pulled from the shared color/palette state
  // at run-time so the user can keep adjusting them in the Color tab.
  function renderEffectForm(){
    const eff = state.effects.find(e => e.name === state.selectedEffect);
    const form = $('#effectForm');
    if (!eff){ form.classList.add('hide'); return; }
    form.classList.remove('hide');
    form.innerHTML = '';

    const stash = state.effectArgValues[eff.name] = state.effectArgValues[eff.name] || {};
    const args = Array.isArray(eff.args) ? eff.args : [];
    if (!args.length) { form.classList.add('hide'); return; }

    for (const a of args){
      // Skip palette and color args — those come from the Color tab state
      // at run-time. Reflecting them as form fields too would just clutter
      // the page and let the two views drift out of sync.
      if (a.name === 'palette') continue;
      if (a.type === 'color') continue;

      if (a.type === 'range' || a.type === 'int'){
        const cur = stash[a.name] !== undefined ? stash[a.name] : a.default;
        const min = a.min !== undefined ? a.min : 0;
        const max = a.max !== undefined ? a.max : 255;
        const wrap = document.createElement('div');
        wrap.className = 'slider';
        wrap.innerHTML =
          `<span class="lbl">${a.description || a.name}</span>` +
          `<input type="range" min="${min}" max="${max}" value="${cur}" data-arg="${a.name}">` +
          `<span class="val">${cur}</span>`;
        const inp = wrap.querySelector('input');
        const val = wrap.querySelector('.val');
        inp.addEventListener('input', () => {
          val.textContent = inp.value;
          stash[a.name] = inp.value;
        });
        form.appendChild(wrap);
      } else if (a.type === 'bool'){
        const cur = stash[a.name] !== undefined ? stash[a.name] :
                    (a.default === '1' || a.default === 'true');
        const wrap = document.createElement('div');
        wrap.className = 'checkrow';
        wrap.innerHTML =
          `<input type="checkbox" id="ck-${a.name}" ${cur ? 'checked' : ''}>` +
          `<label for="ck-${a.name}">${a.description || a.name}</label>`;
        wrap.querySelector('input').addEventListener('change', e => {
          stash[a.name] = e.target.checked;
        });
        form.appendChild(wrap);
      } else if (a.type === 'string' && Array.isArray(a.contents)){
        const cur = stash[a.name] !== undefined ? stash[a.name] : a.default;
        const wrap = document.createElement('div');
        wrap.innerHTML = `<div class="lbl">${a.description || a.name}</div>` +
          `<select data-arg="${a.name}">` +
          a.contents.map(c => `<option value="${c}" ${c === cur ? 'selected' : ''}>${c}</option>`).join('') +
          `</select>`;
        wrap.querySelector('select').addEventListener('change', e => {
          stash[a.name] = e.target.value;
        });
        form.appendChild(wrap);
      } else if (a.type === 'string'){
        const cur = stash[a.name] !== undefined ? stash[a.name] : (a.default || '');
        const wrap = document.createElement('div');
        wrap.innerHTML = `<div class="lbl">${a.description || a.name}</div>` +
          `<input type="text" value="${cur.replace(/"/g,'&quot;')}" data-arg="${a.name}">`;
        wrap.querySelector('input').addEventListener('change', e => {
          stash[a.name] = e.target.value;
        });
        form.appendChild(wrap);
      }
    }
  }

  // ---------- palettes ----------
  function renderPalettes(filter){
    const list = $('#paletteList');
    const search = list.querySelector('.search');
    list.innerHTML = '';
    list.appendChild(search);
    const f = (filter || '').trim().toLowerCase();
    let added = 0;
    for (const p of state.paletteCatalog){
      if (f && !p.toLowerCase().includes(f)) continue;
      const div = document.createElement('div');
      div.className = 'item' + (p === state.selectedPalette ? ' sel' : '');
      div.dataset.pal = p;
      div.textContent = p;
      div.addEventListener('click', () => {
        state.selectedPalette = p;
        localStorage.setItem('fpp.overlay.palette', p);
        $$('#paletteList .item').forEach(el => el.classList.toggle('sel', el.dataset.pal === p));
        toast('Palette: ' + p);
      });
      list.appendChild(div);
      added++;
    }
    if (added === 0){
      const e = document.createElement('div');
      e.className = 'empty';
      e.textContent = state.paletteCatalog.length ? 'No matches' : 'Loading palettes...';
      list.appendChild(e);
    }
  }

  // ---------- run / stop ----------
  // Build the args array for the "Overlay Model Effect" command. Order:
  //   args[0] = model name
  //   args[1] = AutoEnable ("Enabled" / "Disabled" / "Transparent" / "Transparent RGB")
  //   args[2] = effect name
  //   args[3..] = effect-specific args, IN THE EXACT ORDER the effect declares them
  function buildRunArgs(eff){
    const out = [state.selectedModel, 'Enabled', eff.name];
    const stash = state.effectArgValues[eff.name] || {};
    const args = Array.isArray(eff.args) ? eff.args : [];
    for (const a of args){
      let v;
      if (a.name === 'palette') {
        v = state.selectedPalette || a.default || 'Default';
      } else if (a.type === 'color') {
        // colorN -> slot N-1; supports any number of color args (effects
        // like "Bars" declare up to color5).
        const slot = colorArgSlot(a.name);
        ensureColorSlots(slot + 1);
        v = '#' + rgbToHex(state.colors[slot]);
      } else if (stash[a.name] !== undefined) {
        v = stash[a.name];
        if (a.type === 'bool') v = v ? '1' : '0';
      } else {
        v = a.default !== undefined ? a.default : '';
      }
      out.push(String(v));
    }
    return out;
  }

  async function runEffect(){
    if (!state.selectedModel) return toast('Pick a model', true);
    if (!state.selectedEffect) return toast('Pick an effect', true);
    const eff = state.effects.find(e => e.name === state.selectedEffect);
    if (!eff) return;
    try {
      await runCommand('Overlay Model Effect', buildRunArgs(eff));
      toast('Started: ' + eff.name);
      setTimeout(refresh, 350);
    } catch(e){ toast('Failed to start effect', true); }
  }

  async function stopEffect(){
    if (!state.selectedModel) return;
    try {
      await runCommand('Overlay Model Effect',
        [state.selectedModel, 'Disabled', 'Stop Effects']);
      toast('Stopped');
      setTimeout(refresh, 350);
    } catch(e){ toast('Failed to stop', true); }
  }

  async function togglePower(){
    if (!state.selectedModel) return;
    const m = state.models.find(x => (x.Name||x.name) === state.selectedModel);
    const running = m && (m.effectRunning || m.isActive>0);
    if (running) {
      stopEffect();
    } else if (state.selectedEffect) {
      runEffect();
    } else {
      try {
        await fetch('/api/overlays/model/' + encodeURIComponent(state.selectedModel) + '/state',
          {method:'PUT', headers:{'Content-Type':'application/json'},
           body: JSON.stringify({State: 1})});
        toast('Enabled');
        setTimeout(refresh, 250);
      } catch(e){ toast('Failed', true); }
    }
  }

  async function fillColor(){
    if (!state.selectedModel) return toast('Pick a model', true);
    const c = state.colors[0];
    try {
      await fetch('/api/overlays/model/' + encodeURIComponent(state.selectedModel) + '/state',
        {method:'PUT', headers:{'Content-Type':'application/json'},
         body: JSON.stringify({State: 1})});
      await fetch('/api/overlays/model/' + encodeURIComponent(state.selectedModel) + '/fill',
        {method:'PUT', headers:{'Content-Type':'application/json'},
         body: JSON.stringify({RGB: [c.r, c.g, c.b]})});
      toast('Filled');
      setTimeout(refresh, 250);
    } catch(e){ toast('Fill failed', true); }
  }

  async function clearModel(){
    if (!state.selectedModel) return;
    try {
      await fetch('/api/overlays/model/' + encodeURIComponent(state.selectedModel) + '/clear');
      toast('Cleared');
      setTimeout(refresh, 250);
    } catch(e){ toast('Clear failed', true); }
  }

  async function refresh(){
    try {
      state.models = await fetchJSON('/api/overlays/models');
      updateModelSummary();
      const r = await fetchJSON('/api/overlays/running');
      const block = $('#runningBlock');
      if (!r.length) {
        block.className = 'empty';
        block.textContent = 'No effects running';
      } else {
        block.className = '';
        block.innerHTML = '';
        for (const item of r){
          const div = document.createElement('div');
          div.className = 'row';
          const name = (item.Name || item.name || '?');
          const eff = (item.effect && item.effect.name) || '?';
          div.innerHTML = '<div style="flex:1;">' + name + ' → <b>' + eff + '</b></div>';
          const btn = document.createElement('button');
          btn.className = 'btn danger';
          btn.textContent = 'Stop';
          btn.addEventListener('click', async () => {
            await runCommand('Overlay Model Effect', [name, 'Disabled', 'Stop Effects']);
            setTimeout(refresh, 250);
          });
          div.appendChild(btn);
          block.appendChild(div);
        }
      }
    } catch(e){ /* ignore */ }
  }

  async function loadInfo(){
    try {
      const i = await fetchJSON('/json/info');
      const block = $('#infoBlock');
      block.className = '';
      block.innerHTML =
        '<div class="row"><div style="flex:1;color:var(--c-8);">Name</div><div>' + (i.name||'-') + '</div></div>' +
        '<div class="row"><div style="flex:1;color:var(--c-8);">Version</div><div>' + (i.ver||'-') + '</div></div>' +
        '<div class="row"><div style="flex:1;color:var(--c-8);">Platform</div><div>' + (i.arch||'-') + '</div></div>' +
        '<div class="row"><div style="flex:1;color:var(--c-8);">IP</div><div>' + (i.ip||'-') + '</div></div>' +
        '<div class="row"><div style="flex:1;color:var(--c-8);">Effects</div><div>' + (i.fxcount||0) + '</div></div>';
    } catch(e){}
  }

  // Quick-color swatches matching WLED's defaults. Builds the same row
  // in both the full Color tab and the compact Effects-tab editor; both
  // write into the shared color slots.
  const QUICK = ['#ff0000','#ffa000','#ffc800','#ffffff','#000000','#ff00ff',
                 '#0000ff','#00ffc8','#08ff00','#ff8000','#a000ff','#ff0080'];
  function applyQuickColor(c){
    const rgb = hexToRgb(c);
    state.colors[state.selectedSlot] = rgb;
    if (colorPicker) colorPicker.color.set(c);
    paintSlots();
  }
  function buildQuick(){
    [$('#quickColors'), $('#quickColorsE')].forEach(w => {
      if (!w) return;
      QUICK.forEach(c => {
        const d = document.createElement('div');
        d.className = 'qcs';
        d.style.background = c;
        d.addEventListener('click', () => applyQuickColor(c));
        w.appendChild(d);
      });
    });
  }

  // ---------- presets ----------
  // Stored in /home/fpp/media/config/commandPresets.json. We surface only
  // entries whose command is "Overlay Model Effect" — other commands are
  // left untouched on save (we round-trip the full file).
  let presetCache = { commands: [] };

  async function loadPresets(){
    try {
      presetCache = await fetchJSON('/api/configfile/commandPresets.json');
      if (!presetCache || typeof presetCache !== 'object' || !Array.isArray(presetCache.commands)) {
        presetCache = { commands: [] };
      }
    } catch(e){
      presetCache = { commands: [] };
    }
    renderPresets();
  }

  function renderPresets(){
    const list = $('#presetList');
    list.innerHTML = '';
    const overlayPresets = presetCache.commands.filter(p => p.command === 'Overlay Model Effect');
    if (!overlayPresets.length){
      list.innerHTML = '<div class="empty">No overlay presets yet — set up an effect and click Save above.</div>';
      return;
    }
    for (const p of overlayPresets){
      const args = p.args || [];
      const model = args[0] || '?';
      const effect = args[2] || '?';
      const div = document.createElement('div');
      div.className = 'preset-item';
      div.innerHTML =
        '<div><div class="name">' + escapeHtml(p.name || '(unnamed)') + '</div>' +
        '<div class="meta">' + escapeHtml(effect) + ' on ' + escapeHtml(model) + '</div></div>';
      const loadBtn = document.createElement('button');
      loadBtn.className = 'btn primary';
      loadBtn.textContent = 'Load';
      loadBtn.addEventListener('click', () => loadPreset(p));
      const delBtn = document.createElement('button');
      delBtn.className = 'btn danger';
      delBtn.textContent = '✕';
      delBtn.title = 'Delete';
      delBtn.addEventListener('click', () => deletePreset(p.name));
      div.appendChild(loadBtn);
      div.appendChild(delBtn);
      list.appendChild(div);
    }
  }

  function escapeHtml(s){
    return String(s).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  }

  // Apply a preset back into the UI state, then run it. Walking the
  // effect's CommandArg list lets us put each saved arg back where the
  // matching form input expects it (sliders, palette, color slots).
  async function loadPreset(p){
    const args = p.args || [];
    if (args.length < 3) return toast('Preset is malformed', true);
    const model = args[0];
    const effectName = args[2];
    const effect = state.effects.find(e => e.name === effectName);

    // Hop the model + effect into UI state so the user sees what loaded.
    if (state.models.find(m => (m.Name||m.name) === model)) {
      setModel(model);
    }
    selectEffect(effectName);

    if (effect) {
      // Slice off the [model, autoEnable, effect] header to get effect-args.
      const effArgs = args.slice(3);
      const stash = state.effectArgValues[effectName] = state.effectArgValues[effectName] || {};
      const effDefs = Array.isArray(effect.args) ? effect.args : [];
      effDefs.forEach((a, i) => {
        const v = effArgs[i];
        if (v === undefined) return;
        if (a.name === 'palette') {
          state.selectedPalette = v;
          localStorage.setItem('fpp.overlay.palette', v);
        } else if (a.type === 'color') {
          const rgb = hexToRgb(v);
          if (rgb) {
            const slot = colorArgSlot(a.name);
            ensureColorSlots(slot + 1);
            state.colors[slot] = rgb;
          }
        } else if (a.type === 'bool') {
          stash[a.name] = (v === '1' || v === 'true' || v === true);
        } else {
          stash[a.name] = v;
        }
      });
      paintSlots();
      renderPalettes($('#palSearch') ? $('#palSearch').value : '');
      renderEffectForm();
      renderEffectColorSlots();
    }

    // Fire the preset as-is — preserves any custom args we may not have
    // recognized while replaying it into the UI state.
    try {
      await runCommand('Overlay Model Effect', args);
      toast('Loaded: ' + (p.name || effectName));
      setTimeout(refresh, 350);
    } catch(e) {
      toast('Failed to apply preset', true);
    }
  }

  async function savePreset(){
    if (!state.selectedEffect) return toast('Pick an effect first', true);
    if (!state.selectedModel)  return toast('Pick a model first', true);
    const name = ($('#presetName').value || '').trim();
    if (!name) return toast('Give the preset a name', true);

    const eff = state.effects.find(e => e.name === state.selectedEffect);
    if (!eff) return;

    const newPreset = {
      command: 'Overlay Model Effect',
      args: buildRunArgs(eff),
      multisyncCommand: false,
      name: name,
      presetSlot: null,
    };

    // Replace by name if it exists, otherwise append. Other-command
    // presets are passed through untouched.
    const existing = presetCache.commands.findIndex(
      p => p.command === 'Overlay Model Effect' && p.name === name);
    if (existing >= 0) {
      presetCache.commands[existing] = newPreset;
    } else {
      presetCache.commands.push(newPreset);
    }

    try {
      const r = await fetchJSON('/api/configfile/commandPresets.json', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(presetCache, null, 2),
      });
      if (r && r.Status === 'OK') {
        toast('Saved: ' + name);
        $('#presetName').value = '';
        renderPresets();
      } else {
        toast('Save failed', true);
      }
    } catch(e) {
      toast('Save failed', true);
    }
  }

  async function deletePreset(name){
    if (!confirm('Delete preset "' + name + '"?')) return;
    presetCache.commands = presetCache.commands.filter(
      p => !(p.command === 'Overlay Model Effect' && p.name === name));
    try {
      await fetchJSON('/api/configfile/commandPresets.json', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(presetCache, null, 2),
      });
      toast('Deleted: ' + name);
      renderPresets();
    } catch(e) {
      toast('Delete failed', true);
    }
  }

  // ---------- wire up ----------
  document.addEventListener('DOMContentLoaded', () => {
    $$('.tablink').forEach(b => b.addEventListener('click', () => {
      $$('.tablink').forEach(x => x.classList.remove('active'));
      $$('.tabcontent').forEach(x => x.classList.remove('active'));
      b.classList.add('active');
      $('#' + b.dataset.tab).classList.add('active');
      if (b.dataset.tab === 'tab-info')    loadInfo();
      if (b.dataset.tab === 'tab-presets') loadPresets();
    }));

    $('#modelSelectC').addEventListener('change', e => setModel(e.target.value));
    $('#modelSelectE').addEventListener('change', e => setModel(e.target.value));

    $('#fxSearch').addEventListener('input', e => renderEffects(e.target.value));
    $('#palSearch').addEventListener('input', e => renderPalettes(e.target.value));

    $('#runBtn').addEventListener('click', runEffect);
    $('#stopBtn').addEventListener('click', stopEffect);
    $('#powerBtn').addEventListener('click', togglePower);
    $('#fillBtn').addEventListener('click', fillColor);
    $('#clearBtn').addEventListener('click', clearModel);

    // Color tab slot row — fixed at 3 slots, opens nothing extra (the
    // wheel is right there). The Effects-tab slot row is built dynamically
    // by renderEffectColorSlots() based on the selected effect's args.
    [0,1,2].forEach(i => $('#csl' + i).addEventListener('click', () => selectSlot(i, false)));

    function applyHex(input){
      const rgb = hexToRgb(input.value);
      if (!rgb) return;
      state.colors[state.selectedSlot] = rgb;
      if (colorPicker) colorPicker.color.set('#' + rgbToHex(rgb));
      paintSlots();
    }
    $('#hexc').addEventListener('change',  () => applyHex($('#hexc')));
    $('#hexcE').addEventListener('change', () => applyHex($('#hexcE')));
    $('#hexc').value  = rgbToHex(state.colors[0]);
    $('#hexcE').value = rgbToHex(state.colors[0]);

    // Presets
    $('#savePresetBtn').addEventListener('click', savePreset);
    $('#presetName').addEventListener('keydown', e => { if (e.key === 'Enter') savePreset(); });

    buildPicker();
    buildQuick();
    paintSlots();

    loadModels().then(loadEffects).then(refresh);
    setInterval(refresh, 5000);
  });
})();
</script>
</body>
</html>
