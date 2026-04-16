const socketsByPlayer = new Map();

const canvas = document.getElementById("map");
const ctx = canvas.getContext("2d");

let state = null;
let viewedPlayerId = 0;
let selectedDescription = "Click a revealed tile, item, portal, or player for details.";

const MAP_TILES = 20;
const INTERNAL_CANVAS_SIZE = 600;
canvas.width = INTERNAL_CANVAS_SIZE;
canvas.height = INTERNAL_CANVAS_SIZE;

const narrativeEl = document.getElementById("narrative");
const chatMessagesEl = document.getElementById("chatMessages");
const chatInputEl = document.getElementById("chatInput");
const controlsBodyEl = document.getElementById("controlsBody");
const inventoryEl = document.getElementById("inventory");
const statusEl = document.getElementById("status");

function openPlayerSocket() {
    const ws = new WebSocket("ws://localhost:9001");

    ws.onclose = () => {
        for (const [playerId, playerSocket] of socketsByPlayer.entries()) {
            if (playerSocket === ws) {
                socketsByPlayer.delete(playerId);
                break;
            }
        }
        if (state) render();
    };

    ws.onerror = () => {
        statusEl.innerText = "WebSocket error";
    };

    ws.onmessage = (msg) => {
        const incomingState = JSON.parse(msg.data);
        const socketPlayerId = incomingState.yourPlayerId;
        if (typeof socketPlayerId === "number" && socketPlayerId >= 0 && socketPlayerId <= 1) {
            socketsByPlayer.set(socketPlayerId, ws);
        }
        state = incomingState;
        render();
    };
}

openPlayerSocket();
openPlayerSocket();

function getSocketForPlayer(playerId) {
    const ws = socketsByPlayer.get(playerId);
    if (ws && ws.readyState === WebSocket.OPEN) return ws;
    return null;
}

function getAnyOpenSocket() {
    for (const ws of socketsByPlayer.values()) {
        if (ws.readyState === WebSocket.OPEN) return ws;
    }
    return null;
}

function canControlPlayer(playerId) {
    return Boolean(getSocketForPlayer(playerId));
}

function sendMessage(payload, playerId = getActivePlayerId()) {
    const targetSocket = getSocketForPlayer(playerId) || getAnyOpenSocket();
    if (targetSocket && targetSocket.readyState === WebSocket.OPEN) {
        targetSocket.send(JSON.stringify(payload));
    }
}

function getActivePlayerId() {
    return viewedPlayerId;
}

function sanitizeNarrativeForDisplay(rawNarrative) {
    const text = String(rawNarrative || "").trim();
    if (!text) return "The world is quiet.";

    const looksLikeStats = text.includes("Player 1 inventory") ||
        text.includes("Player 2 inventory") ||
        text.includes("weapons=") ||
        text.includes("potions=") ||
        text.includes("key=") ||
        text.includes("health=") ||
        text.includes("Describe the newly explored area in a fantasy tone.") ||
        text.includes("Narrate the result while respecting");

    if (!looksLikeStats) {
        return text;
    }

    const activePlayerId = getActivePlayerId();
    const p = `Player ${activePlayerId + 1}`;
    return `${p} advances through the wild frontier, scanning the land for signs of the hidden key while the ancient portal stirs in the distance.`;
}

function sendAction() {
    const input = document.getElementById("actionInput");
    if (!input) return;

    const action = input.value.trim();
    if (!action) return;
    const activePlayerId = getActivePlayerId();

    sendMessage({
        type: "action",
        player: activePlayerId,
        action
    });

    input.value = "";
}

function sendChat() {
    const text = chatInputEl.value.trim();
    if (!text) return;
    const activePlayerId = getActivePlayerId();

    sendMessage({
        type: "chat",
        player: activePlayerId,
        text
    });

    chatInputEl.value = "";
}

function move(dx, dy) {
    if (!state) {
        statusEl.innerText = "Waiting for game state...";
        return;
    }

    if (state.inCombat) {
        statusEl.innerText = "Movement disabled during combat.";
        return;
    }

    const activePlayerId = getActivePlayerId();
    if (state.currentTurn !== activePlayerId) {
        statusEl.innerText = `Waiting for Player ${state.currentTurn + 1}`;
        return;
    }

    sendMessage({
        type: "move",
        player: activePlayerId,
        dx,
        dy
    });

    statusEl.innerText = `Player ${activePlayerId + 1} moved. Waiting for update...`;
}

