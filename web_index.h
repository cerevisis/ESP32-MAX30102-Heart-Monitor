const char index_html[] PROGMEM = R"rawpriority(
<!DOCTYPE HTML><html>
<head>
  <title>Heart-Monitor Dashboard</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <script src="https://cdn.jsdelivr.net/npm/chart.js@3.9.1/dist/chart.min.js"></script>
  <style>
    :root {
      --bg-color: #0f172a;
      --card-bg: #1e293b;
      --text-main: #f8fafc;
      --text-dim: #94a3b8;
      --accent-bpm: #ef4444;
      --accent-spo2: #3b82f6;
      --accent-pulse: #10b981;
      --accent-pi: #f59e0b;
      --accent-temp: #818cf8;
      --accent-hrv: #d946ef;
    }
    body {
      font-family: 'Inter', system-ui, -apple-system, sans-serif;
      background-color: var(--bg-color);
      color: var(--text-main);
      margin: 0;
      padding: 20px;
      display: flex;
      flex-direction: column;
      align-items: center;
    }
    .header { margin-bottom: 30px; text-align: center; }
    .header h1 { margin: 0; font-weight: 700; background: linear-gradient(to right, #6366f1, #a855f7); -webkit-background-clip: text; -webkit-text-fill-color: transparent; }
    
    .status-bar {
      width: 100%;
      max-width: 1200px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      background: rgba(30, 41, 59, 0.5);
      backdrop-filter: blur(8px);
      padding: 10px 20px;
      border-radius: 12px;
      margin-bottom: 20px;
      border: 1px solid rgba(255, 255, 255, 0.1);
    }
    .status-item { display: flex; align-items: center; gap: 8px; font-size: 0.9rem; }
    .status-dot { width: 8px; height: 8px; border-radius: 50%; background: #94a3b8; transition: background 0.3s; }
    .status-text { font-weight: 600; color: var(--text-main); }
    .rssi-value { color: var(--text-dim); }

    .container {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
      gap: 20px;
      width: 100%;
      max-width: 1200px;
    }
    .card {
      background: var(--card-bg);
      padding: 24px;
      border-radius: 16px;
      box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1), 0 2px 4px -1px rgba(0, 0, 0, 0.06);
      border: 1px solid rgba(255, 255, 255, 0.05);
    }
    .kpi { display: flex; flex-direction: column; align-items: center; }
    .kpi-label { font-size: 0.875rem; color: var(--text-dim); text-transform: uppercase; letter-spacing: 0.05em; margin-bottom: 8px; }
    .kpi-value { font-size: 3rem; font-weight: 800; margin: 0; }
    .bpm-value { color: var(--accent-bpm); }
    .spo2-value { color: var(--accent-spo2); }
    .pi-value { color: var(--accent-pi); }
    .temp-value { color: var(--accent-temp); }
    .hrv-value { color: var(--accent-hrv); }
    .conf-value { color: var(--accent-pulse); }
    
    .chart-container { margin-top: 20px; height: 120px; position: relative; }
    .pulse-card { grid-column: 1 / -1; }
    .pulse-chart-container { height: 250px; position: relative; width: 100%; margin-top: 15px; }
    
    .controls {
      margin-top: 30px;
      width: 100%;
      max-width: 1000px;
      background: var(--card-bg);
      padding: 20px;
      border-radius: 16px;
      display: flex; flex-direction: column; align-items: center; gap: 15px;
    }
    .btn {
      background: #334155;
      color: white;
      border: none;
      padding: 10px 20px;
      border-radius: 8px;
      cursor: pointer;
      font-weight: 600;
      transition: all 0.2s;
    }
    .btn:hover { background: #475569; transform: translateY(-1px); }
    .btn-active { background: var(--accent-pulse); }

    #status { position: fixed; bottom: 20px; right: 20px; font-size: 0.75rem; color: var(--text-dim); }
    #transport-status { position: fixed; bottom: 40px; right: 20px; font-size: 0.75rem; color: var(--accent-pulse); }
  </style>
</head>
<body>
  <div class="header">
    <h1>VITAL SENSE</h1>
    <p style="color: var(--text-dim)">Real-time Health Monitoring</p>
  </div>

  <div class="status-bar">
    <div class="status-item">
      <div id="status-dot" class="status-dot"></div>
      <span class="status-text" id="sys-status">Initializing...</span>
    </div>
    <div class="status-item">
      <span class="rssi-value">Signal: <span id="rssi-val">--</span> dBm</span>
    </div>
  </div>

  <div class="container">
    <div class="card pulse-card">
      <div class="kpi">
        <span class="kpi-label">Pulse Waveform (PPG)</span>
      </div>
      <div class="pulse-chart-container">
        <canvas id="ppgChart"></canvas>
      </div>
    </div>
    
    <div class="card">
      <div class="kpi">
        <span class="kpi-label">Heart Rate</span>
        <h2 id="bpm" class="kpi-value bpm-value">--</h2>
        <span style="font-size: 1rem; color: var(--text-dim)">BPM</span>
      </div>
      <div class="chart-container">
        <canvas id="bpmChart"></canvas>
      </div>
    </div>
    
    <div class="card">
      <div class="kpi">
        <span class="kpi-label">Oxygen Saturation</span>
        <h2 id="spo2" class="kpi-value spo2-value">--</h2>
        <span style="font-size: 1rem; color: var(--text-dim)">% SpO2</span>
      </div>
      <div class="chart-container">
        <canvas id="spo2Chart"></canvas>
      </div>
    </div>

    <!-- Advanced Metrics -->
    <div class="card">
      <div class="kpi">
        <span class="kpi-label">HRV (RMSSD)</span>
        <h2 id="hrv" class="kpi-value hrv-value">--</h2>
        <span style="font-size: 0.9rem; color: var(--text-dim); text-align: center; margin-top: 5px;">ms (Recovery)</span>
      </div>
    </div>

    <div class="card">
      <div class="kpi">
        <span class="kpi-label">Perfusion Index</span>
        <h2 id="pi" class="kpi-value pi-value">--</h2>
        <span style="font-size: 0.9rem; color: var(--text-dim); text-align: center; margin-top: 5px;">Pulse Strength</span>
      </div>
    </div>

    <div class="card">
      <div class="kpi">
        <span class="kpi-label">Signal Quality</span>
        <h2 id="conf" class="kpi-value conf-value">--</h2>
        <span style="font-size: 0.9rem; color: var(--text-dim); text-align: center; margin-top: 5px;">% Confidence</span>
      </div>
    </div>

    <div class="card">
      <div class="kpi">
        <span class="kpi-label">Sensor Temp</span>
        <h2 id="temp" class="kpi-value temp-value">--</h2>
        <span style="font-size: 0.9rem; color: var(--text-dim); text-align: center; margin-top: 5px;">&deg;C (Die Temp)</span>
      </div>
    </div>
  </div>

  <div class="controls">
    <button id="serial-btn" class="btn">Connect via USB (Ultra-Low Latency)</button>
    <p id="serial-msg" style="font-size: 0.8rem; color: var(--text-dim); margin-top: 10px; text-align: center;">Fallback to WebSocket is active.</p>
  </div>

  <div id="transport-status">Mode: WebSocket</div>
  <div id="status">WebSocket: Disconnected</div>

  <script>
    const bpmEl = document.getElementById('bpm');
    const spo2El = document.getElementById('spo2');
    const hrvEl = document.getElementById('hrv');
    const confEl = document.getElementById('conf');
    const piEl = document.getElementById('pi');
    const tempEl = document.getElementById('temp');
    const sysStatusEl = document.getElementById('sys-status');
    const statusDot = document.getElementById('status-dot');
    const rssiEl = document.getElementById('rssi-val');
    const statusEl = document.getElementById('status');
    const transEl = document.getElementById('transport-status');
    const serialBtn = document.getElementById('serial-btn');
    const serialMsg = document.getElementById('serial-msg');

    let bpmChart, spo2Chart, ppgChart;
    let transportMode = 'ws';

    const chartConfig = (color, label, points = 20, sugMin = null, sugMax = null) => ({
      type: 'line',
      data: {
        labels: Array(points).fill(''),
        datasets: [{
          label: label,
          data: Array(points).fill(null),
          borderColor: color,
          borderWidth: 2,
          pointRadius: 0,
          tension: 0.4,
          fill: true,
          backgroundColor: color + '1A'
        }]
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        animation: false,
        plugins: { legend: { display: false } },
        scales: {
          x: { display: false },
          y: { 
            grid: { color: 'rgba(255,255,255,0.05)' }, 
            ticks: { color: '#94a3b8' },
            suggestedMin: sugMin,
            suggestedMax: sugMax
          }
        }
      }
    });

    function handleData(data) {
      if (data.ir !== undefined) {
         if (Array.isArray(data.ir)) {
           data.ir.forEach(val => updatePPG(ppgChart, val));
         } else {
           updatePPG(ppgChart, data.ir);
         }
      }
      if (data.bpm !== undefined) {
        bpmEl.textContent = data.bpm > 0 ? Math.round(data.bpm) : "--";
        if(data.bpm > 0) updateChart(bpmChart, data.bpm, true);
      }
      if (data.spo2 !== undefined) {
        spo2El.textContent = data.spo2 > 0 ? data.spo2.toFixed(1) : "--";
        if(data.spo2 > 0) updateChart(spo2Chart, data.spo2);
      }
      if (data.hrv !== undefined) {
        hrvEl.textContent = data.hrv > 0 ? Math.round(data.hrv) : "--";
      }
      if (data.pi !== undefined) {
        piEl.textContent = data.pi > 0 ? data.pi.toFixed(2) + "%" : "--";
      }
      if (data.temp !== undefined) {
        tempEl.textContent = data.temp > 0 ? data.temp.toFixed(1) : "--";
      }
      if (data.status !== undefined) {
        sysStatusEl.textContent = data.status;
        if (data.status === "Pulse Acquired") statusDot.style.background = "#10b981";
        else if (data.status === "Seeking Pulse") statusDot.style.background = "#f59e0b";
        else statusDot.style.background = "#94a3b8";
      }
      if (data.rssi !== undefined) {
        rssiEl.textContent = data.rssi;
      }
      if (data.conf !== undefined) {
        confEl.textContent = data.conf + "%";
        confEl.style.color = data.conf > 80 ? '#10b981' : (data.conf > 50 ? '#f59e0b' : '#ef4444');
      }
    }

    function updateChart(chart, value, autoScale = false) {
      if (!chart) return;
      chart.data.datasets[0].data.shift();
      chart.data.datasets[0].data.push(value);
      if (autoScale) {
        const data = chart.data.datasets[0].data.filter(v => v !== null);
        if (data.length > 2) {
          let min = Math.min(...data);
          let max = Math.max(...data);
          chart.options.scales.y.min = Math.max(0, Math.floor(min - 5));
          chart.options.scales.y.max = Math.ceil(max + 5);
        }
      }
      chart.update('none');
    }

    function updatePPG(chart, value) {
      if (!chart || value < 10000) return;
      chart.data.datasets[0].data.shift();
      chart.data.datasets[0].data.push(value);
      const data = chart.data.datasets[0].data.filter(v => v !== null);
      if (data.length > 5) {
        let min = Math.min(...data);
        let max = Math.max(...data);
        chart.options.scales.y.min = min - 300;
        chart.options.scales.y.max = max + 300;
      }
      chart.update('none');
    }

    let gateway = `ws://${window.location.hostname}/ws`;
    let websocket;

    function initWebSocket() {
      websocket = new WebSocket(gateway);
      websocket.onopen = (e) => {
        statusEl.textContent = 'WebSocket: Connected';
        statusEl.style.color = '#10b981';
      };
      websocket.onclose = (e) => {
        statusEl.textContent = 'WebSocket: Disconnected';
        statusEl.style.color = '#ef4444';
        setTimeout(initWebSocket, 2000);
      };
      websocket.onmessage = (event) => {
        if (transportMode === 'serial') return;
        try {
          handleData(JSON.parse(event.data));
        } catch (e) { console.error("Parse error:", e); }
      };
    }

    let serialPort;
    let serialReader;

    serialBtn.onclick = async () => {
      if (!('serial' in navigator)) {
        alert("Web Serial is disabled by your browser. Use HTTPS or toggle chrome://flags.");
        return;
      }
      if (transportMode === 'serial') { location.reload(); return; }
      try {
        serialPort = await navigator.serial.requestPort();
        await serialPort.open({ baudRate: 115200 });
        transportMode = 'serial';
        serialBtn.textContent = 'USB Connected (Click to Reset)';
        serialBtn.classList.add('btn-active');
        transEl.textContent = 'Mode: USB Serial';
        let decoder = new TextDecoderStream();
        serialPort.readable.pipeTo(decoder.writable);
        serialReader = decoder.readable.getReader();
        let partial = '';
        while (true) {
          const { value, done } = await serialReader.read();
          if (done) break;
          partial += value;
          let lines = partial.split('\n');
          partial = lines.pop();
          for (let line of lines) {
            if (line.trim().startsWith('>DATA:')) {
              try { handleData(JSON.parse(line.trim().substring(6))); } catch (e) {}
            }
          }
        }
      } catch (e) { console.error(e); }
    };

    window.onload = () => {
      bpmChart = new Chart(document.getElementById('bpmChart').getContext('2d'), chartConfig('#ef4444', 'BPM', 30));
      spo2Chart = new Chart(document.getElementById('spo2Chart').getContext('2d'), chartConfig('#3b82f6', 'SpO2', 30, 90, 100));
      ppgChart = new Chart(document.getElementById('ppgChart').getContext('2d'), chartConfig('#10b981', 'Pulse', 150, 80000, 120000));
      if (!('serial' in navigator)) {
        serialBtn.style.opacity = '0.5';
        serialMsg.innerHTML = '⚠️ Web Serial unavailable (Secure Context required).';
      }
      initWebSocket();
    };
  </script>
</body>
</html>
)rawpriority";
