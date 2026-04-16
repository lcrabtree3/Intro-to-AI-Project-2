#include <App.h>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <vector>

#include "server/AI.h"
#include "server/GameState.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

GameState game;

struct PerSocketData {
    int playerId = -1;
};

using SocketType = uWS::WebSocket<false, true, PerSocketData>;
static std::vector<SocketType*> clients;

static int getNextAvailablePlayerId() {
    bool playerTaken[2] = {false, false};

    for (auto* client : clients) {
        if (!client) continue;
        int id = client->getUserData()->playerId;
        if (id >= 0 && id < 2) {
            playerTaken[id] = true;
        }
    }

    for (int i = 0; i < 2; ++i) {
        if (!playerTaken[i]) return i;
    }

    return -1;
}

static void sendState(SocketType* client) {
    if (!client) return;

    json state = game.toJSON();
    state["yourPlayerId"] = client->getUserData()->playerId;
    client->send(state.dump(), uWS::OpCode::TEXT);
}

static void broadcastState() {
    for (auto* client : clients) {
        sendState(client);
    }
}

static int resolveActingPlayerId(SocketType* ws, const json& msg) {
    const int socketPlayerId = ws->getUserData()->playerId;
    if (socketPlayerId < 0 || socketPlayerId > 1) {
        return -1;
    }

    const int requestedPlayerId = msg.value("player", socketPlayerId);
    const bool isValidRequested = requestedPlayerId >= 0 && requestedPlayerId <= 1;
    if (!isValidRequested) {
        return socketPlayerId;
    }

    // Allow one-client local testing to control both players from a single browser.
    if (clients.size() <= 1) {
        return requestedPlayerId;
    }

    // In multi-client mode, keep each socket bound to its assigned player.
    return socketPlayerId;
}

int main() {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    NarrativeConfig narrativeConfig;
    narrativeConfig.useModel = true;
    narrativeConfig.modelPath = "../models/gpt2-dungeon.gguf";
    narrativeConfig.cliPath = "../../llama.cpp/build/bin/llama-cli";
    narrativeConfig.maxTokens = 60;
    narrativeConfig.contextWindow = 512;
    narrativeConfig.temperature = 0.8f;

    bool initOk = initializeNarrativeModel(narrativeConfig);
    std::cout << getNarrativeModelStatus() << "\n";

    if (!initOk) {
        std::cout << "Continuing with fallback narrative generator.\n";
    }

    uWS::App().ws<PerSocketData>("/*", {
        .open = [](auto* ws) {
            ws->getUserData()->playerId = getNextAvailablePlayerId();
            clients.push_back(ws);
            std::cout << "Client connected\n";

            sendState(ws);
        },

        .message = [](auto* ws, std::string_view message, uWS::OpCode) {
            try {
                json msg = json::parse(message);
                std::string type = msg.value("type", "");
                int playerId = resolveActingPlayerId(ws, msg);

                if (playerId < 0 || playerId > 1) {
                    sendState(ws);
                    return;
                }

                if (type == "move") {
                    game.movePlayer(playerId, msg.value("dx", 0), msg.value("dy", 0));
                } else if (type == "action") {
                    game.processAction(playerId, msg.value("action", ""));
                } else if (type == "combat") {
                    game.combatTurn(playerId, msg.value("choice", ""));
                } else if (type == "usePotion") {
                    game.usePotion(playerId);
                } else if (type == "chat") {
                    game.addChatMessage(playerId, msg.value("text", ""));
                }

                broadcastState();
            } catch (const std::exception& e) {
                std::cerr << "Message handling error: " << e.what() << "\n";
                sendState(ws);
            }
        },

        .close = [](auto* ws, int, std::string_view) {
            clients.erase(std::remove(clients.begin(), clients.end(), ws), clients.end());
            std::cout << "Client disconnected\n";
        }

    }).listen(9001, [](auto* token) {
        if (token) {
            std::cout << "Server running on port 9001\n";
        } else {
            std::cout << "Failed to start server\n";
        }
    }).run();

    return 0;
}