function chooseCombat(choice) {
    if (!state) return;
    if (!state.inCombat) return;
    const activePlayerId = getActivePlayerId();
    if (state.currentTurn !== activePlayerId) return;

    sendMessage({
        type: "combat",
        player: activePlayerId,
        choice
    });
}

function usePotion() {
    if (!state) return;
    if (!state.inCombat) return;
    const activePlayerId = getActivePlayerId();
    if (state.currentTurn !== activePlayerId) return;

    sendMessage({
        type: "usePotion",
        player: activePlayerId
    });
}

function switchPlayer(id) {
    if (id !== 0 && id !== 1) return;
    viewedPlayerId = id;
    render();
}

function getTileSize() {
    return canvas.width / MAP_TILES;
}

function getTerrainName(tile) {
    if (tile === 0) return "Forest";
    if (tile === 1) return "Dirt";
    if (tile === 2) return "Rock";
    return "Unknown";
}

function getItemDescription(type) {
    if (type === "KEY") return "A portal key. Needed to unlock the portal.";
    if (type === "POTION") return "A health potion. Can be used in combat to restore health.";
    if (type === "WEAPON") return "A weapon. Increases damage dealt during boss combat.";
    return "Unknown item.";
}

function getTileDescription(x, y) {
    if (!state || !state.fog[y] || !state.fog[y][x]) {
        return "This area is hidden by fog of war.";
    }

    const playersHere = state.players
        .map((p, idx) => ({ ...p, idx }))
        .filter(p => p.x === x && p.y === y);

    if (playersHere.length > 0) {
        return playersHere
            .map(p => `Player ${p.idx + 1}: Health ${p.health}, Weapons ${p.weaponCount}, Potions ${p.potionCount}, Key ${p.hasKey ? "Yes" : "No"}`)
            .join("\n");
    }

    const item = state.items.find(i => i.x === x && i.y === y);
    if (item) {
        return `${item.type}: ${getItemDescription(item.type)}`;
    }

    if (state.portal && state.portal[0] === x && state.portal[1] === y) {
        return state.portalUnlocked
            ? "The portal is unlocked. A boss encounter has begun."
            : "A locked portal. A key is required to activate it.";
    }

    return `Terrain: ${getTerrainName(state.map[y][x])}`;
}

canvas.addEventListener("click", (event) => {
    if (!state) return;

    const rect = canvas.getBoundingClientRect();
    const scaleX = canvas.width / rect.width;
    const scaleY = canvas.height / rect.height;
    const tileSize = getTileSize();

    const x = Math.floor(((event.clientX - rect.left) * scaleX) / tileSize);
    const y = Math.floor(((event.clientY - rect.top) * scaleY) / tileSize);

    if (x < 0 || y < 0 || y >= state.map.length || x >= state.map[0].length) return;

    selectedDescription = getTileDescription(x, y);
    renderControls();
});

function render() {
    if (!state) return;

    renderMap();
    renderNarrative();
    renderChat();
    renderInventory();
    renderControls();

    const activeView = getActivePlayerId();
    const connectedPlayers = [...socketsByPlayer.keys()].map((p) => `P${p + 1}`).sort().join(", ") || "none";
    if (!canControlPlayer(activeView)) {
        statusEl.innerText = `Connected players: ${connectedPlayers} — viewing Player ${activeView + 1} (not connected)`;
    } else if (state.currentTurn === activeView && !state.inCombat) {
        statusEl.innerText = `Connected players: ${connectedPlayers} — viewing Player ${activeView + 1} turn ready`;
    } else if (state.currentTurn === activeView && state.inCombat) {
        statusEl.innerText = `Connected players: ${connectedPlayers} — viewing Player ${activeView + 1} combat turn`;
    } else {
        statusEl.innerText = `Connected players: ${connectedPlayers} — viewing Player ${activeView + 1}, waiting for Player ${state.currentTurn + 1}`;
    }
}

