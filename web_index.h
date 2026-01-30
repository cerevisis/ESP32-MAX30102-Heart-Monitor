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
    .container {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
      gap: 20px;
      width: 100%;
      max-width: 1000px;
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
    .chart-container { margin-top: 20px; height: 180px; position: relative; }
    .pulse-card { grid-column: 1 / -1; }
    .pulse-chart-container { height: 250px; position: relative; width: 100%; margin-top: 15px; }
    
    .controls {
      margin-top: 30px;
      width: 100%;
      max-width: 1000px;
      background: var(--card-bg);
      padding: 20px;
      border-radius: 16px;
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 15px;
    }
    .slider-container { width: 100%; display: flex; align-items: center; gap: 20px; }
    input[type=range] { flex: 1; cursor: pointer; accent-color: var(--accent-pulse); }

    #status { position: fixed; bottom: 20px; right: 20px; font-size: 0.75rem; color: var(--text-dim); }
  </style>
</head>
<body>
  <div class="header">
    <h1>VITAL SENSE</h1>
    <p style="color: var(--text-dim)">Real-time Health Monitoring</p>
  </div>

  <div class="container">
    <!-- Pulse Waveform Card -->
    <div class="card pulse-card">
      <div class="kpi">
        <span class="kpi-label">Pulse Waveform (PPG)</span>
      </div>
      <div class="pulse-chart-container">
        <canvas id="ppgChart"></canvas>
      </div>
    </div>

    <!-- Heart Rate Card -->
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

    <!-- SpO2 Card -->
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
  </div>

  <div class="controls">
    <span class="kpi-label">WebSocket Frequency: <span id="freq-val" style="color: var(--accent-pulse)">30</span> Hz</span>
    <div class="slider-container">
      <span>1Hz</span>
      <input type="range" id="freq-slider" min="1" max="100" value="30">
      <span>100Hz</span>
    </div>
  </div>

  <div id="status">WebSocket: Disconnected</div>

  <script>
    const bpmEl = document.getElementById('bpm');
    const spo2El = document.getElementById('spo2');
    const statusEl = document.getElementById('status');
    const freqEl = document.getElementById('freq-val');
    const freqSlider = document.getElementById('freq-slider');

    let bpmChart, spo2Chart, ppgChart;

    const chartConfig = (color, label, points = 20) => ({
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
            suggestedMin: 80000,
            suggestedMax: 120000
          }
        }
      }
    });

    function updateChart(chart, value) {
      if (!chart) return;
      chart.data.datasets[0].data.shift();
      chart.data.datasets[0].data.push(value);
      chart.update('none');
    }

    function updatePPG(chart, value) {
      if (!chart || value < 10000) return;
      chart.data.datasets[0].data.shift();
      chart.data.datasets[0].data.push(value);
      
      const data = chart.data.datasets[0].data.filter(v => v !== null);
      if (data.length > 0) {
        const min = Math.min(...data);
        const max = Math.max(...data);
        chart.options.scales.y.min = min - 200;
        chart.options.scales.y.max = max + 200;
      }
      chart.update('none');
    }

    let gateway = `ws://${window.location.hostname}/ws`;
    let websocket;

    function initWebSocket() {
      websocket = new WebSocket(gateway);
      websocket.onopen = (event) => {
        statusEl.textContent = 'WebSocket: Connected';
        statusEl.style.color = '#10b981';
      };
      websocket.onclose = (event) => {
        statusEl.textContent = 'WebSocket: Disconnected';
        statusEl.style.color = '#ef4444';
        setTimeout(initWebSocket, 2000);
      };
      websocket.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          if (data.ir !== undefined) {
            updatePPG(ppgChart, data.ir);
          }
          if (data.bpm !== undefined) {
            bpmEl.textContent = data.bpm > 0 ? Math.round(data.bpm) : "--";
            if(data.bpm > 0) updateChart(bpmChart, data.bpm);
          }
          if (data.spo2 !== undefined) {
            spo2El.textContent = data.spo2 > 0 ? data.spo2.toFixed(1) : "--";
            if(data.spo2 > 0) updateChart(spo2Chart, data.spo2);
          }
        } catch (e) { console.error("Parse error:", e); }
      };
      websocket.onerror = (error) => console.error('WS Error:', error);
    }

    freqSlider.oninput = () => {
      const val = freqSlider.value;
      freqEl.textContent = val;
      if (websocket && websocket.readyState == WebSocket.OPEN) {
        websocket.send(JSON.stringify({freq: parseInt(val)}));
      }
    };

    window.onload = () => {
      bpmChart = new Chart(document.getElementById('bpmChart').getContext('2d'), chartConfig('#ef4444', 'BPM', 30));
      spo2Chart = new Chart(document.getElementById('spo2Chart').getContext('2d'), chartConfig('#3b82f6', 'SpO2', 30));
      ppgChart = new Chart(document.getElementById('ppgChart').getContext('2d'), chartConfig('#10b981', 'Pulse', 100));
      initWebSocket();
    };
  </script>
</body>
</html>
)rawpriority";
