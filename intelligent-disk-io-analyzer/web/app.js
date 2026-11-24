const $ = (id) => document.getElementById(id);

const state = { results: [], selected: null, requestCount: 0, chart: null };
const algorithmNames = {
  FCFS: 'First Come First Serve', SSTF: 'Shortest Seek Time First',
  SCAN: 'SCAN', 'C-SCAN': 'Circular SCAN', LOOK: 'LOOK', 'C-LOOK': 'Circular LOOK'
};

function parseQueue() {
  const raw = $('requests').value.trim();
  if (!raw) throw new Error('Please enter at least one disk request.');
  const parts = raw.split(/[\s,]+/).filter(Boolean);
  if (parts.length > 128) throw new Error('The queue can contain a maximum of 128 requests.');
  const values = parts.map((part) => {
    if (!/^-?\d+$/.test(part)) throw new Error(`“${part}” is not a valid integer cylinder number.`);
    const value = Number(part);
    if (!Number.isSafeInteger(value)) throw new Error('A request value is outside the supported integer range.');
    return value;
  });
  return values;
}

function currentInput() {
  const requests = parseQueue();
  const payload = {
    head: Number($('head').value), disk_min: Number($('diskMin').value),
    disk_max: Number($('diskMax').value), direction: $('direction').value,
    algorithm: $('algorithm').value, requests
  };
  for (const key of ['head', 'disk_min', 'disk_max']) {
    if (!Number.isSafeInteger(payload[key])) throw new Error('Head position and disk limits must be valid integers.');
  }
  return payload;
}

function showError(message) {
  $('errorBox').textContent = message;
  $('errorBox').hidden = false;
}
function clearError() { $('errorBox').hidden = true; $('errorBox').textContent = ''; }

function updateRequestCount() {
  try {
    const count = parseQueue().length;
    $('requestCount').textContent = `${count} request${count === 1 ? '' : 's'}`;
  } catch (_) { $('requestCount').textContent = 'Queue input'; }
}

async function runAnalysis(event) {
  event?.preventDefault();
  clearError();
  let payload;
  try { payload = currentInput(); }
  catch (error) { showError(error.message); return; }

  const button = $('analyzeBtn');
  button.disabled = true;
  button.innerHTML = '<span class="play-icon">◌</span> Analyzing…';
  try {
    const response = await fetch('/api/analyze', {
      method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(payload)
    });
    const data = await response.json();
    if (!response.ok) throw new Error(data.error || 'The C scheduling engine returned an error.');
    if (!Array.isArray(data.results) || data.results.length === 0) throw new Error('No algorithm results were returned.');
    state.results = data.results;
    state.requestCount = data.request_count;
    state.selected = chooseDefaultResult(data.results);
    renderResults();
    $('emptyState').hidden = true;
    $('resultsContent').hidden = false;
    $('resultsContent').scrollIntoView({ behavior: 'smooth', block: 'nearest' });
  } catch (error) {
    showError(`${error.message} Make sure the C server is running from the project folder.`);
  } finally {
    button.disabled = false;
    button.innerHTML = '<span class="play-icon">▶</span> Run analysis';
  }
}

function chooseDefaultResult(results) {
  return results.reduce((best, item) => item.total_movement < best.total_movement ? item : best, results[0]).algorithm;
}
function selectedResult() { return state.results.find((r) => r.algorithm === state.selected) || state.results[0]; }

function renderResults() {
  const best = state.results.reduce((a, b) => b.total_movement < a.total_movement ? b : a, state.results[0]);
  const selected = selectedResult();
  $('bestAlgorithm').textContent = best.algorithm;
  $('bestFoot').textContent = `${best.total_movement} cylinders · best in this run`;
  $('totalMovement').textContent = selected.total_movement;
  $('averageSeek').textContent = selected.average_seek.toFixed(2);
  $('servedCount').textContent = selected.service_order.length;
  $('servedTotal').textContent = `/ ${state.requestCount}`;
  renderComparison(best);
  renderDetailSelector();
  renderDetail(selected);
}

function renderComparison(best) {
  const max = Math.max(1, ...state.results.map((r) => r.total_movement));
  const rows = [...state.results].sort((a, b) => a.total_movement - b.total_movement);
  $('comparisonRows').innerHTML = rows.map((r) => {
    const selected = r.algorithm === state.selected;
    const width = Math.max(2, (r.total_movement / max) * 100);
    return `<tr data-algorithm="${r.algorithm}" class="${selected ? 'selected' : ''}" tabindex="0" aria-label="Show ${r.algorithm} details">
      <td><span class="algorithm-name">${r.algorithm}</span>${r.algorithm === best.algorithm ? '<span class="best-mark">BEST</span>' : ''}</td>
      <td><span class="movement-cell">${r.total_movement}</span></td>
      <td>${r.average_seek.toFixed(2)}</td>
      <td><div class="bar-track"><div class="bar-fill" style="width:${width}%"></div></div></td>
    </tr>`;
  }).join('');
  $('comparisonRows').querySelectorAll('tr').forEach((row) => {
    const choose = () => selectAlgorithm(row.dataset.algorithm);
    row.addEventListener('click', choose);
    row.addEventListener('keydown', (event) => { if (event.key === 'Enter' || event.key === ' ') { event.preventDefault(); choose(); } });
  });
}

function renderDetailSelector() {
  const select = $('detailAlgorithm');
  const current = state.selected;
  select.innerHTML = state.results.map((r) => `<option value="${r.algorithm}">${r.algorithm}</option>`).join('');
  select.value = current;
}

function selectAlgorithm(name) {
  if (!state.results.some((r) => r.algorithm === name)) return;
  state.selected = name;
  renderResults();
}

