const bcf = document.getElementById("bandChannelFreq");
const bandSelect = document.getElementById("bandSelect");
const channelSelect = document.getElementById("channelSelect");
const freqOutput = document.getElementById("freqOutput");
const announcerSelect = document.getElementById("announcerSelect");
const announcerRateInput = document.getElementById("rate");
const enterRssiInput = document.getElementById("enter");
const exitRssiInput = document.getElementById("exit");
const enterRssiSpan = document.getElementById("enterSpan");
const exitRssiSpan = document.getElementById("exitSpan");
const pilotNameInput = document.getElementById("pname");
const ssidInput = document.getElementById("ssid");
const pwdInput = document.getElementById("pwd");
const minLapInput = document.getElementById("minLap");
const alarmThreshold = document.getElementById("alarmThreshold");

const freqLookup = [
  [5865, 5845, 5825, 5805, 5785, 5765, 5745, 5725],
  [5733, 5752, 5771, 5790, 5809, 5828, 5847, 5866],
  [5705, 5685, 5665, 5645, 5885, 5905, 5925, 5945],
  [5740, 5760, 5780, 5800, 5820, 5840, 5860, 5880],
  [5658, 5695, 5732, 5769, 5806, 5843, 5880, 5917],
  [5362, 5399, 5436, 5473, 5510, 5547, 5584, 5621],
];

const config = document.getElementById("config");
const race = document.getElementById("race");
const calib = document.getElementById("calib");
const ota = document.getElementById("ota");

var enterRssi = 120,
  exitRssi = 100;
var frequency = 0;
var announcerRate = 1.0;

var lapNo = -1;
var lapTimes = [];

var timerInterval;
const timer = document.getElementById("timer");
const startRaceButton = document.getElementById("startRaceButton");
const stopRaceButton = document.getElementById("stopRaceButton");

const batteryVoltageDisplay = document.getElementById("bvolt");

const rssiBuffer = [];
var rssiValue = 0;
var rssiSending = false;
var rssiChart;
var crossing = false;
var rssiSeries = new TimeSeries();
var rssiCrossingSeries = new TimeSeries();
var maxRssiValue = enterRssi + 10;
var minRssiValue = exitRssi - 10;

var audioEnabled = false;
var speakObjsQueue = [];

onload = function (e) {
  config.style.display = "block";
  race.style.display = "none";
  calib.style.display = "none";
  ota.style.display = "none";
  fetch("/config")
    .then((response) => response.json())
    .then((config) => {
      console.log(config);
      setBandChannelIndex(config.freq);
      minLapInput.value = (parseFloat(config.minLap) / 10).toFixed(1);
      updateMinLap(minLapInput, minLapInput.value);
      alarmThreshold.value = (parseFloat(config.alarm) / 10).toFixed(1);
      updateAlarmThreshold(alarmThreshold, alarmThreshold.value);
      announcerSelect.selectedIndex = config.anType;
      announcerRateInput.value = (parseFloat(config.anRate) / 10).toFixed(1);
      updateAnnouncerRate(announcerRateInput, announcerRateInput.value);
      enterRssiInput.value = config.enterRssi;
      updateEnterRssi(enterRssiInput, enterRssiInput.value);
      exitRssiInput.value = config.exitRssi;
      updateExitRssi(exitRssiInput, exitRssiInput.value);
      pilotNameInput.value = config.name;
      ssidInput.value = config.ssid;
      pwdInput.value = config.pwd;
      populateFreqOutput();
      stopRaceButton.disabled = true;
      startRaceButton.disabled = false;
      clearInterval(timerInterval);
      timer.innerHTML = "00:00:00s";
      clearLaps();
      createRssiChart();
    });
};

function getBatteryVoltage() {
  fetch("/status")
    .then((response) => response.text())
    .then((response) => {
      const batteryVoltageMatch = response.match(/Battery Voltage:\s*([\d.]+v)/);
      const batteryVoltage = batteryVoltageMatch ? batteryVoltageMatch[1] : null;
      batteryVoltageDisplay.innerText = batteryVoltage;
    });
}

setInterval(getBatteryVoltage, 2000);

