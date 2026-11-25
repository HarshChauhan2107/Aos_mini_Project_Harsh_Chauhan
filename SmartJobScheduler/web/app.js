const API = '';
const COLORS = ['#7168dc','#3eabc0','#9877df','#35ad91','#d78e62','#507ac4','#b56eaa','#7891a7','#b5a15b'];
const ALGORITHMS = {
  FCFS: 'FCFS executes jobs in arrival order. It is simple and fair, but long jobs can delay shorter ones.',
  SJF: 'SJF selects the shortest available CPU burst. It often reduces average waiting time, but long jobs may wait longer.',
  SRTF: 'SRTF is the preemptive form of shortest-job scheduling. A newly arrived shorter job can interrupt the current job.',
  PRIORITY: 'Priority scheduling selects the highest-priority available job (1 is highest). Equal priorities are resolved by arrival time.',
  RR: 'Round Robin gives each ready job a fixed time quantum. It improves responsiveness and fairness for time-sharing workloads.',
  EDF: 'Earliest Deadline First selects the available job with the closest deadline. It is useful when time constraints matter.'
};
let currentJobs = [];
let latestResult = null;
let recommendation = null;

const $ = (id) => document.getElementById(id);
const esc = (value) => String(value).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));

async function api(path, options = {}) {
  const response = await fetch(API + path, {
    headers: { 'Content-Type': 'application/json', ...(options.headers || {}) },
    ...options
  });
  let data;
  try { data = await response.json(); } catch { throw new Error('The server returned an unreadable response.'); }
  if (!response.ok) throw new Error(data.error || `Request failed (${response.status}).`);
  return data;
}

function toast(message, type = '') {
  const node = document.createElement('div');
  node.className = `toast ${type}`;
  node.textContent = message;
  $('toastRegion').appendChild(node);
  setTimeout(() => node.remove(), 3200);
}

function setBusy(button, busy, label) {
  if (!button) return;
  button.disabled = busy;
  if (busy) {
    button.dataset.original = button.innerHTML;
    button.textContent = label || 'Working...';
  } else if (button.dataset.original) {
    button.innerHTML = button.dataset.original;
    delete button.dataset.original;
  }
}

async function checkHealth() {
  try {
    const data = await api('/api/health');
    $('backendStatus').textContent = `C ENGINE ONLINE · ${data.jobCount} JOBS`;
    return true;
  } catch (error) {
    $('backendStatus').textContent = 'BACKEND OFFLINE';
    toast('Cannot reach the C backend. Start the server window and refresh this page.', 'error');
    return false;
  }
}

async function loadJobs() {
  const data = await api('/api/jobs');
  currentJobs = data.jobs || [];
  renderJobs();
  $('totalJobs').textContent = currentJobs.length;
  return currentJobs;
}

function renderJobs() {
  $('queueCount').textContent = currentJobs.length;
  $('queueFooter').textContent = `${currentJobs.length} of 100 job slots used · saved locally`;
  if (!currentJobs.length) {
    $('jobsBody').innerHTML = '<tr><td colspan="6" class="table-empty">No jobs in the queue. Add a task to begin.</td></tr>';
    return;
  }
  $('jobsBody').innerHTML = currentJobs.map(job => {
    const tone = job.priority === 1 ? 'high' : job.priority >= 4 ? 'low' : '';
    return `<tr>
      <td class="job-name-cell">${esc(job.name)}<span class="job-id">JOB-${String(job.id).padStart(3,'0')}</span></td>
      <td>${job.arrival}</td><td>${job.burst}</td>
      <td><span class="priority-badge ${tone}">P${job.priority}</span></td>
      <td>${job.deadline}</td>
      <td><button class="delete-job" data-delete="${job.id}" aria-label="Delete ${esc(job.name)}" title="Remove job">×</button></td>
    </tr>`;
  }).join('');
}

function updateAlgorithmUI() {
  const algorithm = $('algorithmSelect').value;
  $('quantumField').classList.toggle('visible', algorithm === 'RR');
  $('algorithmHint').querySelector('span:last-child').textContent = ALGORITHMS[algorithm];
}

