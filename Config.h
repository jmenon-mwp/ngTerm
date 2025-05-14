#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <gtkmm.h>

// Use the nlohmann::json namespace
using json = nlohmann::json;

class Config {
public:
    // Initialize configuration system
    static void init();

    // Get the current configuration
    static const json& get();

    // Update and save configuration
    static void update(const json& new_config);

    // Configuration getters with default values
    static bool get_always_new_connection();
    static bool get_save_window_coords();

    // Function to show and handle the preferences dialog
    // Returns true if configuration was changed, false otherwise
    static bool show_preferences_dialog(Gtk::Window& parent_window);

private:
    static json config;
    static void load_config();
    static void save_config();
    static std::filesystem::path get_config_path();
    static void ensure_config_dir();
};

#endif // CONFIG_H