function addRssiPoint() {
  if (calib.style.display != "none") {
    rssiChart.start();
    if (rssiBuffer.length > 0) {
      rssiValue = parseInt(rssiBuffer.shift());
      if (crossing && rssiValue < exitRssi) {
        crossing = false;
      } else if (!crossing && rssiValue > enterRssi) {
        crossing = true;
      }
      maxRssiValue = Math.max(maxRssiValue, rssiValue);
      minRssiValue = Math.min(minRssiValue, rssiValue);
    }

    // update horizontal lines and min max values
    rssiChart.options.horizontalLines = [
      { color: "hsl(8.2, 86.5%, 53.7%)", lineWidth: 1.7, value: enterRssi }, // red
      { color: "hsl(25, 85%, 55%)", lineWidth: 1.7, value: exitRssi }, // orange
    ];

    rssiChart.options.maxValue = Math.max(maxRssiValue, enterRssi + 10);

    rssiChart.options.minValue = Math.max(0, Math.min(minRssiValue, exitRssi - 10));

    var now = Date.now();
    rssiSeries.append(now, rssiValue);
    if (crossing) {
      rssiCrossingSeries.append(now, 256);
    } else {
      rssiCrossingSeries.append(now, -10);
    }
  } else {
    rssiChart.stop();
    maxRssiValue = enterRssi + 10;
    minRssiValue = exitRssi - 10;
  }
}

setInterval(addRssiPoint, 200);

function createRssiChart() {
  rssiChart = new SmoothieChart({
    responsive: true,
    millisPerPixel: 50,
    grid: {
      strokeStyle: "rgba(255,255,255,0.25)",
      sharpLines: true,
      verticalSections: 0,
      borderVisible: false,
    },
    labels: {
      precision: 0,
    },
    maxValue: 1,
    minValue: 0,
  });
  rssiChart.addTimeSeries(rssiSeries, {
    lineWidth: 1.7,
    strokeStyle: "hsl(214, 53%, 60%)",
    fillStyle: "hsla(214, 53%, 60%, 0.4)",
  });
  rssiChart.addTimeSeries(rssiCrossingSeries, {
    lineWidth: 1.7,
    strokeStyle: "none",
    fillStyle: "hsla(136, 71%, 70%, 0.3)",
  });
  rssiChart.streamTo(document.getElementById("rssiChart"), 200);
}

function openTab(evt, tabName) {
  // Declare all variables
  var i, tabcontent, tablinks;

  // Get all elements with class="tabcontent" and hide them
  tabcontent = document.getElementsByClassName("tabcontent");
  for (i = 0; i < tabcontent.length; i++) {
    tabcontent[i].style.display = "none";
  }

  // Get all elements with class="tablinks" and remove the class "active"
  tablinks = document.getElementsByClassName("tablinks");
  for (i = 0; i < tablinks.length; i++) {
    tablinks[i].className = tablinks[i].className.replace(" active", "");
  }

  // Show the current tab, and add an "active" class to the button that opened the tab
  document.getElementById(tabName).style.display = "block";
  evt.currentTarget.className += " active";

  // if event comes from calibration tab, signal to start sending RSSI events
  if (tabName === "calib" && !rssiSending) {
    fetch("/timer/rssiStart", {
      method: "POST",
      headers: {
        Accept: "application/json",
        "Content-Type": "application/json",
      },
    })
      .then((response) => {
        if (response.ok) rssiSending = true;
        return response.json();
      })
      .then((response) => console.log("/timer/rssiStart:" + JSON.stringify(response)));
  } else if (rssiSending) {
    fetch("/timer/rssiStop", {
      method: "POST",
      headers: {
        Accept: "application/json",
        "Content-Type": "application/json",
      },
    })
      .then((response) => {
        if (response.ok) rssiSending = false;
        return response.json();
      })
      .then((response) => console.log("/timer/rssiStop:" + JSON.stringify(response)));
  }
}

function updateEnterRssi(obj, value) {
  enterRssi = parseInt(value);
  enterRssiSpan.textContent = enterRssi;
  if (enterRssi <= exitRssi) {
    exitRssi = Math.max(0, enterRssi - 1);
    exitRssiInput.value = exitRssi;
    exitRssiSpan.textContent = exitRssi;
  }
}

function updateExitRssi(obj, value) {
  exitRssi = parseInt(value);
  exitRssiSpan.textContent = exitRssi;
  if (exitRssi >= enterRssi) {
    enterRssi = Math.min(255, exitRssi + 1);
    enterRssiInput.value = enterRssi;
    enterRssiSpan.textContent = enterRssi;
  }
}

function saveConfig() {
  fetch("/config", {
    method: "POST",
    headers: {
      Accept: "application/json",
      "Content-Type": "application/json",
    },
    body: JSON.stringify({
      freq: frequency,
      minLap: parseInt(minLapInput.value * 10),
      alarm: parseInt(alarmThreshold.value * 10),
      anType: announcerSelect.selectedIndex,
      anRate: parseInt(announcerRate * 10),
      enterRssi: enterRssi,
      exitRssi: exitRssi,
      name: pilotNameInput.value,
      ssid: ssidInput.value,
      pwd: pwdInput.value,
    }),
  })
    .then((response) => response.json())
    .then((response) => console.log("/config:" + JSON.stringify(response)));
}