function renderDetail(result) {
  $('detailAlgorithm').value = result.algorithm;
  $('pathSubtitle').textContent = `${algorithmNames[result.algorithm]} · ${result.path.length} head positions including the starting point.`;
  $('movementCount').textContent = `${result.total_movement} CYLINDERS TOTAL`;
  $('sequence').innerHTML = result.path.map((position, index) => {
    const start = index === 0, end = index === result.path.length - 1;
    const arrow = index < result.path.length - 1 ? '<span class="sequence-arrow">→</span>' : '';
    return `<span class="sequence-node ${start ? 'start' : ''} ${end ? 'end' : ''}"><span class="sequence-value">${position}</span>${arrow}</span>`;
  }).join('');
  $('stepRows').innerHTML = result.steps.map((step, index) => `
    <tr><td>${index + 1}</td><td>${step.from}</td><td>${step.to}</td><td>${step.distance}</td>
    <td><span class="kind-pill ${step.kind}">${step.kind.toUpperCase()}</span></td></tr>`).join('') || '<tr><td colspan="5">No movement required.</td></tr>';
  drawChart(result);
}

function drawChart(result) {
  const canvas = $('pathChart');
  const ctx = canvas.getContext('2d');
  const cssWidth = canvas.clientWidth || 600;
  const cssHeight = 210;
  const dpr = window.devicePixelRatio || 1;
  canvas.width = Math.round(cssWidth * dpr);
  canvas.height = Math.round(cssHeight * dpr);
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, cssWidth, cssHeight);

  const values = result.path;
  const diskMin = Number($('diskMin').value);
  const diskMax = Number($('diskMax').value);
  const left = 39, right = cssWidth - 16, top = 19, bottom = cssHeight - 32;
  const plotW = Math.max(1, right - left), plotH = Math.max(1, bottom - top);
  const x = (i) => values.length <= 1 ? left + plotW / 2 : left + (i / (values.length - 1)) * plotW;
  const y = (v) => bottom - ((v - diskMin) / (diskMax - diskMin)) * plotH;

  ctx.font = '9px "DM Mono", monospace';
  ctx.fillStyle = '#71829e'; ctx.strokeStyle = 'rgba(139,158,190,.15)'; ctx.lineWidth = 1;
  for (let i = 0; i <= 4; i++) {
    const value = diskMin + ((diskMax - diskMin) * i / 4);
    const yy = y(value);
    ctx.beginPath(); ctx.moveTo(left, yy); ctx.lineTo(right, yy); ctx.stroke();
    ctx.textAlign = 'right'; ctx.fillText(String(Math.round(value)), left - 8, yy + 3);
  }
  ctx.fillStyle = '#71829e'; ctx.textAlign = 'left'; ctx.fillText('CYLINDER', 3, 10);
  ctx.textAlign = 'right'; ctx.fillText('STEP', right, cssHeight - 7);

  if (values.length > 1) {
    ctx.strokeStyle = '#7898e7'; ctx.lineWidth = 2; ctx.beginPath();
    values.forEach((v, i) => { if (i === 0) ctx.moveTo(x(i), y(v)); else ctx.lineTo(x(i), y(v)); });
    ctx.stroke();
  }
  values.forEach((v, i) => {
    const step = result.steps[i - 1];
    let color = '#8aaaff';
    if (step?.kind === 'service') color = '#75e0c4';
    if (step?.kind === 'wrap' || step?.kind === 'boundary') color = '#b09aff';
    ctx.beginPath(); ctx.arc(x(i), y(v), i === 0 ? 4 : 3.3, 0, Math.PI * 2);
    ctx.fillStyle = color; ctx.fill();
    if (values.length <= 14 || i === 0 || i === values.length - 1) {
      ctx.fillStyle = '#aab9d2'; ctx.textAlign = i === 0 ? 'left' : (i === values.length - 1 ? 'right' : 'center');
      ctx.fillText(String(v), x(i), Math.max(11, y(v) - 9));
    }
  });
}

$('simForm').addEventListener('submit', runAnalysis);
$('requests').addEventListener('input', updateRequestCount);
$('detailAlgorithm').addEventListener('change', (event) => selectAlgorithm(event.target.value));
$('sampleBtn').addEventListener('click', () => {
  $('head').value = 53; $('diskMin').value = 0; $('diskMax').value = 199;
  $('direction').value = 'right'; $('requests').value = '98, 183, 37, 122, 14, 124, 65, 67';
  $('algorithm').value = 'ALL'; updateRequestCount(); clearError();
});
$('randomBtn').addEventListener('click', () => {
  const min = Number($('diskMin').value), max = Number($('diskMax').value);
  if (!Number.isSafeInteger(min) || !Number.isSafeInteger(max) || min >= max) { showError('Set a valid disk range before generating random requests.'); return; }
  const count = 8 + Math.floor(Math.random() * 7);
  const values = Array.from({ length: count }, () => Math.floor(Math.random() * (max - min + 1)) + min);
  $('requests').value = values.join(', ');
  $('head').value = Math.floor(Math.random() * (max - min + 1)) + min;
  updateRequestCount(); clearError();
});
$('clearBtn').addEventListener('click', () => {
  $('requests').value = ''; $('head').value = ''; updateRequestCount(); clearError();
  $('resultsContent').hidden = true; $('emptyState').hidden = false; state.results = []; state.selected = null;
});
window.addEventListener('resize', () => { if (state.results.length) drawChart(selectedResult()); });

async function checkServer() {
  try {
    const response = await fetch('/api/health');
    if (!response.ok) throw new Error('offline');
    $('serverStatus').textContent = 'C engine online';
  } catch (_) {
    $('serverStatus').textContent = 'C engine offline';
    $('serverStatus').style.color = '#ff9aa7';
  }
}
updateRequestCount();
checkServer();
