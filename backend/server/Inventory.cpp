#include "Inventory.h"

std::string itemToString(ItemType type) {
    switch (type) {
        case KEY: return "KEY";
        case POTION: return "POTION";
        case WEAPON: return "WEAPON";
        default: return "NONE";
    }
}