function populateFreqOutput() {
  let band = bandSelect.options[bandSelect.selectedIndex].value;
  let chan = channelSelect.options[channelSelect.selectedIndex].value;
  frequency = freqLookup[bandSelect.selectedIndex][channelSelect.selectedIndex];
  freqOutput.textContent = band + chan + " " + frequency;
}

bcf.addEventListener("change", function handleChange(event) {
  populateFreqOutput();
});

function updateAnnouncerRate(obj, value) {
  announcerRate = parseFloat(value);
  $(obj).parent().find("span").text(announcerRate.toFixed(1));
}

function updateMinLap(obj, value) {
  $(obj)
    .parent()
    .find("span")
    .text(parseFloat(value).toFixed(1) + "s");
}

function updateAlarmThreshold(obj, value) {
  $(obj)
    .parent()
    .find("span")
    .text(parseFloat(value).toFixed(1) + "v");
}

// function getAnnouncerVoices() {
//   $().articulate("getVoices", "#voiceSelect", "System Default Announcer Voice");
// }

function beep(duration, frequency, type) {
  var context = new AudioContext();
  var oscillator = context.createOscillator();
  oscillator.type = type;
  oscillator.frequency.value = frequency;
  oscillator.connect(context.destination);
  oscillator.start();
  // Beep for 500 milliseconds
  setTimeout(function () {
    oscillator.stop();
  }, duration);
}

function addLap(lapStr) {
  const pilotName = pilotNameInput.value;
  var last2lapStr = "";
  var last3lapStr = "";
  const newLap = parseFloat(lapStr);
  lapNo += 1;
  const table = document.getElementById("lapTable");
  const row = table.insertRow();
  const cell1 = row.insertCell(0);
  const cell2 = row.insertCell(1);
  const cell3 = row.insertCell(2);
  const cell4 = row.insertCell(3);
  cell1.innerHTML = lapNo;
  if (lapNo == 0) {
    cell2.innerHTML = "Hole Shot: " + lapStr + "s";
  } else {
    cell2.innerHTML = lapStr + "s";
  }
  if (lapTimes.length >= 2 && lapNo != 0) {
    last2lapStr = (newLap + lapTimes[lapTimes.length - 1]).toFixed(2);
    cell3.innerHTML = last2lapStr + "s";
  }
  if (lapTimes.length >= 3 && lapNo != 0) {
    last3lapStr = (newLap + lapTimes[lapTimes.length - 2] + lapTimes[lapTimes.length - 1]).toFixed(2);
    cell4.innerHTML = last3lapStr + "s";
  }

  switch (announcerSelect.options[announcerSelect.selectedIndex].value) {
    case "beep":
      beep(100, 330, "square");
      break;
    case "1lap":
      if (lapNo == 0) {
        queueSpeak(`<p>Hole Shot ${lapStr}<p>`);
      } else {
        const lapNoStr = pilotName + " Lap " + lapNo + ", ";
        const text = "<p>" + lapNoStr + lapStr + "</p>";
        queueSpeak(text);
      }
      break;
    case "2lap":
      if (lapNo == 0) {
        queueSpeak(`<p>Hole Shot ${lapStr}<p>`);
      } else if (last2lapStr != "") {
        const text2 = "<p>" + pilotName + " 2 laps " + last2lapStr + "</p>";
        queueSpeak(text2);
      }
      break;
    case "3lap":
      if (lapNo == 0) {
        queueSpeak(`<p>Hole Shot ${lapStr}<p>`);
      } else if (last3lapStr != "") {
        const text3 = "<p>" + pilotName + " 3 laps " + last3lapStr + "</p>";
        queueSpeak(text3);
      }
      break;
    default:
      break;
  }
  lapTimes.push(newLap);
}

function startTimer() {
  var millis = 0;
  var seconds = 0;
  var minutes = 0;
  timerInterval = setInterval(function () {
    millis += 1;

    if (millis == 100) {
      millis = 0;
      seconds++;

      if (seconds == 60) {
        seconds = 0;
        minutes++;

        if (minutes == 60) {
          minutes = 0;
        }
      }
    }
    let m = minutes < 10 ? "0" + minutes : minutes;
    let s = seconds < 10 ? "0" + seconds : seconds;
    let ms = millis < 10 ? "0" + millis : millis;
    timer.innerHTML = `${m}:${s}:${ms}s`;
  }, 10);

  fetch("/timer/start", {
    method: "POST",
    headers: {
      Accept: "application/json",
      "Content-Type": "application/json",
    },
  })
    .then((response) => response.json())
    .then((response) => console.log("/timer/start:" + JSON.stringify(response)));
}

function queueSpeak(obj) {
  if (!audioEnabled) {
    return;
  }
  speakObjsQueue.push(obj);
}

