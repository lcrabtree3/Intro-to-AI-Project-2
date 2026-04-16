#include "AI.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>

namespace {
    bool g_modelLoaded = false;
    NarrativeConfig g_config;
    std::string g_modelStatus = "Fallback narrative generator active.";

    std::string truncateContext(const std::string& input, std::size_t maxLen = 700) {
        if (input.size() <= maxLen) {
            return input;
        }
        return input.substr(0, maxLen) + "...";
    }

    bool startsWith(const std::string& value, const std::string& prefix) {
        return value.rfind(prefix, 0) == 0;
    }

    int extractFirstPlayerNumber(const std::string& text) {
        std::size_t playerPos = text.find("Player ");
        if (playerPos == std::string::npos) return -1;
        std::size_t numPos = playerPos + 7;
        if (numPos >= text.size() || !std::isdigit(static_cast<unsigned char>(text[numPos]))) return -1;
        return text[numPos] - '0';
    }

    bool extractFirstCoordinate(const std::string& text, int& x, int& y) {
        std::size_t l = text.find('(');
        if (l == std::string::npos) return false;
        std::size_t comma = text.find(',', l + 1);
        std::size_t r = text.find(')', comma == std::string::npos ? l + 1 : comma + 1);
        if (comma == std::string::npos || r == std::string::npos) return false;

        const std::string xPart = text.substr(l + 1, comma - l - 1);
        const std::string yPart = text.substr(comma + 1, r - comma - 1);

        int parsedX = 0;
        int parsedY = 0;
        const auto xRes = std::from_chars(xPart.data(), xPart.data() + xPart.size(), parsedX);
        const auto yRes = std::from_chars(yPart.data(), yPart.data() + yPart.size(), parsedY);
        if (xRes.ec != std::errc() || yRes.ec != std::errc()) return false;

        x = parsedX;
        y = parsedY;
        return true;
    }

    std::string trim(const std::string& s) {
        std::size_t start = s.find_first_not_of(" \n\r\t");
        if (start == std::string::npos) {
            return "";
        }
        std::size_t end = s.find_last_not_of(" \n\r\t");
        return s.substr(start, end - start + 1);
    }

    std::string sanitizePromptArg(const std::string& input) {
        std::string out;
        out.reserve(input.size());
        for (char c : input) {
            if (c == '"') {
                out += '\'';
            } else if (c == '\n' || c == '\r') {
                out += ' ';
            } else {
                out += c;
            }
        }
        return out;
    }

    std::string fallbackNarrative(const std::string& context) {
        const int player = extractFirstPlayerNumber(context);
        int x = -1;
        int y = -1;
        const bool hasCoord = extractFirstCoordinate(context, x, y);
        const std::string p = player > 0 ? ("Player " + std::to_string(player)) : "The party";

        if (startsWith(context, "Exploration update.")) {
            std::ostringstream out;
            out << p << " pushes deeper into the wilds";
            if (hasCoord) {
                out << ", reaching tile (" << x << "," << y << ")";
            }
            out << " while searching for the key and signs of the locked portal.";
            return out.str();
        }

        if (startsWith(context, "Movement attempt.")) {
            return p + " tests the edge of the terrain, but the path refuses to open and the party holds position.";
        }

        if (startsWith(context, "Pickup event.")) {
            return p + " discovers useful gear on the ground and quickly stows it before continuing the hunt.";
        }

        if (startsWith(context, "Portal event.")) {
            if (context.find("boss appears") != std::string::npos) {
                return p + " unlocks the portal, and a violent surge of magic calls the boss into the battlefield.";
            }
            return p + " finds the ancient portal, but without the key its seal remains unbroken.";
        }

        if (startsWith(context, "Action update.")) {
            return p + " acts with caution as the world reacts, and the cooperative quest presses forward.";
        }

        if (startsWith(context, "Combat round.")) {
            return p + " clashes with the boss in a tense exchange as both sides fight for control of the portal chamber.";
        }

        if (startsWith(context, "Combat support action.")) {
            return p + " takes a breath amid the chaos and regains strength before the next strike.";
        }

        if (startsWith(context, "Victory event.")) {
            return "With a final coordinated blow, the boss falls and silence returns to the portal ruins.";
        }

        if (startsWith(context, "Defeat event.")) {
            return "The battle is lost, and the portal chamber fades into darkness as the heroes collapse.";
        }

        if (startsWith(context, "World state:")) {
            return "Two adventurers step into a mysterious wilderness, determined to find the key and awaken the portal's secret.";
        }

        return "The wind shifts across the map as the two adventurers continue their shared quest.";
    }

    std::string fallbackSummary(const std::string& narrative) {
        if (narrative.empty()) {
            return "No narrative available yet.";
        }
        return "Summary: " + truncateContext(narrative, 140);
    }

    bool looksLikeStatsOrPromptEcho(const std::string& text) {
        return text.find("AI Narrative:") != std::string::npos ||
               text.find("Exploration update.") != std::string::npos ||
               text.find("Action update.") != std::string::npos ||
               text.find("Player 1 inventory") != std::string::npos ||
               text.find("Player 2 inventory") != std::string::npos ||
               text.find("inventory:") != std::string::npos ||
               text.find("weapons=") != std::string::npos ||
               text.find("potions=") != std::string::npos ||
               text.find("key=") != std::string::npos ||
               text.find("health=") != std::string::npos;
    }

