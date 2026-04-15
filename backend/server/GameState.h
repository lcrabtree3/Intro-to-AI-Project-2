#pragma once
#include <vector>
#include <string>
#include <nlohmann/json.hpp>
#include "Inventory.h"

struct Player {
    int x = 0;
    int y = 0;
    int health = 10;
    std::vector<ItemType> inventory;
};

class GameState {
public:
    GameState();

    void movePlayer(int id, int dx, int dy);
    void processAction(int id, const std::string& action);
    void combatTurn(int id, const std::string& choice);
    void usePotion(int id);
    void addChatMessage(int id, const std::string& text);

    void forceCombatForTesting(int startingPlayer = 0);

    nlohmann::json toJSON();

private:
    Player players[2];

    std::vector<std::vector<int>> map;
    std::vector<std::vector<bool>> fog;
    std::vector<Item> items;
    std::vector<std::string> chatLog;
    std::vector<std::string> actionLog;

    int portalX = 0;
    int portalY = 0;
    bool portalUnlocked = false;

    int bossHealth = 5;
    bool inCombat = false;
    int currentTurn = 0;

    bool combatSubmitted[2] = {false, false};
    std::string combatActions[2];

    std::string lastNarrative;

    void generateMap();
    void checkPickup(int id);
    void nextTurn();
    bool playerHasItem(int id, ItemType type) const;
    int countItem(int id, ItemType type) const;
    void removeOneItem(int id, ItemType type);
    int weaponBonus(int id) const;

    void appendActionHistory(const std::string& entry);
    void resetCombatRound();
    void finalizeCombatRound();
};