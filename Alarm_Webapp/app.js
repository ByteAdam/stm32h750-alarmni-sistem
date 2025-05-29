document.addEventListener('DOMContentLoaded', () => {
  const connectBtn = document.getElementById('connect-btn');
  const statusEl   = document.getElementById('status');
  const distanceEl = document.getElementById('distance');
  const logEl      = document.getElementById('log');
  const eventsEl   = document.getElementById('events');
  const exportSer  = document.getElementById('export-serial');
  const exportEvt  = document.getElementById('export-events');
  const leds = [
    document.getElementById('led1'),
    document.getElementById('led2'),
    document.getElementById('led3'),
    document.getElementById('led4')
  ];

  let port, reader;
  const enc = new TextEncoder();
  const dec = new TextDecoder();
  let buf = '';

  const serialData = [];
  const eventData  = [];
  const ts = () => new Date().toLocaleTimeString('sl-SI');
  const addEventLine = txt => {
    eventsEl.value += txt + '\n';
    eventsEl.scrollTop = eventsEl.scrollHeight;
    eventData.push({ time: ts(), text: txt });
  };
  const setBodyClass = cls => {
    document.body.className = 'neutral';
    if (cls) document.body.classList.add(cls);
  };

  connectBtn.addEventListener('click', async () => {
    try { port ? await disconnect() : await connect(); }
    catch (e) { alert(e.message || e); }
  });

  async function connect() {
    port = await navigator.serial.requestPort();
    await port.open({ baudRate: 115200 });
    reader = port.readable.getReader();
    statusEl.textContent   = 'Povezano';
    connectBtn.textContent = 'Prekini';
    setBodyClass('neutral');
    readLoop();
  }
  async function disconnect() {
    await reader.cancel(); reader.releaseLock();
    await port.close(); port = null;
    statusEl.textContent   = 'Odklopljeno';
    connectBtn.textContent = 'Poveži';
    setBodyClass('neutral');
  }

  let flashTimer = null;
  async function readLoop() {
    while (true) {
      const { value, done } = await reader.read();
      if (done) break;
      buf += dec.decode(value, { stream: true });
      let ix;
      while ((ix = buf.indexOf('\n')) !== -1) {
        const line = buf.slice(0, ix).trim();
        buf = buf.slice(ix + 1);
        if (!line) continue;

        // Serijski dnevnik
        logEl.value += line + '\n';
        logEl.scrollTop = logEl.scrollHeight;
        serialData.push({ time: ts(), text: line });

        // Razdalja
        const mDist = line.match(/^US:\s*(\d+)\s*cm$/i);
        if (mDist) distanceEl.textContent = mDist[1];

        // LED status
        const mLed = line.match(/^LED(\d):\s*(ON|OFF)$/i);
        if (mLed) leds[+mLed[1] - 1].classList.toggle('off', mLed[2] === 'OFF');

        // Dogodki
        if (line.startsWith('ALARM_SPROZEN')) {
          addEventLine(`${ts()}: Alarm sprožen`);
          statusEl.textContent = 'Odštevanje';
          setBodyClass('flash-red');
          flashTimer = setTimeout(() => setBodyClass('alarm-orange'), 5000);
        }
        else if (line.startsWith('ALARM_NOTIFIED')) {
          addEventLine(`${ts()}: Alarmna agencija obveščena`);
          if (flashTimer) { clearTimeout(flashTimer); flashTimer = null; }
          setBodyClass('alarm-red');
        }
        else if (line.startsWith('ALARM_DEAKTIVIRAN')) {
          addEventLine(`${ts()}: Alarm deaktiviran`);
          if (flashTimer) { clearTimeout(flashTimer); flashTimer = null; }
          setBodyClass('alarm-green');
          statusEl.textContent = 'Deaktivirano (10 s)';
          setTimeout(() => setBodyClass('neutral'), 10000);
        }
        else if (line.startsWith('NAPACNA_KODA')) {
          addEventLine(`${ts()}: Napačna koda – števci ponastavljeni`);
        }
        else if (line.startsWith('Ready')) {
          statusEl.textContent = 'Pripravljen';
        }
      }
    }
  }

  // CSV izvoz
  const downloadCSV = (name, rows) => {
    const csv = ['čas,sporočilo',
      ...rows.map(r => `"${r.time}","${r.text.replace(/"/g,'""')}"`)
    ].join('\n');
    const url = URL.createObjectURL(new Blob([csv], { type:'text/csv' }));
    const a   = Object.assign(document.createElement('a'),
                  { href:url, download:name });
    document.body.appendChild(a); a.click(); a.remove();
    URL.revokeObjectURL(url);
  };
  exportSer.addEventListener('click', () => downloadCSV('serijski.csv', serialData));
  exportEvt.addEventListener('click', () => downloadCSV('dogodki.csv',  eventData));
});
