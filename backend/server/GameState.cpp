#include "GameState.h"
#include "MapGenerator.h"
#include "Combat.h"
#include "FogOfWar.h"
#include "AI.h"
#include <cstdlib>
#include <algorithm>

using json = nlohmann::json;

namespace {
    constexpr int MAP_SIZE = 20;
    constexpr int MAX_HEALTH = 10;

    std::string yesNo(bool value) {
        return value ? "yes" : "no";
    }
}

GameState::GameState() {
    generateMap();
}

void GameState::generateMap() {
    map = generateMapGrid(MAP_SIZE, MAP_SIZE);
    fog = std::vector<std::vector<bool>>(MAP_SIZE, std::vector<bool>(MAP_SIZE, false));
    items.clear();
    chatLog.clear();

    players[0] = Player{};
    players[1] = Player{};

    players[0].x = 1;
    players[0].y = 1;

    players[1].x = MAP_SIZE - 2;
    players[1].y = MAP_SIZE - 2;

    portalX = std::rand() % MAP_SIZE;
    portalY = std::rand() % MAP_SIZE;

    if ((portalX == players[0].x && portalY == players[0].y) ||
        (portalX == players[1].x && portalY == players[1].y)) {
        portalX = MAP_SIZE / 2;
        portalY = MAP_SIZE / 2;
    }

    items.push_back({KEY, std::rand() % MAP_SIZE, std::rand() % MAP_SIZE});
    items.push_back({POTION, std::rand() % MAP_SIZE, std::rand() % MAP_SIZE});
    items.push_back({POTION, std::rand() % MAP_SIZE, std::rand() % MAP_SIZE});
    items.push_back({WEAPON, std::rand() % MAP_SIZE, std::rand() % MAP_SIZE});
    items.push_back({WEAPON, std::rand() % MAP_SIZE, std::rand() % MAP_SIZE});

    portalUnlocked = false;
    bossHealth = 5;
    inCombat = false;
    currentTurn = 0;

    reveal(fog, players[0].x, players[0].y, 2);
    reveal(fog, players[1].x, players[1].y, 2);

    lastNarrative = generateNarrative(
        "World state: two players enter a shared fantasy landscape. "
        "Player 1 begins at (" + std::to_string(players[0].x) + "," + std::to_string(players[0].y) + "). "
        "Player 2 begins at (" + std::to_string(players[1].x) + "," + std::to_string(players[1].y) + "). "
        "A locked portal exists somewhere on the map. "
        "The players must explore together, recover the KEY, gather POTION and WEAPON items, "
        "and prepare for a boss encounter."
    );
}

bool GameState::playerHasItem(int id, ItemType type) const {
    for (auto item : players[id].inventory) {
        if (item == type) return true;
    }
    return false;
}

int GameState::countItem(int id, ItemType type) const {
    int count = 0;
    for (auto item : players[id].inventory) {
        if (item == type) count++;
    }
    return count;
}

void GameState::removeOneItem(int id, ItemType type) {
    auto& inv = players[id].inventory;
    for (auto it = inv.begin(); it != inv.end(); ++it) {
        if (*it == type) {
            inv.erase(it);
            return;
        }
    }
}

int GameState::weaponBonus(int id) const {
    return countItem(id, WEAPON);
}

void GameState::checkPickup(int id) {
    for (auto it = items.begin(); it != items.end();) {
        if (it->x == players[id].x && it->y == players[id].y) {
            ItemType pickedType = it->type;
            players[id].inventory.push_back(pickedType);
            it = items.erase(it);

            lastNarrative = generateNarrative(
                "Pickup event. Player " + std::to_string(id + 1) +
                " steps onto an item tile and collects " + itemToString(pickedType) + ". "
                "Player " + std::to_string(id + 1) +
                " now has weapons=" + std::to_string(countItem(id, WEAPON)) +
                ", potions=" + std::to_string(countItem(id, POTION)) +
                ", key=" + yesNo(playerHasItem(id, KEY)) + "."
            );
        } else {
            ++it;
        }
    }
}

void GameState::nextTurn() {
    currentTurn = (currentTurn + 1) % 2;
}

void GameState::movePlayer(int id, int dx, int dy) {
    if (id != currentTurn || inCombat) return;

    int newX = players[id].x + dx;
    int newY = players[id].y + dy;

    newX = std::max(0, std::min(MAP_SIZE - 1, newX));
    newY = std::max(0, std::min(MAP_SIZE - 1, newY));

    if (newX == players[id].x && newY == players[id].y) {
        lastNarrative = generateNarrative(
            "Movement attempt. Player " + std::to_string(id + 1) +
            " tries to move but remains in place at tile (" +
            std::to_string(players[id].x) + "," + std::to_string(players[id].y) + ")."
        );
        nextTurn();
        return;
    }

    players[id].x = newX;
    players[id].y = newY;

    reveal(fog, players[id].x, players[id].y, 2);
    checkPickup(id);

    if (players[id].x == portalX && players[id].y == portalY) {
        if (playerHasItem(id, KEY)) {
            portalUnlocked = true;
            inCombat = true;
            lastNarrative = generateNarrative(
                "Portal event. Player " + std::to_string(id + 1) +
                " reaches the portal while carrying the KEY. "
                "Ancient energy surges through the shared battlefield and the boss appears. "
                "Combat begins now."
            );
            return;
        } else {
            lastNarrative = generateNarrative(
                "Portal event. Player " + std::to_string(id + 1) +
                " has discovered the portal at (" + std::to_string(portalX) + "," + std::to_string(portalY) + "), "
                "but the seal remains locked because the KEY has not been brought here yet."
            );
            nextTurn();
            return;
        }
    }

    lastNarrative = generateNarrative(
        "Exploration update. Player " + std::to_string(id + 1) +
        " moves to tile (" + std::to_string(players[id].x) + "," + std::to_string(players[id].y) + "). "
        "Player 1 inventory: weapons=" + std::to_string(countItem(0, WEAPON)) +
        ", potions=" + std::to_string(countItem(0, POTION)) +
        ", key=" + yesNo(playerHasItem(0, KEY)) + ". "
        "Player 2 inventory: weapons=" + std::to_string(countItem(1, WEAPON)) +
        ", potions=" + std::to_string(countItem(1, POTION)) +
        ", key=" + yesNo(playerHasItem(1, KEY)) + ". "
        "Describe the newly explored area in a fantasy tone."
    );

    nextTurn();
}

