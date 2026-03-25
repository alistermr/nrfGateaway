const API = "http://localhost:5000"; // Flask backend URL

let addressCache = [0]; // index 0 = gateway placeholder, provisioned nodes start at index 1
let nonProvCache = [];
let prov_beacon = false;
let selectedDeviceIdx = null;
let scanPollTimer = null;

function delay(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

function setStatus(type, text) {
    const badge = document.getElementById("statusBadge");
    badge.style.display = "inline-block";
    badge.className = "status-badge " + type;
    badge.textContent = text;
}

function setOutput(content) {
    document.getElementById("output").textContent =
        typeof content === "string" ? content : JSON.stringify(content, null, 2);
}

async function sendMessage(message) {
    try {
        const res = await fetch(API + "/api/send", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ message }),
        });

        return res;
    } catch (err) {
        setStatus("error", "Network Error");
        setOutput(err.message);
        return null;
    }
}

async function sendMessagebtn() {
    const msg = document.getElementById("message").value.trim();
    if (!msg) {
        setOutput("Please enter a message.");
        return;
    }

    const btn = document.getElementById("sendBtn");
    btn.disabled = true;
    setOutput("");
    setStatus("pending", "Sending…");
    document.getElementById("output").innerHTML =
        '<span class="spinner"></span> Sending message to board (c2d)…';

    const res = await sendMessage(msg);
    if (!res) {
        btn.disabled = false;
        return;
    }

    const data = await res.json();

    if (res.ok) {
        setStatus("success", "Message sent");
    } else {
        setStatus("error", `${res.status} Error`);
    }

    setOutput(data);
    btn.disabled = false;
}

async function getDeviceState() {
    setOutput("");
    setStatus("pending", "Fetching…");
    document.getElementById("output").innerHTML =
        '<span class="spinner"></span> Fetching device state…';

    try {
        const res = await fetch(API + "/api/device");
        const data = await res.json();

        if (res.ok) {
            setStatus("success", `${res.status} OK`);
        } else {
            setStatus("error", `${res.status} Error`);
        }

        setOutput(data);
    } catch (err) {
        setStatus("error", "Network Error");
        setOutput(err.message);
    }
}

async function listMessages() {
    setOutput("");
    setStatus("pending", "Fetching…");
    document.getElementById("output").innerHTML =
        '<span class="spinner"></span> Loading message history…';

    try {
        const res = await fetch(API + "/api/get_messages");
        const data = await res.json();

        const items = data.response?.items ?? [];

        if (res.ok) {
            setStatus("success", `${items.length} messages`);
        } else {
            setStatus("error", `${res.status} Error`);
        }

        if (items.length === 0) {
            setOutput("No messages found.");
        } else {
            const formatted = items.map(m =>
                `[${m.receivedAt ?? ""}] ${m.appId ?? ""}: ${m.data ?? JSON.stringify(m)}`
            ).join("\n");
            setOutput(formatted);
        }
    } catch (err) {
        setStatus("error", "Network Error");
        setOutput(err.message);
    }
}

async function getUnprovisionedDevices() {
    const hardcodedUUIDs = [
        "271f1594ac0b426098e0ea6b53f41d9f",
        "e6f3c64022f543b3990c39bfdd0a6c4c",
        "0622b315eab04703b9dd4cea154fc8fc"
    ];

    try {
        const res = await fetch(API + "/api/nonprov");
        const data = await res.json();

        if (!res.ok) {
            setStatus("error", `${res.status} Error`);
            return;
        }

        const fetched = Array.isArray(data.uuids) ? data.uuids : [];
        const merged = [...new Set([...hardcodedUUIDs, ...fetched])];
        nonProvCache = merged;
        renderDeviceList();
    } catch (err) {
        nonProvCache = hardcodedUUIDs;
        renderDeviceList();
    }
}

async function clearUnprovisionedDevices() {
    try {
        await fetch(API + "/api/nonprov/clear", {
            method: "POST",
            headers: { "Content-Type": "application/json" }
        });
    } catch (err) {
        console.error("Failed to clear UUID cache:", err);
    }
}

async function mesh_init() {
    setOutput("");
    setStatus("pending", "Initializing mesh");
    document.getElementById("output").innerHTML =
        '<span class="spinner"></span> Initializing mesh network…';

    document.getElementById("initBtn").style.display = "none";

    await sendMessage("init");

    setStatus("success", "Mesh Initialized");
    document.getElementById("output").innerHTML = "Mesh network initialized.";
}