    std::string normalizeNarrativeOutput(const std::string& raw, const std::string& context) {
        std::string cleaned = trim(raw);
        if (cleaned.empty()) {
            return fallbackNarrative(context);
        }

        if (startsWith(cleaned, "AI Narrative:")) {
            cleaned = trim(cleaned.substr(std::string("AI Narrative:").size()));
        }

        if (looksLikeStatsOrPromptEcho(cleaned)) {
            return fallbackNarrative(context);
        }

        return cleaned;
    }

    std::string buildPrompt(const std::string& context) {
        std::ostringstream prompt;
        prompt
            << "You are the narrative engine for a cooperative fantasy dungeon game. "
            << "Write 2 to 4 vivid sentences in your own fantasy voice. "
            << "Be cinematic, atmospheric, and specific to the current scene. "
            << "Respect the exact world state and inventory in the context. "
            << "Do not contradict the context. "
            << "Do not add dialogue unless the context implies it. "
            << "Never output instructions, labels, or developer notes.\n\n"
            << "Context:\n"
            << truncateContext(context)
            << "\n\nNarrative:\n";
        return prompt.str();
    }

    bool fileExists(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        return f.good();
    }

    std::string runCommandCapture(const std::string& command) {
        std::array<char, 256> buffer{};
        std::string result;

        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
            return "";
        }

        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
            result += buffer.data();
        }

        pclose(pipe);
        return result;
    }
}

bool initializeNarrativeModel(const NarrativeConfig& config) {
    g_config = config;
    g_modelLoaded = false;

    if (!config.useModel) {
        g_modelStatus = "Narrative model disabled. Using fallback narrative generator.";
        return true;
    }

    if (!fileExists(config.modelPath)) {
        g_modelStatus = "Model file not found at: " + config.modelPath + ". Using fallback narrative generator.";
        return false;
    }

    if (!fileExists(config.cliPath)) {
        g_modelStatus = "llama-cli not found at: " + config.cliPath + ". Using fallback narrative generator.";
        return false;
    }

    g_modelLoaded = true;
    g_modelStatus = "Local GGUF narrative runtime enabled using llama-cli at " + config.cliPath + ".";
    return true;
}

bool isNarrativeModelLoaded() {
    return g_modelLoaded;
}

std::string getNarrativeModelStatus() {
    return g_modelStatus;
}

std::string generateNarrative(const std::string& context) {
    if (!g_modelLoaded) {
        return fallbackNarrative(context);
    }

    std::string prompt = buildPrompt(context);

    std::ostringstream cmd;
    cmd
        << "\"" << g_config.cliPath << "\" "
        << "-m \"" << g_config.modelPath << "\" "
        << "-n " << g_config.maxTokens << " "
        << "-c " << g_config.contextWindow << " "
        << "--temp " << g_config.temperature << " "
        << "--simple-io "
        << "--no-display-prompt "
        << "-p \"" << sanitizePromptArg(prompt) << "\" 2>NUL";

    std::string rawOutput = runCommandCapture(cmd.str());
    std::string cleaned = trim(rawOutput);

    if (cleaned.empty()) {
        return fallbackNarrative(context);
    }

    std::size_t narrativePos = cleaned.find("Narrative:");
    if (narrativePos != std::string::npos) {
        cleaned = trim(cleaned.substr(narrativePos + 10));
    }

    std::size_t stopPos = cleaned.find("<|");
    if (stopPos != std::string::npos) {
        cleaned = trim(cleaned.substr(0, stopPos));
    }

    return normalizeNarrativeOutput(cleaned, context);
}

std::string summarizeNarrative(const std::string& narrative) {
    if (narrative.empty()) {
        return "No narrative available yet.";
    }

    const std::string cleanedNarrative = normalizeNarrativeOutput(narrative, "World state:");

    if (!g_modelLoaded) {
        return fallbackSummary(cleanedNarrative);
    }

    std::ostringstream prompt;
    prompt
        << "Summarize the following game narrative in one concise sentence. "
        << "Preserve key game facts such as player actions, inventory, portal, and combat outcome when present.\n\n"
        << "Narrative:\n"
        << truncateContext(cleanedNarrative, 700)
        << "\n\nSummary:\n";

    std::ostringstream cmd;
    cmd
        << "\"" << g_config.cliPath << "\" "
        << "-m \"" << g_config.modelPath << "\" "
        << "-n 40 "
        << "-c " << g_config.contextWindow << " "
        << "--temp 0.4 "
        << "--simple-io "
        << "--no-display-prompt "
        << "-p \"" << sanitizePromptArg(prompt.str()) << "\" 2>NUL";

    std::string rawOutput = runCommandCapture(cmd.str());
    std::string cleaned = trim(rawOutput);

    if (cleaned.empty()) {
        return fallbackSummary(cleanedNarrative);
    }

    std::size_t summaryPos = cleaned.find("Summary:");
    if (summaryPos != std::string::npos) {
        cleaned = trim(cleaned.substr(summaryPos + 8));
    }

    std::size_t stopPos = cleaned.find("<|");
    if (stopPos != std::string::npos) {
        cleaned = trim(cleaned.substr(0, stopPos));
    }

    return cleaned.empty() ? fallbackSummary(cleanedNarrative) : cleaned;
}