async function enableAudioLoop() {
  audioEnabled = true;
  while(audioEnabled) {
    if (speakObjsQueue.length > 0) {
      let isSpeakingFlag = $().articulate('isSpeaking');
      if (!isSpeakingFlag) {
        let obj = speakObjsQueue.shift();
        doSpeak(obj);
      }
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}

function disableAudioLoop() {
  audioEnabled = false;
}
function generateAudio() {
  if (!audioEnabled) {
    return;
  }

  const pilotName = pilotNameInput.value;
  queueSpeak('<div>testing sound for pilot ' + pilotName + '</div>');
  for (let i = 1; i <= 3; i++) {
    queueSpeak('<div>' + i + '</div>')
  }
}

function doSpeak(obj) {
  $(obj).articulate("rate", announcerRate).articulate('speak');
}

async function startRace() {
  startRaceButton.disabled = true;
  // Calculate time taken to say starting phrase
  const baseWordsPerMinute = 150;
  let baseWordsPerSecond = baseWordsPerMinute / 60;
  let wordsPerSecond = baseWordsPerSecond * announcerRate;
  // 3 words in "Arm your quad"
  let timeToSpeak1 = 3 / wordsPerSecond * 1000; 
  queueSpeak("<p>Arm your quad</p>");
  await new Promise((r) => setTimeout(r, timeToSpeak1));
  // 8 words in "Starting on the tone in less than five"
  let timeToSpeak2 = 8 / wordsPerSecond * 1000; 
  queueSpeak("<p>Starting on the tone in less than five</p>");
  // Random start time between 1 and 5 seconds
  // Accounts for time taken to make previous announcement
  let delayTime = (Math.random() * (5000 - 1000)) + timeToSpeak2;
  await new Promise((r) => setTimeout(r, delayTime));
  beep(1, 1, "square"); // needed for some reason to make sure we fire the first beep
  beep(500, 880, "square");
  startTimer();
  stopRaceButton.disabled = false;
}

function stopRace() {
  queueSpeak('<p>Race stopped</p>');
  clearInterval(timerInterval);
  timer.innerHTML = "00:00:00s";

  fetch("/timer/stop", {
    method: "POST",
    headers: {
      Accept: "application/json",
      "Content-Type": "application/json",
    },
  })
    .then((response) => response.json())
    .then((response) => console.log("/timer/stop:" + JSON.stringify(response)));

  stopRaceButton.disabled = true;
  startRaceButton.disabled = false;

  lapNo = -1;
  lapTimes = [];
}

function clearLaps() {
  var tableHeaderRowCount = 1;
  var rowCount = lapTable.rows.length;
  for (var i = tableHeaderRowCount; i < rowCount; i++) {
    lapTable.deleteRow(tableHeaderRowCount);
  }
  lapNo = -1;
  lapTimes = [];
}

if (!!window.EventSource) {
  var source = new EventSource("/events");

  source.addEventListener(
    "open",
    function (e) {
      console.log("Events Connected");
    },
    false
  );

  source.addEventListener(
    "error",
    function (e) {
      if (e.target.readyState != EventSource.OPEN) {
        console.log("Events Disconnected");
      }
    },
    false
  );

  source.addEventListener(
    "rssi",
    function (e) {
      rssiBuffer.push(e.data);
      if (rssiBuffer.length > 10) {
        rssiBuffer.shift();
      }
      console.log("rssi", e.data, "buffer size", rssiBuffer.length);
    },
    false
  );

  source.addEventListener(
    "lap",
    function (e) {
      var lap = (parseFloat(e.data) / 1000).toFixed(2);
      addLap(lap);
      console.log("lap raw:", e.data, " formatted:", lap);
    },
    false
  );

  // Обробник countdown біпів
  source.addEventListener(
    "countdown",
    function (e) {
      var countNumber = parseInt(e.data);
      console.log("Countdown beep:", countNumber);
      
      // Генеруємо короткий біп 500Hz на 250мс (як на buzzer)
      beep(250, 500, "sine");
      
      // Без TTS озвучки - тільки біп
    },
    false
  );

  // Обробник звуку старту гонки
  source.addEventListener(
    "race",
    function (e) {
      if (e.data === "start") {
        console.log("Race start!");
        
        // Генеруємо звук старту 800Hz на 500мс (як на buzzer)
        beep(500, 800, "sine");
        
        // Запускаємо таймер на сторінці (без TTS озвучки)
        startTimer();
      } else if (e.data === "finish") {
        console.log("Race finish!");
        
        // Генеруємо звук зупинки 800Hz на 500мс
        beep(500, 800, "sine");
        
        // Зупиняємо таймер та озвучуємо FINISH
        stopTimer();
        queueSpeak(`<div>FINISH</div>`);
      }
    },
    false
  );

  // Обробник фіксації кола
  source.addEventListener(
    "lapComplete",
    function (e) {
      var data = JSON.parse(e.data);
      var lapNumber = data.lap;
      var lapTime = data.time;
      var timeInSeconds = (lapTime / 1000).toFixed(2);
      
      console.log("Lap complete:", lapNumber, "Time:", timeInSeconds);
      
      // Генеруємо біп фіксації кола 500Hz на 250мс
      beep(250, 500, "sine");
      
      // Озвучуємо результат: "Lap 1, 4.23"
      var seconds = Math.floor(timeInSeconds);
      var hundredths = Math.round((timeInSeconds - seconds) * 100);
      var announcement = `Lap ${lapNumber}, ${seconds} ${hundredths}`;
      queueSpeak(`<div>${announcement}</div>`);
      
      // Додаємо коло до таблиці (використовуємо існуючу функцію)
      addLap(timeInSeconds);
    },
    false
  );

  // Обробник попередження про низький заряд батареї
  source.addEventListener(
    "batteryWarning",
    function (e) {
      var data = JSON.parse(e.data);
      var voltage = data.voltage;
      var percentage = data.percentage;
      
      console.log("Battery warning:", voltage + "V (" + percentage + "%)");
      
      // Генеруємо довгий попереджувальний біп
      beep(1000, 300, "sine");
      setTimeout(() => beep(1000, 300, "sine"), 300);
      
      // Озвучуємо рівень заряду: "Battery 8%"
      var announcement = `Battery ${percentage} percent`;
      queueSpeak(`<div>${announcement}</div>`);
      
      // Оновлюємо індикатор батареї на сторінці (якщо відкрита WiFi вкладка)
      if (document.getElementById('batteryVoltage')) {
        document.getElementById('batteryVoltage').textContent = voltage + 'V';
        document.getElementById('batteryPercentage').textContent = percentage + '%';
        
        // Оновлюємо візуальний індикатор
        const batteryFill = document.getElementById('batteryFill');
        if (batteryFill) {
          batteryFill.style.width = percentage + '%';
          batteryFill.className = 'battery-fill battery-critical';
        }
      }
    },
    false
  );
}

function setBandChannelIndex(freq) {
  for (var i = 0; i < freqLookup.length; i++) {
    for (var j = 0; j < freqLookup[i].length; j++) {
      if (freqLookup[i][j] == freq) {
        bandSelect.selectedIndex = i;
        channelSelect.selectedIndex = j;
      }
    }
  }
}

// WiFi Management Functions
function initWiFiTab() {
  // Handle WiFi mode radio buttons
  const wifiModeRadios = document.querySelectorAll('input[name="wifiModeSelect"]');
  const clientConfig = document.getElementById('clientConfig');
  const apConfig = document.getElementById('apConfig');
  
  wifiModeRadios.forEach(radio => {
    radio.addEventListener('change', function() {
      if (this.value === 'STA') {
        clientConfig.style.display = 'block';
        apConfig.style.display = 'none';
      } else {
        clientConfig.style.display = 'none';
        apConfig.style.display = 'block';
      }
    });
  });
}

// Load WiFi status and apply settings
function loadWiFiStatus() {
  fetch('/api/wifi/status')
    .then(response => response.json())
    .then(data => {
      document.getElementById('wifiMode').textContent = data.mode === 'AP' ? 'Access Point' : 'Client';
      document.getElementById('currentSSID').textContent = data.ssid;
      document.getElementById('ipAddress').textContent = data.ip;
      document.getElementById('signalStrength').textContent = data.signal || 'N/A';
      
      // Set radio button based on current mode
      const modeRadio = document.querySelector(`input[value="${data.mode}"]`);
      if (modeRadio) {
        modeRadio.checked = true;
        modeRadio.dispatchEvent(new Event('change'));
      }
      
      // Fill current settings
      if (data.mode === 'AP') {
        document.getElementById('apSSID').value = data.ssid;
      } else {
        document.getElementById('ssid').value = data.ssid;
      }
    })
    .catch(error => {
      console.error('Failed to load WiFi status:', error);
    });
}

// Battery status loading function
function loadBatteryStatus() {
  fetch('/api/battery/status')
    .then(response => response.json())
    .then(data => {
      const voltage = data.voltage;
      const percentage = data.stepPercentage;
      
      // Оновлюємо глобальний footer індикатор (завжди)
      updateFooterBatteryIndicator(voltage, percentage);
      
      // Оновлюємо WiFi вкладку тільки якщо вона відкрита
      const batteryVoltageEl = document.getElementById('batteryVoltage');
      if (batteryVoltageEl) {
        batteryVoltageEl.textContent = voltage + 'V';
        document.getElementById('batteryPercentage').textContent = percentage + '%';
        
        const batteryFill = document.getElementById('batteryFill');
        if (batteryFill) {
          updateBatteryFillIndicator(batteryFill, percentage);
        }
      }
    })
    .catch(error => {
      console.error('Failed to load battery status:', error);
      // Fallback values для footer
      const footerBatteryText = document.getElementById('footerBatteryText');
      if (footerBatteryText) {
        footerBatteryText.textContent = 'N/A';
      }
    });
}

// Оновлення footer індикатора батареї
function updateFooterBatteryIndicator(voltage, percentage) {
  const footerBatteryText = document.getElementById('footerBatteryText');
  const footerBatteryFill = document.getElementById('footerBatteryFill');
  
  if (footerBatteryText && footerBatteryFill) {
    footerBatteryText.textContent = `${voltage}V (${percentage}%)`;
    updateBatteryFillIndicator(footerBatteryFill, percentage);
  }
}

// Універсальна функція для оновлення індикатора заливки батареї
function updateBatteryFillIndicator(fillElement, percentage) {
  fillElement.style.width = percentage + '%';
  
  // Встановлюємо колір залежно від рівня заряду
  fillElement.className = 'battery-fill';
  if (percentage >= 75) {
    fillElement.classList.add('battery-full');
  } else if (percentage >= 50) {
    fillElement.classList.add('battery-high');
  } else if (percentage >= 25) {
    fillElement.classList.add('battery-medium');
  } else if (percentage > 0) {
    fillElement.classList.add('battery-low');
  } else {
    fillElement.classList.add('battery-critical');
  }
}

function scanWifiNetworks() {
  const scanButton = event.target;
  const networkList = document.getElementById('networkList');
  const scanResults = document.getElementById('wifiScanResults');
  
  scanButton.textContent = 'Scanning...';
  scanButton.disabled = true;
  
  fetch('/api/wifi/scan')
    .then(response => response.json())
    .then(data => {
      networkList.innerHTML = '';
      
      if (data.networks && data.networks.length > 0) {
        data.networks.forEach(network => {
          const networkDiv = document.createElement('div');
          networkDiv.className = 'network-item';
          networkDiv.onclick = () => selectNetwork(network.ssid);
          
          networkDiv.innerHTML = `
            <span>${network.ssid}</span>
            <div>
              <span class="network-signal">Signal: ${network.rssi}dBm</span>
              ${network.secure ? '<span class="network-secure">🔒</span>' : ''}
            </div>
          `;
          
          networkList.appendChild(networkDiv);
        });
        
        scanResults.style.display = 'block';
      } else {
        networkList.innerHTML = '<div style="padding: 10px; text-align: center;">No networks found</div>';
        scanResults.style.display = 'block';
      }
    })
    .catch(error => {
      console.error('WiFi scan failed:', error);
      networkList.innerHTML = '<div style="padding: 10px; text-align: center; color: red;">Scan failed</div>';
      scanResults.style.display = 'block';
    })
    .finally(() => {
      scanButton.textContent = 'Scan Networks';
      scanButton.disabled = false;
    });
}

function selectNetwork(ssid) {
  document.getElementById('ssid').value = ssid;
}

function saveWifiConfig() {
  const mode = document.querySelector('input[name="wifiModeSelect"]:checked').value;
  const ssid = document.getElementById('ssid').value;
  const password = document.getElementById('pwd').value;
  const apPassword = document.getElementById('apPassword').value;
  
  const config = {
    mode: mode,
    ssid: ssid,
    password: password,
    apPassword: apPassword
  };
  
  if (mode === 'STA' && !ssid.trim()) {
    alert('Please enter WiFi network name (SSID)');
    return;
  }
  
  const saveButton = event.target;
  saveButton.textContent = 'Saving...';
  saveButton.disabled = true;
  
  fetch('/api/wifi/config', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json'
    },
    body: JSON.stringify(config)
  })
  .then(response => response.json())
  .then(data => {
    if (data.success) {
      alert('WiFi configuration saved! Device will restart in 3 seconds...');
      setTimeout(() => {
        restartDevice();
      }, 3000);
    } else {
      alert('Failed to save WiFi configuration: ' + data.error);
    }
  })
  .catch(error => {
    console.error('Failed to save WiFi config:', error);
    alert('Failed to save WiFi configuration');
  })
  .finally(() => {
    saveButton.textContent = 'Save & Apply WiFi Settings';
    saveButton.disabled = false;
  });
}