async function toggle_prov_beacon() {
    await sendMessage("scan");
    if (prov_beacon) {
        prov_beacon = false;
    } else {
        prov_beacon = true;
    }
}

function renderProvisionedList() {
    const list = document.getElementById("provisionedList");
    const cache = addressCache.slice(1); // do not mutate addressCache

    if (cache.length === 0) {
        list.innerHTML = '<div class="no-devices">No provisioned devices yet.</div>';
        return;
    }

    list.innerHTML = "";

    cache.forEach((addr, i) => {
        for (let u = 0; u < 4; u++) {
            const elemAddr = addr + u; // numeric addition
            const elemAddrHex = "0x" + elemAddr.toString(16).padStart(4, "0");
            const el = document.createElement("div");
            el.className = "device-item";
            el.innerHTML =
                `<span><span class="device-index">Node ${i} — Element ${u}</span>` +
                `<span class="device-uuid">${elemAddrHex}</span></span>` +
                `<button class="btn-device" onclick="toggle_light(${elemAddr}, true)">On</button>` +
                `<button class="btn-device" onclick="toggle_light(${elemAddr}, false)" style="background:#ef4444;color:#fff;">Off</button>`;
            list.appendChild(el);
        }
    });
}

function renderDeviceList() {
    const list = document.getElementById("deviceList");
    const provBtn = document.getElementById("provisionBtn");

    if (nonProvCache.length === 0) {
        list.innerHTML = '<div class="no-devices">No unprovisioned devices found. Click Scan to start.</div>';
        selectedDeviceIdx = null;
        provBtn.disabled = true;
        return;
    }

    list.innerHTML = "";

    nonProvCache.forEach((uuid, i) => {
        const el = document.createElement("div");
        el.className = "device-item" + (selectedDeviceIdx === i ? " selected" : "");
        el.innerHTML =
            `<span><span class="device-index">#${i}</span>` +
            `<span class="device-uuid">${uuid}</span></span>`;
        el.onclick = () => selectDevice(i);
        list.appendChild(el);
    });
}

function selectDevice(idx) {
    selectedDeviceIdx = idx;
    document.getElementById("provisionBtn").disabled = false;
    renderDeviceList();
}

async function toggleScan() {
    const btn = document.getElementById("scanBtn");

    await toggle_prov_beacon();

    if (prov_beacon) {
        btn.textContent = "⏹ Stop";
        btn.classList.add("scanning");
        setStatus("pending", "Scanning…");
        document.getElementById("output").innerHTML =
            '<span class="spinner"></span> Scanning for unprovisioned devices…';

        await clearUnprovisionedDevices();
        await delay(500);
        await getUnprovisionedDevices();

        scanPollTimer = setInterval(async () => {
            await getUnprovisionedDevices();
        }, 1500);
    } else {
        btn.textContent = "🔍 Scan";
        btn.classList.remove("scanning");
        setStatus("success", "Scan stopped");

        if (scanPollTimer) {
            clearInterval(scanPollTimer);
            scanPollTimer = null;
        }
    }

    renderDeviceList();
}

async function provisionSelected() {
    if (selectedDeviceIdx === null) return;

    const uuid = nonProvCache[selectedDeviceIdx];
    const nextAddrNum = addressCache.length * 0x10;
    const toHex = (n) => "0x" + n.toString(16).padStart(4, "0");
    const nextAddr = toHex(nextAddrNum);

    setStatus("pending", "Provisioning… " + nextAddr);
    document.getElementById("output").innerHTML =
        '<span class="spinner"></span> Provisioning ' + uuid + '…';

    await sendMessage(`prov ${uuid} 0 0`);

    addressCache.push(nextAddrNum); // store as number so addr + u is addition, not concatenation
    renderProvisionedList();

    nonProvCache.splice(selectedDeviceIdx, 1);
    selectedDeviceIdx = null;
    document.getElementById("provisionBtn").disabled = true;
    renderDeviceList();

    setStatus("success", "Provisioned " + nextAddr);
    setOutput(`Device ${uuid} provisioned at address ${nextAddr}`);
}

async function toggle_light(addr, on) {
    if (on) {
        await sendMessage(`light 0 0 ${addr} 1`);
    } else {
        await sendMessage(`light 0 0 ${addr} 0`);
    }
}