void GameState::processAction(int id, const std::string& action) {
    if (id != currentTurn || inCombat) return;

    lastNarrative = generateNarrative(
        "Action update. Player " + std::to_string(id + 1) +
        " attempts the following action: " + action + ". "
        "Player 1 status: health=" + std::to_string(players[0].health) +
        ", weapons=" + std::to_string(countItem(0, WEAPON)) +
        ", potions=" + std::to_string(countItem(0, POTION)) +
        ", key=" + yesNo(playerHasItem(0, KEY)) + ". "
        "Player 2 status: health=" + std::to_string(players[1].health) +
        ", weapons=" + std::to_string(countItem(1, WEAPON)) +
        ", potions=" + std::to_string(countItem(1, POTION)) +
        ", key=" + yesNo(playerHasItem(1, KEY)) + ". "
        "Narrate the result while respecting the inventory and current world state."
    );

    nextTurn();
}

void GameState::combatTurn(int id, const std::string& choice) {
    if (id != currentTurn || !inCombat) return;

    std::string result = resolveCombat(choice);

    if (result == "Win") {
        int damage = 1 + weaponBonus(id);
        bossHealth -= damage;
        if (bossHealth < 0) bossHealth = 0;

        lastNarrative = generateNarrative(
            "Combat round. Player " + std::to_string(id + 1) +
            " chooses " + choice + " and defeats the boss move. "
            "Damage dealt: " + std::to_string(damage) + ". "
            "Boss health is now " + std::to_string(bossHealth) + "."
        );
    } else if (result == "Tie") {
        players[id].health = std::max(0, players[id].health - 1);

        lastNarrative = generateNarrative(
            "Combat round. Player " + std::to_string(id + 1) +
            " chooses " + choice + " and ties with the boss. "
            "Player " + std::to_string(id + 1) +
            " still takes 1 damage and now has health " + std::to_string(players[id].health) + "."
        );
    } else {
        players[id].health = std::max(0, players[id].health - 1);

        lastNarrative = generateNarrative(
            "Combat round. Player " + std::to_string(id + 1) +
            " chooses " + choice + " and loses the exchange. "
            "Player " + std::to_string(id + 1) +
            " takes 1 damage and now has health " + std::to_string(players[id].health) + "."
        );
    }

    if (bossHealth <= 0) {
        inCombat = false;
        lastNarrative = generateNarrative(
            "Victory event. The boss collapses after the cooperative assault. "
            "The portal chamber falls silent and the players survive the encounter."
        );
        return;
    }

    if (players[0].health <= 0 && players[1].health <= 0) {
        inCombat = false;
        lastNarrative = generateNarrative(
            "Defeat event. Both players have fallen during the boss encounter. "
            "The portal remains in darkness."
        );
        return;
    }

    nextTurn();
}

void GameState::usePotion(int id) {
    if (id != currentTurn || !inCombat) return;

    if (!playerHasItem(id, POTION)) {
        lastNarrative = generateNarrative(
            "Combat support action. Player " + std::to_string(id + 1) +
            " tries to use a POTION, but none remain in inventory."
        );
        return;
    }

    removeOneItem(id, POTION);
    players[id].health = std::min(players[id].health + 3, MAX_HEALTH);

    lastNarrative = generateNarrative(
        "Combat support action. Player " + std::to_string(id + 1) +
        " drinks a POTION and recovers health. "
        "Player " + std::to_string(id + 1) +
        " now has health " + std::to_string(players[id].health) +
        " and potions=" + std::to_string(countItem(id, POTION)) + "."
    );

    nextTurn();
}

void GameState::addChatMessage(int id, const std::string& text) {
    chatLog.push_back("Player " + std::to_string(id + 1) + ": " + text);

    if (chatLog.size() > 30) {
        chatLog.erase(chatLog.begin());
    }
}

json GameState::toJSON() {
    return {
        {"players", {
            {
                {"x", players[0].x},
                {"y", players[0].y},
                {"health", players[0].health},
                {"inventory", players[0].inventory},
                {"weaponCount", countItem(0, WEAPON)},
                {"potionCount", countItem(0, POTION)},
                {"hasKey", playerHasItem(0, KEY)}
            },
            {
                {"x", players[1].x},
                {"y", players[1].y},
                {"health", players[1].health},
                {"inventory", players[1].inventory},
                {"weaponCount", countItem(1, WEAPON)},
                {"potionCount", countItem(1, POTION)},
                {"hasKey", playerHasItem(1, KEY)}
            }
        }},
        {"map", map},
        {"fog", fog},
        {"items", items},
        {"portal", {portalX, portalY}},
        {"portalUnlocked", portalUnlocked},
        {"bossHealth", bossHealth},
        {"inCombat", inCombat},
        {"currentTurn", currentTurn},
        {"chat", chatLog},
        {"narrative", lastNarrative},
        {"modelLoaded", isNarrativeModelLoaded()}
    };
}