function renderMap() {
    const tileSize = getTileSize();
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    state.map.forEach((row, y) => {
        row.forEach((tile, x) => {
            if (!state.fog[y][x]) {
                ctx.fillStyle = "black";
            } else {
                if (tile === 0) ctx.fillStyle = "#2e8b57";
                else if (tile === 1) ctx.fillStyle = "#8b5a2b";
                else ctx.fillStyle = "#6b7280";
            }

            ctx.fillRect(x * tileSize, y * tileSize, tileSize, tileSize);
            ctx.strokeStyle = "#111";
            ctx.strokeRect(x * tileSize, y * tileSize, tileSize, tileSize);
        });
    });

    if (state.portal && state.fog[state.portal[1]][state.portal[0]]) {
        ctx.fillStyle = state.portalUnlocked ? "#00e5ff" : "#7c3aed";
        ctx.beginPath();
        ctx.arc(
            state.portal[0] * tileSize + tileSize / 2,
            state.portal[1] * tileSize + tileSize / 2,
            tileSize / 3,
            0,
            Math.PI * 2
        );
        ctx.fill();
    }

    state.items.forEach(i => {
        if (!state.fog[i.y][i.x]) return;

        if (i.type === "KEY") ctx.fillStyle = "#facc15";
        else if (i.type === "POTION") ctx.fillStyle = "#ef4444";
        else ctx.fillStyle = "#60a5fa";

        ctx.fillRect(
            i.x * tileSize + tileSize * 0.2,
            i.y * tileSize + tileSize * 0.2,
            tileSize * 0.6,
            tileSize * 0.6
        );
    });

    const playerGroups = new Map();
    state.players.forEach((p, idx) => {
        const key = `${p.x},${p.y}`;
        if (!playerGroups.has(key)) playerGroups.set(key, []);
        playerGroups.get(key).push({ ...p, idx });
    });

    playerGroups.forEach((playersOnTile) => {
        playersOnTile.forEach((p, localIndex) => {
            if (!state.fog[p.y][p.x]) return;

            const baseX = p.x * tileSize + tileSize / 2;
            const baseY = p.y * tileSize + tileSize / 2;

            let offsetX = 0;
            if (playersOnTile.length === 2) {
                offsetX = localIndex === 0 ? -tileSize * 0.16 : tileSize * 0.16;
            }

            ctx.fillStyle = p.idx === 0 ? "#ff6b6b" : "#4dabf7";
            ctx.beginPath();
            ctx.arc(baseX + offsetX, baseY, tileSize * 0.24, 0, Math.PI * 2);
            ctx.fill();

            ctx.fillStyle = "white";
            ctx.font = `${Math.max(10, tileSize * 0.35)}px Arial`;
            ctx.fillText(String(p.idx + 1), baseX + offsetX - tileSize * 0.08, baseY + tileSize * 0.08);
        });
    });
}

function renderNarrative() {
    const playerTurnText = state.inCombat
        ? `Combat turn: Player ${state.currentTurn + 1}`
        : `Exploration turn: Player ${state.currentTurn + 1}`;
    const aiSummary = state.summary || "No summary available yet.";

    const cleanedNarrative = sanitizeNarrativeForDisplay(state.narrative);
    narrativeEl.innerHTML = `
        <div class="summaryBox">
            <div class="panelSection"><strong>AI Narrative</strong></div>
            <div class="panelSection">${escapeHtml(cleanedNarrative)}</div>
        </div>
        <div class="summaryBox">
            <div class="panelSection"><strong>AI Summary</strong></div>
            <div class="panelSection">${escapeHtml(aiSummary)}</div>
        </div>
        <div class="summaryBox">
            <div class="panelSection"><strong>Status</strong></div>
            <div class="panelSection">${escapeHtml(playerTurnText)}</div>
            <div class="panelSection"><strong>Boss Health</strong></div>
            <div class="panelSection">${state.inCombat ? state.bossHealth : "No active boss fight"}</div>
        </div>
    `;
}

function renderChat() {
    const messages = state.chat || [];
    chatMessagesEl.innerHTML = messages.map(line => `<div class="chatLine">${escapeHtml(line)}</div>`).join("");
    chatMessagesEl.scrollTop = chatMessagesEl.scrollHeight;
}

function renderInventory() {
    const activeView = getActivePlayerId();
    inventoryEl.innerHTML = state.players.map((p, idx) => `
        <div class="inventoryBlock ${idx === activeView ? "activePlayer" : ""}">
            <div><strong>Player ${idx + 1}</strong></div>
            <div>Health: ${p.health}</div>
            <div>Weapons: ${p.weaponCount}</div>
            <div>Potions: ${p.potionCount}</div>
            <div>Key: ${p.hasKey ? "Yes" : "No"}</div>
            <div>Items: ${p.inventory.length ? p.inventory.join(", ") : "None"}</div>
        </div>
    `).join("");
}

