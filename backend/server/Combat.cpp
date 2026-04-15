#include "Combat.h"
#include <vector>
#include <cstdlib>

std::string resolveCombat(const std::string& playerChoice) {
    std::vector<std::string> choices = {"rock", "paper", "scissors"};
    std::string boss = choices[rand() % 3];

    if (playerChoice == boss) return "Tie";

    if ((playerChoice == "rock" && boss == "scissors") ||
        (playerChoice == "paper" && boss == "rock") ||
        (playerChoice == "scissors" && boss == "paper")) {
        return "Win";
    }
    return "Lose";
}