#include "Config.h"

// Initialize static member
json Config::config;

void Config::init() {
    ensure_config_dir();
    load_config();
}

const json& Config::get() {
    return config;
}

void Config::update(const json& new_config) {
    config = new_config;
    save_config();
}

bool Config::get_always_new_connection() {
    return config.value("always_new_connection", false);
}

bool Config::get_save_window_coords() {
    return config.value("save_window_coords", true);
}

void Config::ensure_config_dir() {
    auto config_path = get_config_path();
    if (!std::filesystem::exists(config_path.parent_path())) {
        std::filesystem::create_directories(config_path.parent_path());
    }
}

std::filesystem::path Config::get_config_path() {
    const char* home_dir = std::getenv("HOME");
    if (!home_dir) {
        throw std::runtime_error("Could not find home directory");
    }
    return std::filesystem::path(home_dir) / ".config" / "ngTerm" / "ngTerm.json";
}

void Config::load_config() {
    auto config_path = get_config_path();

    // Start with default configuration
    config = {
        {"window_width", 1024},
        {"window_height", 768},
        {"always_new_connection", false},
        {"save_window_coords", true}
    };

    // Load existing configuration if it exists
    if (std::filesystem::exists(config_path)) {
        try {
            std::ifstream file(config_path);
            if (file.is_open()) {
                file >> config;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error reading config file: " << e.what() << std::endl;
            // Continue with defaults if read fails
        }
    }
}

void Config::save_config() {
    try {
        auto config_path = get_config_path();
        ensure_config_dir();

        std::ofstream file(config_path);
        if (!file.is_open()) {
            std::cerr << "Failed to open config file for writing: " << config_path << std::endl;
            return;
        }

        file << config.dump(4);  // Pretty print with 4 spaces
        file.close();
    } catch (const std::exception& e) {
        std::cerr << "Error writing config file: " << e.what() << std::endl;
    }
}

bool Config::show_preferences_dialog(Gtk::Window& parent_window) {
    Gtk::Dialog dialog("Preferences", parent_window, true);
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Save", Gtk::RESPONSE_OK);
    dialog.set_default_size(400, -1);

    // Get the content area of the dialog
    Gtk::Box* content_area = dialog.get_content_area();
    content_area->set_spacing(12);
    content_area->set_margin_start(12);
    content_area->set_margin_end(12);
    content_area->set_margin_top(12);
    content_area->set_margin_bottom(12);

    // Connection Double-click Behavior
    Gtk::Frame connection_frame;
    connection_frame.set_label("Connection Double-click Behavior");
    Gtk::Box connection_box(Gtk::ORIENTATION_VERTICAL, 6);
    connection_box.set_margin_start(12);
    connection_box.set_margin_end(12);
    connection_box.set_margin_top(6);
    connection_box.set_margin_bottom(6);

    Gtk::RadioButton::Group conn_group;
    Gtk::RadioButton switch_radio("Switch to existing tab if connection is open");
    Gtk::RadioButton new_radio("Always open new connection");
    switch_radio.set_group(conn_group);
    new_radio.set_group(conn_group);

    // Set active based on config
    if (get_always_new_connection()) {
        new_radio.set_active();
    } else {
        switch_radio.set_active();
    }

    connection_box.pack_start(switch_radio, Gtk::PACK_SHRINK);
    connection_box.pack_start(new_radio, Gtk::PACK_SHRINK);
    connection_frame.add(connection_box);
    content_area->pack_start(connection_frame, Gtk::PACK_SHRINK);

    // Window Settings
    Gtk::Frame window_frame;
    window_frame.set_label("Window Settings");
    Gtk::Box window_box(Gtk::ORIENTATION_VERTICAL, 6);
    window_box.set_margin_start(12);
    window_box.set_margin_end(12);
    window_box.set_margin_top(6);
    window_box.set_margin_bottom(6);

    Gtk::CheckButton save_coords_check("Save window position and size on exit");
    save_coords_check.set_active(get_save_window_coords());

    window_box.pack_start(save_coords_check, Gtk::PACK_SHRINK);
    window_frame.add(window_box);
    content_area->pack_start(window_frame, Gtk::PACK_SHRINK);

    dialog.show_all();
    int result = dialog.run();

    if (result == Gtk::RESPONSE_OK) {
        bool config_changed = false;
        json new_config = config;

        // Update config only if values changed
        if (new_config.value("always_new_connection", false) != new_radio.get_active()) {
            new_config["always_new_connection"] = new_radio.get_active();
            config_changed = true;
        }

        if (new_config.value("save_window_coords", true) != save_coords_check.get_active()) {
            new_config["save_window_coords"] = save_coords_check.get_active();
            config_changed = true;
        }

        // If save_window_coords is disabled, remove window coordinates
        if (!save_coords_check.get_active()) {
            if (new_config.contains("window_x") || new_config.contains("window_y") ||
                new_config.contains("window_width") || new_config.contains("window_height")) {
                new_config.erase("window_x");
                new_config.erase("window_y");
                new_config.erase("window_width");
                new_config.erase("window_height");
                config_changed = true;
            }
        }

        // Save config if anything changed
        if (config_changed) {
            update(new_config);
        }

        return config_changed;
    }

    return false;
}