function renderGantt(result) {
  const segments = result.segments || [];
  if (!segments.length || result.metrics.makespan <= 0) {
    $('ganttEmpty').classList.remove('hidden');
    $('ganttWrap').classList.add('hidden');
    return;
  }
  $('ganttEmpty').classList.add('hidden');
  $('ganttWrap').classList.remove('hidden');
  const start = Math.min(...segments.map(s => s.start));
  const end = Math.max(...segments.map(s => s.end));
  const total = Math.max(1, end - start);
  const axis = [];
  const tickCount = Math.min(10, Math.max(2, total + 1));
  for (let i = 0; i <= tickCount; i++) {
    const t = Math.round(start + total * i / tickCount);
    const edge = i === 0 ? 'transform:none' : i === tickCount ? 'transform:translateX(-100%)' : 'transform:translateX(-50%)';
    axis.push(`<span style="left:${(i / tickCount) * 100}%;${edge}">${t}</span>`);
  }
  $('ganttAxis').innerHTML = axis.join('');
  const minWidth = segments.length > 18 ? 0 : 0;
  $('ganttTrack').innerHTML = segments.map((segment, index) => {
    const width = Math.max(minWidth, ((segment.end - segment.start) / total) * 100);
    const idle = segment.jobId === 0;
    const color = idle ? '#e8ecf3' : COLORS[(Math.abs(segment.jobId) - 1) % COLORS.length];
    const label = idle ? 'IDLE' : esc(segment.name);
    const title = `${label}: ${segment.start} → ${segment.end} (${segment.duration} units)`;
    return `<div class="gantt-segment ${idle ? 'idle' : ''}" style="flex:0 0 ${width}%;background:${color}" title="${esc(title)}"><strong>${label}</strong><small>${segment.start}–${segment.end}</small></div>`;
  }).join('');
  $('ganttFoot').innerHTML = `<span>TIME ${start}</span><span>${segments.length} execution blocks</span><span>TIME ${end}</span>`;
}

function renderResults(result) {
  if (!result.jobs || !result.jobs.length) {
    $('resultsBody').innerHTML = '<tr><td colspan="6" class="table-empty">No jobs to analyze.</td></tr>';
    return;
  }
  $('resultsBody').innerHTML = result.jobs.map(job => `<tr>
    <td>${esc(job.name)}</td><td>${job.start}</td><td>${job.completion}</td>
    <td>${job.waiting}</td><td>${job.turnaround}</td><td>${job.response}</td>
  </tr>`).join('');
}

function updateMetrics(result) {
  const m = result.metrics;
  $('avgWaiting').textContent = Number(m.avgWaiting).toFixed(2);
  $('avgTurnaround').textContent = Number(m.avgTurnaround).toFixed(2);
  $('cpuUtilization').textContent = Number(m.cpuUtilization).toFixed(1);
  $('utilTrack').style.width = `${Math.max(0, Math.min(100, m.cpuUtilization))}%`;
  $('lastAlgorithm').textContent = result.algorithm;
  $('lastMakespan').textContent = `Makespan: ${m.makespan}`;
}

async function runSchedule(algorithm = $('algorithmSelect').value, showToast = true) {
  const button = $('runBtn');
  setBusy(button, true, 'Scheduling...');
  try {
    const quantum = Math.max(1, Math.min(50, Number($('quantumInput').value) || 2));
    const result = await api('/api/schedule', { method: 'POST', body: JSON.stringify({ algorithm, quantum }) });
    if (result.error) throw new Error(result.error);
    latestResult = result;
    updateMetrics(result);
    renderGantt(result);
    renderResults(result);
    $('algorithmSelect').value = algorithm;
    updateAlgorithmUI();
    if (showToast) toast(`${result.algorithm} schedule generated successfully.`, 'success');
    return result;
  } catch (error) {
    toast(error.message, 'error');
    return null;
  } finally {
    setBusy(button, false);
  }
}

async function getRecommendation() {
  try {
    recommendation = await api('/api/recommend');
    const fullName = {
      FCFS: 'First Come, First Served', SJF: 'Shortest Job First', SRTF: 'Shortest Remaining Time',
      PRIORITY: 'Priority Scheduling', RR: 'Round Robin', EDF: 'Earliest Deadline First'
    }[recommendation.algorithm] || recommendation.algorithm;
    $('recommendValue').textContent = fullName;
    $('recommendReason').textContent = recommendation.reason;
    return recommendation;
  } catch (error) {
    toast(error.message, 'error');
    return null;
  }
}

async function refreshAll() {
  try {
    await loadJobs();
    await getRecommendation();
    await checkHealth();
  } catch (error) {
    toast(error.message, 'error');
  }
}

