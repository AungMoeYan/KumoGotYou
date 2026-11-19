"use strict";

/* ============================================================
   KUMOGOTYOU — DASHBOARD CONTROLLER
   Existing UI and endpoint names are intentionally preserved.
   ============================================================ */

let currentCommand = "stop";
let lastSensorUpdate = 0;
let cameraFrameLoaded = false;
let cameraRetryTimer = null;
let voiceEnabled = false;
let lastSpokenAlertId = 0;

const pressedKeys = new Set();

const $ = (id) => document.getElementById(id);

const movementDisplay = $("movementDisplay");
const espStatus = $("espStatus");
const camStatus = $("camStatus");
const espDot = $("espDot");
const camDot = $("camDot");
const cameraFeed = $("cameraFeed");
const cameraOverlay = $("cameraOverlay");
const cameraLabel = $("cameraLabel");
const voiceToggle = $("voiceToggle");
const aiInput = $("aiInput");
const aiSend = $("aiSend");
const chat = $("chat");

const MOVEMENT_NAMES = {
    forward: "FORWARD",
    backward: "BACKWARD",
    left: "LEFT",
    right: "RIGHT",
    turn_left: "TURN LEFT",
    turn_right: "TURN RIGHT",
    stand: "STAND",
    sit: "SIT",
    stop: "STOP"
};

const DIRECTION_COMMANDS = new Set([
    "forward", "backward", "left", "right", "turn_left", "turn_right"
]);

/* ============================================================
   API HELPERS
   ============================================================ */

async function fetchJSON(url, options = {}) {
    const response = await fetch(url, {
        cache: "no-store",
        ...options,
        headers: {
            ...(options.headers || {}),
            ...(options.body ? { "Content-Type": "application/json" } : {})
        }
    });

    if (!response.ok) {
        throw new Error(`${url} returned HTTP ${response.status}`);
    }

    return response.json();
}

/* ============================================================
   ROBOT COMMANDS
   ============================================================ */

async function sendCommand(command) {
    command = String(command || "").trim().toLowerCase();
    if (!command) return;

    try {
        const data = await fetchJSON("/robot_command", {
            method: "POST",
            body: JSON.stringify({ command })
        });

        if (data.status === "ok") {
            currentCommand = data.command || command;
            updateMovementDisplay(currentCommand);
            console.log("[WEB COMMAND]", data.id, data.command);
        }
    } catch (error) {
        console.error("[COMMAND ERROR]", error);
    }
}

function updateMovementDisplay(command) {
    if (!movementDisplay) return;

    const label = MOVEMENT_NAMES[command] || String(command).toUpperCase();
    movementDisplay.textContent = "";

    const dot = document.createElement("span");
    dot.className = "movement-dot";
    movementDisplay.append(dot, document.createTextNode(label));
}

document.querySelectorAll("[data-command]").forEach((button) => {
    const command = button.dataset.command;

    if (!DIRECTION_COMMANDS.has(command)) {
        button.addEventListener("click", (event) => {
            event.preventDefault();
            sendCommand(command);
        });
        return;
    }

    let pointerHeld = false;

    button.addEventListener("pointerdown", (event) => {
        event.preventDefault();
        if (pointerHeld) return;
        pointerHeld = true;
        button.classList.add("active");
        try { button.setPointerCapture(event.pointerId); } catch (_) {}
        sendCommand(command);
    });

    const release = (event) => {
        if (!pointerHeld) return;
        if (event) event.preventDefault();
        pointerHeld = false;
        button.classList.remove("active");
        sendCommand("stop");
    };

    button.addEventListener("pointerup", release);
    button.addEventListener("pointercancel", release);
    button.addEventListener("lostpointercapture", release);
});

/* ============================================================
   KEYBOARD CONTROL
   ============================================================ */

const keyMap = {
    ArrowUp: "forward",
    ArrowDown: "backward",
    ArrowLeft: "left",
    ArrowRight: "right",
    w: "forward",
    a: "left",
    s: "backward",
    d: "right",
    q: "turn_left",
    e: "turn_right",
    " ": "stop"
};

function isTextEntry(target) {
    return target instanceof HTMLElement && (
        target.isContentEditable ||
        ["INPUT", "TEXTAREA", "SELECT"].includes(target.tagName)
    );
}