function resetWifiConfig() {
  if (confirm('Reset WiFi settings to default (Access Point mode)?')) {
    fetch('/api/wifi/reset', { method: 'POST' })
      .then(response => response.json())
      .then(data => {
        if (data.success) {
          alert('WiFi settings reset! Device will restart...');
          setTimeout(() => {
            restartDevice();
          }, 2000);
        }
      })
      .catch(error => {
        console.error('Failed to reset WiFi:', error);
      });
  }
}

function restartDevice() {
  if (confirm('Restart device? This will interrupt current connection.')) {
    fetch('/api/system/restart', { method: 'POST' })
      .then(() => {
        alert('Device is restarting... Please reconnect in 30 seconds.');
      })
      .catch(error => {
        console.log('Restart command sent');
      });
  }
}

// Initialize WiFi tab when page loads
document.addEventListener('DOMContentLoaded', function() {
  initWiFiTab();
  
  // Завантажуємо статус WiFi при завантаженні
  loadWiFiStatus();
  
  // Завантажуємо статус батареї при завантаженні
  loadBatteryStatus();
  
  // Оновлюємо статус батареї кожну хвилину
  setInterval(loadBatteryStatus, 60000);
  
  // Initialize network tab
  initNetworkTab();
  
  // Auto-refresh nodes table in Master mode every 10 seconds
  setInterval(() => {
    if (document.getElementById('deviceMode') && 
        document.getElementById('deviceMode').value == '1' && 
        document.getElementById('masterConfig') &&
        document.getElementById('masterConfig').style.display !== 'none') {
      loadRegisteredNodes();
    }
  }, 10000);
});

