#include "Config.h"

// Function to manage application configuration
void manage_config() {
    // Get the path to the user's home directory
    const char* home_dir = std::getenv("HOME");
    if (!home_dir) {
        std::cerr << "Could not find home directory" << std::endl;
        return;
    }

    // Construct the path to the ngTerm config directory
    std::filesystem::path config_dir =
        std::filesystem::path(home_dir) / ".config" / "ngTerm";

    // Create the directory if it doesn't exist
    try {
        if (!std::filesystem::exists(config_dir)) {
            std::filesystem::create_directories(config_dir);
            std::cout << "Created config directory: " << config_dir << std::endl;
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error creating config directory: " << e.what() << std::endl;
        return;
    }

    // Path to the configuration file
    std::filesystem::path config_file = config_dir / "ngTerm.json";

    // Create default configuration only if the file doesn't exist
    if (!std::filesystem::exists(config_file)) {
        try {
            // Create default configuration
            json default_config = {
                {"window_width", 1024},
                {"window_height", 768}
            };

            // Write default configuration to file
            std::ofstream file(config_file);
            if (!file.is_open()) {
                std::cerr << "Failed to open config file for writing: " << config_file << std::endl;
                return;
            }

            file << default_config.dump(4);  // Pretty print with 4 spaces
            file.close();

            std::cout << "Created default configuration file: " << config_file << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error writing config file: " << e.what() << std::endl;
        }
    }
}

// Function to read configuration
json read_config() {
    // Get the path to the user's home directory
    const char* home_dir = std::getenv("HOME");
    if (!home_dir) {
        std::cerr << "Could not find home directory" << std::endl;
        return {
            {"window_width", 1024},
            {"window_height", 768}
        };
    }

    // Construct the path to the configuration file
    std::filesystem::path config_file =
        std::filesystem::path(home_dir) / ".config" / "ngTerm" / "ngTerm.json";

    try {
        // First, ensure the config file exists
        if (!std::filesystem::exists(config_file)) {
            std::cerr << "Config file does not exist: " << config_file << std::endl;
            manage_config();  // Try to create the config file
        }

        std::ifstream file(config_file);
        if (!file.is_open()) {
            std::cerr << "Could not open config file: " << config_file << std::endl;
            return {
                {"window_width", 1024},
                {"window_height", 768}
            };
        }

        json config;
        file >> config;
        file.close(); // Close the file after reading
        return config;
    } catch (const std::exception& e) {
        std::cerr << "Error reading config file: " << e.what() << std::endl;
        return {
            {"window_width", 1024},
            {"window_height", 768}
        };
    }
}

// Function to save configuration
void save_config(const json& config) {
    // Get the path to the user's home directory
    const char* home_dir = std::getenv("HOME");
    if (!home_dir) {
        std::cerr << "Could not find home directory" << std::endl;
        return;
    }

    // Construct the path to the configuration file
    std::filesystem::path config_file =
        std::filesystem::path(home_dir) / ".config" / "ngTerm" / "ngTerm.json";

    try {
        // Ensure the directory exists
        if (!std::filesystem::exists(config_file.parent_path())) {
            std::filesystem::create_directories(config_file.parent_path());
        }

        // Write configuration to file
        std::ofstream file(config_file);
        if (!file.is_open()) {
            std::cerr << "Failed to open config file for writing: " << config_file << std::endl;
            return;
        }

        file << config.dump(4);  // Pretty print with 4 spaces
        file.close();

    } catch (const std::exception& e) {
        std::cerr << "Error writing config file: " << e.what() << std::endl;
    }
}