window.addEventListener("keydown", (event) => {
    if (isTextEntry(event.target)) return;

    const command = keyMap[event.key] || keyMap[event.key.toLowerCase()];
    if (!command || pressedKeys.has(event.key)) return;

    pressedKeys.add(event.key);
    event.preventDefault();
    sendCommand(command);
});

window.addEventListener("keyup", (event) => {
    if (isTextEntry(event.target)) return;

    const command = keyMap[event.key] || keyMap[event.key.toLowerCase()];
    if (!command) return;

    pressedKeys.delete(event.key);
    event.preventDefault();

    if (command !== "stop") sendCommand("stop");
});

function releaseAllControls() {
    pressedKeys.clear();
    document.querySelectorAll("[data-command].active")
        .forEach((button) => button.classList.remove("active"));
    sendCommand("stop");
}

window.addEventListener("blur", releaseAllControls);
document.addEventListener("visibilitychange", () => {
    if (document.hidden) releaseAllControls();
});

/* ============================================================
   SENSOR DATA
   ============================================================ */

async function updateSensors() {
    try {
        const data = await fetchJSON("/sensor_data");
        lastSensorUpdate = Date.now();
        updateSensorUI(data);

        const updated = $("sensorUpdated");
        if (updated) {
            updated.textContent = "UPDATED " + new Date(lastSensorUpdate)
                .toLocaleTimeString([], {
                    hour: "2-digit",
                    minute: "2-digit",
                    second: "2-digit"
                });
        }
    } catch (error) {
        console.error("[SENSOR ERROR]", error);
    }
}

function updateSensorUI(data = {}) {
    const distance = Number(data?.ultrasonic?.distance);
    const mq2 = Number(data?.mq2);
    const pir = Boolean(data?.pir);
    const ir = Boolean(data?.ir_obstacle);

    const distanceElement = $("distance");
    const mq2Element = $("mq2");
    const pirElement = $("pirStatus");
    const irElement = $("irStatus");
    const rawElement = $("irRaw");

    if (distanceElement) {
        distanceElement.textContent =
            Number.isFinite(distance) && distance > 0 ? distance.toFixed(1) : "--";
    }

    if (mq2Element) {
        mq2Element.textContent = Number.isFinite(mq2) ? String(Math.round(mq2)) : "--";
    }

    if (pirElement) {
        pirElement.textContent = pir ? "DETECTED" : "CLEAR";
        pirElement.style.color = pir ? "#ff687c" : "#eaf7ff";
    }

    if (irElement) {
        irElement.textContent = ir ? "BLOCKED" : "CLEAR";
        irElement.style.color = ir ? "#ff687c" : "#eaf7ff";
    }

    if (rawElement) {
        rawElement.textContent = `RAW: ${data.ir_raw ?? "--"}`;
    }
}

/* ============================================================
   CONNECTION STATUS
   ============================================================ */

function setESPStatus(online) {
    if (espStatus) espStatus.textContent = online ? "ONLINE" : "OFFLINE";
    if (espDot) {
        espDot.classList.toggle("online", online);
        espDot.classList.toggle("offline", !online);
    }
}

function setCameraStatus(online) {
    const active = Boolean(online && cameraFrameLoaded);

    if (camStatus) camStatus.textContent = active ? "ONLINE" : "OFFLINE";
    if (cameraLabel) {
        cameraLabel.textContent = active ? "ONLINE" : "OFFLINE";
        cameraLabel.style.color = active ? "#4fe2b0" : "#ff657b";
    }
    if (camDot) {
        camDot.classList.toggle("online", active);
        camDot.classList.toggle("offline", !active);
    }
    if (cameraOverlay) {
        cameraOverlay.classList.toggle("hidden", active);
    }
}

async function updateStatus() {
    try {
        const data = await fetchJSON("/status");
        setESPStatus(Boolean(data.esp32));
        setCameraStatus(Boolean(data.camera));
    } catch (error) {
        setESPStatus(false);
        setCameraStatus(false);
        console.error("[STATUS ERROR]", error);
    }
}

/* ============================================================
   LIVE CAMERA
   ============================================================ */

function scheduleCameraRetry() {
    if (cameraRetryTimer) return;

    cameraRetryTimer = window.setTimeout(() => {
        cameraRetryTimer = null;
        if (!cameraFeed) return;

        cameraFrameLoaded = false;
        setCameraStatus(false);

        // Re-open the same Flask MJPEG proxy; no direct camera URL or capture route.
        cameraFeed.src = `/camera?retry=${Date.now()}`;
    }, 3000);
}