function renderControls() {
    if (!state) return;

    const activeView = getActivePlayerId();
    const isMyTurn = state.currentTurn === activeView && canControlPlayer(activeView);
    const turnText = isMyTurn
        ? `Player ${activeView + 1} turn`
        : `Waiting for Player ${state.currentTurn + 1}`;

    if (state.inCombat) {
        controlsBodyEl.innerHTML = `
            <div class="summaryBox">
                <div class="panelSection"><strong>${turnText}</strong></div>
                <div class="panelSection">Choose Rock, Paper, or Scissors. Use a POTION if needed.</div>
            </div>

            <div class="buttonRow">
                <button id="combat-rock" ${!isMyTurn ? "disabled" : ""}>Rock</button>
                <button id="combat-paper" ${!isMyTurn ? "disabled" : ""}>Paper</button>
                <button id="combat-scissors" ${!isMyTurn ? "disabled" : ""}>Scissors</button>
                <button id="use-potion" ${!isMyTurn ? "disabled" : ""}>Use Potion</button>
            </div>

            <div class="playerSwitchRow">
                <button id="switch-p1" ${activeView === 0 ? "disabled" : ""}>View as Player 1</button>
                <button id="switch-p2" ${activeView === 1 ? "disabled" : ""}>View as Player 2</button>
            </div>

            <div class="selectionBox">
                <div class="panelSection"><strong>Selected Tile / Object</strong></div>
                <div class="panelSection">${escapeHtml(selectedDescription)}</div>
            </div>
        `;

        document.getElementById("combat-rock")?.addEventListener("click", () => chooseCombat("rock"));
        document.getElementById("combat-paper")?.addEventListener("click", () => chooseCombat("paper"));
        document.getElementById("combat-scissors")?.addEventListener("click", () => chooseCombat("scissors"));
        document.getElementById("use-potion")?.addEventListener("click", () => usePotion());
        document.getElementById("switch-p1")?.addEventListener("click", () => switchPlayer(0));
        document.getElementById("switch-p2")?.addEventListener("click", () => switchPlayer(1));
        return;
    }

    controlsBodyEl.innerHTML = `
        <div class="summaryBox">
            <div class="panelSection"><strong>${turnText}</strong></div>
            <div class="panelSection">Both players share the same map. They may stand on the same tile.</div>
        </div>

        <div class="controlsTopRow">
            <div class="moveGrid">
                <div></div>
                <button id="move-up" ${!isMyTurn ? "disabled" : ""}>↑</button>
                <div></div>
                <button id="move-left" ${!isMyTurn ? "disabled" : ""}>←</button>
                <button disabled>•</button>
                <button id="move-right" ${!isMyTurn ? "disabled" : ""}>→</button>
                <div></div>
                <button id="move-down" ${!isMyTurn ? "disabled" : ""}>↓</button>
                <div></div>
            </div>

            <div class="controlsInfo">
                <div class="actionRow">
                    <input id="actionInput" placeholder="Describe an action...">
                    <button id="send-action" ${!isMyTurn ? "disabled" : ""}>Send Action</button>
                </div>

                <div class="playerSwitchRow">
                    <button id="switch-p1" ${activeView === 0 ? "disabled" : ""}>View as Player 1</button>
                    <button id="switch-p2" ${activeView === 1 ? "disabled" : ""}>View as Player 2</button>
                </div>
            </div>
        </div>

        <div class="selectionBox">
            <div class="panelSection"><strong>Selected Tile / Object</strong></div>
            <div class="panelSection">${escapeHtml(selectedDescription)}</div>
        </div>
    `;

    document.getElementById("move-up")?.addEventListener("click", () => move(0, -1));
    document.getElementById("move-left")?.addEventListener("click", () => move(-1, 0));
    document.getElementById("move-right")?.addEventListener("click", () => move(1, 0));
    document.getElementById("move-down")?.addEventListener("click", () => move(0, 1));
    document.getElementById("send-action")?.addEventListener("click", sendAction);
    document.getElementById("switch-p1")?.addEventListener("click", () => switchPlayer(0));
    document.getElementById("switch-p2")?.addEventListener("click", () => switchPlayer(1));

    const newActionInput = document.getElementById("actionInput");
    if (newActionInput) {
        newActionInput.addEventListener("keydown", (e) => {
            if (e.key === "Enter") sendAction();
        });
    }
}

chatInputEl.addEventListener("keydown", (e) => {
    if (e.key === "Enter") sendChat();
});

function escapeHtml(value) {
    return String(value)
        .replaceAll("&", "&amp;")
        .replaceAll("<", "&lt;")
        .replaceAll(">", "&gt;");
}
