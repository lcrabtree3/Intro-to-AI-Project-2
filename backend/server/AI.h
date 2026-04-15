#pragma once
#include <string>

struct NarrativeConfig {
    bool useModel = false;
    std::string modelPath;
    std::string cliPath = "../../llama.cpp/build/bin/llama-cli";
    int maxTokens = 60;
    int contextWindow = 512;
    float temperature = 0.8f;
};

bool initializeNarrativeModel(const NarrativeConfig& config);
std::string generateNarrative(const std::string& context);
std::string summarizeNarrative(const std::string& narrative);
bool isNarrativeModelLoaded();
std::string getNarrativeModelStatus();
