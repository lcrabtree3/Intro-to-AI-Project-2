#pragma once
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

enum ItemType {
    NONE,
    KEY,
    POTION,
    WEAPON
};

struct Item {
    ItemType type;
    int x, y;
};

std::string itemToString(ItemType type);

inline void to_json(json& j, const ItemType& type) {
    j = itemToString(type);
}

inline void to_json(json& j, const Item& item) {
    j = {
        {"type", item.type},
        {"x", item.x},
        {"y", item.y}
    };
}