// Master-Slave Network Functions
function initNetworkTab() {
  const deviceModeSelect = document.getElementById('deviceMode');
  if (!deviceModeSelect) return; // Network tab not loaded yet
  
  const masterConfig = document.getElementById('masterConfig');
  const slaveConfig = document.getElementById('slaveConfig');
  const standaloneConfig = document.getElementById('standaloneConfig');
  
  // Handle device mode changes
  deviceModeSelect.addEventListener('change', function() {
    const mode = parseInt(this.value);
    
    // Hide all configs first
    masterConfig.style.display = 'none';
    slaveConfig.style.display = 'none';
    standaloneConfig.style.display = 'none';
    
    // Show appropriate config
    switch(mode) {
      case 0: // Standalone
        standaloneConfig.style.display = 'block';
        break;
      case 1: // Master
        masterConfig.style.display = 'block';
        loadRegisteredNodes();
        break;
      case 2: // Slave
        slaveConfig.style.display = 'block';
        break;
    }
  });
  
  // Test master connection button
  const testButton = document.getElementById('testMasterConnection');
  if (testButton) {
    testButton.addEventListener('click', testMasterConnection);
  }
  
  // Save network config button
  const saveButton = document.getElementById('saveNetworkConfig');
  if (saveButton) {
    saveButton.addEventListener('click', saveNetworkConfig);
  }
  
  // Reset network config button
  const resetButton = document.getElementById('resetNetworkConfig');
  if (resetButton) {
    resetButton.addEventListener('click', resetNetworkConfig);
  }
  
  // Load current configuration
  loadNetworkConfig();
}