if (cameraFeed) {
    cameraFeed.addEventListener("load", () => {
        cameraFrameLoaded = true;
        if (cameraRetryTimer) {
            clearTimeout(cameraRetryTimer);
            cameraRetryTimer = null;
        }
        setCameraStatus(true);
    });

    cameraFeed.addEventListener("error", () => {
        cameraFrameLoaded = false;
        setCameraStatus(false);
        scheduleCameraRetry();
    });
}

/* ============================================================
   KUMO VOICE
   ============================================================ */

function speakKumo(message) {
    if (!voiceEnabled || !("speechSynthesis" in window) || !message) return;

    window.speechSynthesis.cancel();
    const utterance = new SpeechSynthesisUtterance(String(message));
    utterance.lang = "en-US";
    utterance.rate = 1;
    utterance.pitch = 1;
    window.speechSynthesis.speak(utterance);
}

if (voiceToggle) {
    voiceToggle.addEventListener("click", () => {
        voiceEnabled = !voiceEnabled;
        voiceToggle.textContent = voiceEnabled ? "VOICE ON" : "VOICE OFF";
        voiceToggle.setAttribute("aria-pressed", String(voiceEnabled));
        voiceToggle.classList.toggle("active", voiceEnabled);

        if (!voiceEnabled && "speechSynthesis" in window) {
            window.speechSynthesis.cancel();
        } else {
            speakKumo("Kumo voice alerts enabled.");
        }
    });
}

/* ============================================================
   AUTOMATIC AI ALERTS
   ============================================================ */

async function updateAIAlert() {
    try {
        const data = await fetchJSON("/ai_alert");
        const alertBox = $("ai-alert");
        if (!alertBox) return;

        if (!data.alert) {
            alertBox.style.display = "none";
            return;
        }

        alertBox.style.display = "block";
        const status = data.pending ? "ANALYSING" : "AI ALERT";
        alertBox.textContent =
            `${status} — ${data.type || "Sensor event"}: ` +
            `${data.response || "Checking sensor readings..."}`;

        const alertId = Number(data.id || data.time || 0);
        if (!data.pending && alertId && alertId !== lastSpokenAlertId) {
            lastSpokenAlertId = alertId;
            speakKumo(`Kumo alert. ${data.type || "Sensor event"}. ${data.response || ""}`);
        }
    } catch (error) {
        console.warn("[AI ALERT ERROR]", error);
    }
}

/* ============================================================
   AI CHAT
   ============================================================ */

function addChatMessage(message, type) {
    if (!chat) return;

    const element = document.createElement("div");
    element.className = `chat-message ${type}`;

    if (type === "ai") {
        const tag = document.createElement("div");
        tag.className = "message-tag";
        tag.textContent = "KUMO";

        const body = document.createElement("div");
        body.textContent = String(message);

        element.append(tag, body);
    } else {
        element.textContent = String(message);
    }

    chat.appendChild(element);
    chat.scrollTop = chat.scrollHeight;
}

async function sendAI() {
    if (!aiInput) return;

    const message = aiInput.value.trim();
    if (!message) return;

    addChatMessage(message, "user");
    aiInput.value = "";

    if (aiSend) aiSend.disabled = true;

    try {
        const data = await fetchJSON("/ai", {
            method: "POST",
            body: JSON.stringify({ message })
        });

        if (data.response) {
            addChatMessage(data.response, "ai");
            speakKumo(data.response);
        } else {
            addChatMessage(data.error || "Kumo could not respond.", "ai");
        }
    } catch (error) {
        addChatMessage("Connection to Kumo failed.", "ai");
        console.error("[AI CHAT ERROR]", error);
    } finally {
        if (aiSend) aiSend.disabled = false;
    }
}

if (aiSend) aiSend.addEventListener("click", sendAI);

if (aiInput) {
    aiInput.addEventListener("keydown", (event) => {
        if (event.key === "Enter" && !event.shiftKey) {
            event.preventDefault();
            sendAI();
        }
    });
}

/* ============================================================
   INITIALIZATION
   ============================================================ */

updateSensors();
updateStatus();
updateAIAlert();
updateMovementDisplay("stop");

window.setInterval(updateSensors, 500);
window.setInterval(updateStatus, 1000);
window.setInterval(updateAIAlert, 1000);

console.log(
    "%c KUMOGOTYOU UI READY ",
    "background:#071a24;color:#4de5ff;padding:5px 10px;border-radius:4px;font-weight:bold;"
);
