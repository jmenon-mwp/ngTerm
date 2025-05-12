#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <filesystem>
#include <fstream> // For std::ofstream, std::ifstream
#include <iostream> // For std::cerr, std::cout
#include <cstdlib> // For std::getenv
#include <nlohmann/json.hpp>

// Use the nlohmann::json namespace
using json = nlohmann::json;

// Function to ensure the configuration directory and a default config file exist.
// Creates a default config if one isn't present.
void manage_config();

// Function to read configuration from ngTerm.json
// Returns a json object containing the configuration.
// If the file doesn't exist or an error occurs, returns a default configuration.
json read_config();

// Function to save configuration to ngTerm.json
// Takes a const json reference to the configuration object to be saved.
void save_config(const json& config);

#endif // CONFIG_H