function loadNetworkConfig() {
  fetch('/config')
    .then(response => response.json())
    .then(data => {
      const deviceMode = document.getElementById('deviceMode');
      const masterIP = document.getElementById('masterIP');
      const nodeChannel = document.getElementById('nodeChannel');
      
      if (deviceMode) {
        deviceMode.value = data.deviceMode || 0;
        deviceMode.dispatchEvent(new Event('change'));
      }
      
      if (masterIP) {
        masterIP.value = data.masterIP || '192.168.4.1';
      }
      
      if (nodeChannel) {
        nodeChannel.value = data.nodeChannel || 1;
      }
    })
    .catch(error => console.error('Failed to load network config:', error));
}

function loadRegisteredNodes() {
  fetch('/api/nodes/list')
    .then(response => response.json())
    .then(data => {
      const table = document.querySelector('#nodesTable table');
      const nodeCountEl = document.getElementById('nodeCount');
      if (!table) return;
      
      // Clear existing rows (except header)
      const rows = table.querySelectorAll('tr:not(:first-child)');
      rows.forEach(row => row.remove());
      
      const nodeCount = data.nodes ? data.nodes.length : 0;
      if (nodeCountEl) {
        nodeCountEl.textContent = nodeCount;
        nodeCountEl.style.color = nodeCount >= 7 ? '#dc3545' : '#28a745';
      }
      
      if (data.nodes && data.nodes.length > 0) {
        data.nodes.forEach(node => {
          const row = table.insertRow();
          row.innerHTML = `
            <td>${node.nodeId}</td>
            <td><a href="http://${node.ipAddress}" target="_blank">${node.ipAddress}</a></td>
            <td>Channel ${node.channel}</td>
            <td><span class="status-${node.isActive ? 'active' : 'inactive'}">${node.isActive ? 'Active' : 'Inactive'}</span></td>
            <td>${node.totalLaps > 0 ? (node.lastLapTime/1000).toFixed(3) + 's' : 'N/A'}</td>
            <td><button onclick="removeNode('${node.nodeId}')" class="btn-danger">Remove</button></td>
          `;
        });
      } else {
        const row = table.insertRow();
        row.innerHTML = '<td colspan="6">No slave nodes registered</td>';
      }
    })
    .catch(error => console.error('Failed to load nodes:', error));
}

