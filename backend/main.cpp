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

struct PerSocketData {};

using SocketType = uWS::WebSocket<false, true, PerSocketData>;
static std::vector<SocketType*> clients;

static void broadcastState() {
    json state = game.toJSON();
    std::string payload = state.dump();

    for (auto* client : clients) {
        if (client) {
            client->send(payload, uWS::OpCode::TEXT);
        }
    }
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
            clients.push_back(ws);
            std::cout << "Client connected\n";

            json state = game.toJSON();
            ws->send(state.dump(), uWS::OpCode::TEXT);
        },

        .message = [](auto* ws, std::string_view message, uWS::OpCode) {
            try {
                json msg = json::parse(message);
                std::string type = msg.value("type", "");

                if (type == "move") {
                    game.movePlayer(msg.value("player", 0), msg.value("dx", 0), msg.value("dy", 0));
                } else if (type == "action") {
                    game.processAction(msg.value("player", 0), msg.value("action", ""));
                } else if (type == "combat") {
                    game.combatTurn(msg.value("player", 0), msg.value("choice", ""));
                } else if (type == "usePotion") {
                    game.usePotion(msg.value("player", 0));
                } else if (type == "chat") {
                    game.addChatMessage(msg.value("player", 0), msg.value("text", ""));
                }

                broadcastState();
            } catch (const std::exception& e) {
                std::cerr << "Message handling error: " << e.what() << "\n";
                json state = game.toJSON();
                ws->send(state.dump(), uWS::OpCode::TEXT);
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