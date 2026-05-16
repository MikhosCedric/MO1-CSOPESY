#pragma once
#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <string>

// Handled by Pair B (FileSystem Module)
// Responsible for reading the config.txt file and pushing the data to the GlobalConfig
class ConfigParser {
public:
    // Reads the file and returns true if successful, false if it fails
    static bool loadConfig(const std::string& filePath);
};

#endif // CONFIG_PARSER_H