async function addJob(event) {
  event.preventDefault();
  const button = $('jobForm').querySelector('button[type="submit"]');
  const payload = {
    name: $('jobName').value.trim(),
    arrival: Number($('arrival').value),
    burst: Number($('burst').value),
    priority: Number($('priority').value),
    deadline: Number($('deadline').value)
  };
  setBusy(button, true, 'Adding...');
  try {
    await api('/api/jobs', { method: 'POST', body: JSON.stringify(payload) });
    $('jobForm').reset();
    $('arrival').value = 0; $('burst').value = 5; $('priority').value = 3; $('deadline').value = 20;
    await refreshAll();
    toast('Job added and saved to the local queue.', 'success');
  } catch (error) { toast(error.message, 'error'); }
  finally { setBusy(button, false); }
}

async function deleteJob(id) {
  try {
    await api(`/api/jobs?id=${encodeURIComponent(id)}`, { method: 'DELETE' });
    await refreshAll();
    if (latestResult) await runSchedule(latestResult.algorithm, false);
    toast('Job removed from the queue.', 'success');
  } catch (error) { toast(error.message, 'error'); }
}

async function restoreDemo() {
  if (!confirm('Restore the original seven demo jobs? Your current queue will be replaced.')) return;
  try {
    await api('/api/reset', { method: 'POST', body: '{}' });
    await refreshAll();
    await runSchedule('FCFS', false);
    toast('Demo workload restored.', 'success');
  } catch (error) { toast(error.message, 'error'); }
}

async function runBenchmark() {
  const button = $('benchmarkBtn');
  setBusy(button, true, 'Comparing policies...');
  const algorithms = ['FCFS','SJF','SRTF','PRIORITY','RR','EDF'];
  const scores = [];
  try {
    for (const algorithm of algorithms) {
      const result = await api('/api/schedule', { method: 'POST', body: JSON.stringify({ algorithm, quantum: Number($('quantumInput').value) || 2 }) });
      scores.push({ algorithm, wait: Number(result.metrics.avgWaiting), turnaround: Number(result.metrics.avgTurnaround) });
    }
    scores.sort((a,b) => a.wait - b.wait);
    const best = scores[0];
    const max = Math.max(1, ...scores.map(s => s.wait));
    const names = {FCFS:'FCFS',SJF:'SJF',SRTF:'SRTF',PRIORITY:'Priority',RR:'Round Robin',EDF:'EDF'};
    $('benchmarkResults').innerHTML = scores.map((s,i) => `<div class="bench-row ${i===0?'best':''}"><span class="bench-label">${names[s.algorithm]}</span><div class="bench-track"><div class="bench-fill" style="width:${Math.max(2,(s.wait/max)*100)}%"></div></div><span class="bench-value">${s.wait.toFixed(2)}</span></div>`).join('') + `<div class="benchmark-foot">Best average waiting time in this workload: <b>${names[best.algorithm]}</b> (${best.wait.toFixed(2)} units). This result may change when arrivals, bursts, priorities, or deadlines change.</div>`;
    toast(`Benchmark complete. ${names[best.algorithm]} has the lowest average wait.`, 'success');
  } catch (error) { toast(error.message, 'error'); }
  finally { setBusy(button, false); }
}

$('algorithmSelect').addEventListener('change', updateAlgorithmUI);
$('runBtn').addEventListener('click', () => runSchedule());
$('jobForm').addEventListener('submit', addJob);
$('jobsBody').addEventListener('click', event => {
  const button = event.target.closest('[data-delete]');
  if (button) deleteJob(Number(button.dataset.delete));
});
$('resetBtn').addEventListener('click', restoreDemo);
$('refreshBtn').addEventListener('click', refreshAll);
$('queueRefresh').addEventListener('click', refreshAll);
$('recommendBtn').addEventListener('click', async () => {
  const rec = await getRecommendation();
  if (rec) { $('algorithmSelect').value = rec.algorithm; updateAlgorithmUI(); await runSchedule(rec.algorithm); }
});
$('applyRecommendation').addEventListener('click', async () => {
  const rec = recommendation || await getRecommendation();
  if (rec) { $('algorithmSelect').value = rec.algorithm; updateAlgorithmUI(); await runSchedule(rec.algorithm); }
});
$('benchmarkBtn').addEventListener('click', runBenchmark);

(async function init() {
  updateAlgorithmUI();
  const online = await checkHealth();
  if (!online) return;
  await refreshAll();
  await runSchedule('FCFS', false);
})();