function removeNode(nodeId) {
  if (confirm(`Remove node "${nodeId}"?`)) {
    fetch('/api/nodes/remove', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: `nodeId=${nodeId}`
    })
    .then(response => response.json())
    .then(data => {
      if (data.success) {
        loadRegisteredNodes(); // Reload the table
      } else {
        alert('Failed to remove node');
      }
    })
    .catch(error => console.error('Failed to remove node:', error));
  }
}

function testMasterConnection() {
  const masterIP = document.getElementById('masterIP').value;
  const nodeId = document.getElementById('pname').value || 'TestNode';
  const channel = document.getElementById('nodeChannel').value;
  const statusEl = document.getElementById('connectionStatus');
  
  statusEl.textContent = 'Testing connection...';
  statusEl.className = 'status-message status-pending';
  
  // Test connection to master
  fetch(`http://${masterIP}/api/node/register`, {
    method: 'POST',
    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
    body: `nodeId=${nodeId}&channel=${channel}`
  })
  .then(response => response.json().then(data => ({status: response.status, data})))
  .then(({status, data}) => {
    if (status === 200 && data.success) {
      statusEl.textContent = `✅ Connected to Master! Assigned to Channel ${data.assignedChannel}`;
      statusEl.className = 'status-message status-success';
    } else if (status === 400 && data.error) {
      // Handle specific error messages from master
      if (data.error.includes('Maximum 7 slave nodes')) {
        statusEl.textContent = '⚠️ Master network is full (7/7 slaves). Try again later or contact race coordinator.';
        statusEl.className = 'status-message status-warning';
      } else if (data.error.includes('nodeId and channel required')) {
        statusEl.textContent = '❌ Missing Node ID or Channel. Please fill all fields.';
        statusEl.className = 'status-message status-error';
      } else if (data.error.includes('nodeId cannot be empty')) {
        statusEl.textContent = '❌ Node ID cannot be empty. Please enter a pilot name or node identifier.';
        statusEl.className = 'status-message status-error';
      } else if (data.error.includes('Invalid channel')) {
        statusEl.textContent = '❌ Invalid channel selected. Please choose a channel between 1-8.';
        statusEl.className = 'status-message status-error';
      } else {
        statusEl.textContent = '❌ Connection failed: ' + data.error;
        statusEl.className = 'status-message status-error';
      }
    } else {
      statusEl.textContent = '❌ Connection failed: ' + (data.message || data.error || 'Unknown error');
      statusEl.className = 'status-message status-error';
    }
  })
  .catch(error => {
    statusEl.textContent = `❌ Cannot reach master at ${masterIP}. Check Master IP and network connection.`;
    statusEl.className = 'status-message status-error';
    console.error('Connection test failed:', error);
  });
}

function saveNetworkConfig() {
  const deviceMode = document.getElementById('deviceMode').value;
  const masterIP = document.getElementById('masterIP').value;
  const nodeChannel = document.getElementById('nodeChannel').value;
  
  const config = {
    deviceMode: parseInt(deviceMode),
    masterIP: masterIP,
    nodeChannel: parseInt(nodeChannel)
  };
  
  fetch('/config', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(config)
  })
  .then(response => response.json())
  .then(data => {
    if (data.success || response.ok) {
      alert('Network configuration saved! Device will restart to apply changes.');
      setTimeout(() => {
        window.location.reload();
      }, 2000);
    } else {
      alert('Failed to save configuration');
    }
  })
  .catch(error => {
    console.error('Error saving config:', error);
    alert('Failed to save configuration');
  });
}

function resetNetworkConfig() {
  if (confirm('Reset to Standalone mode? This will restart the device.')) {
    const config = {
      deviceMode: 0, // Standalone
      masterIP: '192.168.4.1',
      nodeChannel: 1
    };
    
    fetch('/config', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify(config)
    })
    .then(() => {
      alert('Reset to Standalone mode! Device restarting...');
      setTimeout(() => window.location.reload(), 2000);
    })
    .catch(error => console.error('Reset failed:', error));
  }
}
