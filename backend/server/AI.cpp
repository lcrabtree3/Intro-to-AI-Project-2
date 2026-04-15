#include "AI.h"
#include <algorithm>
#include <array>
#include <cstdio>
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

    std::string trim(const std::string& s) {
        std::size_t start = s.find_first_not_of(" \n\r\t");
        if (start == std::string::npos) {
            return "";
        }
        std::size_t end = s.find_last_not_of(" \n\r\t");
        return s.substr(start, end - start + 1);
    }

    std::string shellEscapeSingleQuotes(const std::string& input) {
        std::string out;
        out.reserve(input.size() + 16);
        for (char c : input) {
            if (c == '\'') {
                out += "'\\''";
            } else {
                out += c;
            }
        }
        return out;
    }

    std::string fallbackNarrative(const std::string& context) {
        std::ostringstream out;
        out << "AI Narrative: " << truncateContext(context);
        return out.str();
    }

    std::string buildPrompt(const std::string& context) {
        std::ostringstream prompt;
        prompt
            << "You are the narrative engine for a cooperative fantasy dungeon game. "
            << "Write 2 to 4 vivid sentences in a clear fantasy style. "
            << "Respect the exact world state and inventory in the context. "
            << "Do not contradict the context. "
            << "Do not add dialogue unless the context implies it. "
            << "Keep it concise and game-ready.\n\n"
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
        << "'" << g_config.cliPath << "' "
        << "-m '" << g_config.modelPath << "' "
        << "-n " << g_config.maxTokens << " "
        << "-c " << g_config.contextWindow << " "
        << "--temp " << g_config.temperature << " "
        << "--simple-io "
        << "--no-display-prompt "
        << "-p '" << shellEscapeSingleQuotes(prompt) << "' 2>/dev/null";

    std::string rawOutput = runCommandCapture(cmd.str());
    std::string cleaned = trim(rawOutput);

    if (cleaned.empty()) {
        return fallbackNarrative(context);
    }

    std::size_t narrativePos = cleaned.find("Narrative:");
    if (narrativePos != std::string::npos) {
        cleaned = trim(cleaned.substr(narrativePos + 10));
    }

    if (cleaned.empty()) {
        return fallbackNarrative(context);
    }

    std::size_t stopPos = cleaned.find("<|");
    if (stopPos != std::string::npos) {
        cleaned = trim(cleaned.substr(0, stopPos));
    }

    return cleaned.empty() ? fallbackNarrative(context) : cleaned;
}