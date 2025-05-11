#include <gtkmm.h>
#include <vte/vte.h>
#include <iostream>
#include <vector>
#include <string>
#include <glib.h> // For Glib basic types if needed
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/frame.h>
#include <gtkmm/grid.h>
#include <gtkmm/box.h>      // For Gtk::Box
#include <gtkmm/toolbar.h>  // For Gtk::Toolbar
#include <gtkmm/toolbutton.h> // For Gtk::ToolButton
#include <gdkmm/pixbuf.h>   // For Gdk::Pixbuf for icons
#include "TreeModelColumns.h" // Include the new header for ConnectionColumns

#include "Connections.h"
#include "Folders.h"

using json = nlohmann::json;

// Global TreeView, Model, and Columns
Gtk::TreeView* connections_treeview = nullptr;
Glib::RefPtr<Gtk::TreeStore> connections_liststore; // Stays as RefPtr
ConnectionColumns connection_columns; // Use the custom ConnectionColumns struct

// Global UI elements for Info Panel
Gtk::Frame* info_frame = nullptr;
Gtk::Grid* info_grid = nullptr;
Gtk::Label* host_value_label = nullptr;
Gtk::Label* type_value_label = nullptr;
Gtk::Label* port_value_label = nullptr;

// Global MenuItems for Edit functionality
Gtk::MenuItem* edit_folder_menu_item = nullptr;
Gtk::MenuItem* edit_connection_menu_item = nullptr;

// Global ToolButtons for Edit/Delete functionality on the toolbar
Gtk::ToolButton* edit_folder_menu_item_toolbar = nullptr;
Gtk::ToolButton* delete_folder_menu_item_toolbar = nullptr;
Gtk::ToolButton* edit_connection_menu_item_toolbar = nullptr;
Gtk::ToolButton* delete_connection_menu_item_toolbar = nullptr;

// Function to handle selection changes in the TreeView
void on_connection_selection_changed() {
    if (!connections_treeview || !host_value_label || !type_value_label || !port_value_label) return; // Guard against null pointers

    Glib::RefPtr<Gtk::TreeSelection> selection = connections_treeview->get_selection();
    bool is_folder_selected = false;
    bool is_connection_selected = false;

    if (selection) {
        Gtk::TreeModel::iterator iter = selection->get_selected();
        if (iter) {
            Gtk::TreeModel::Row row = *iter;
            bool is_connection = row[connection_columns.is_connection];
            bool is_folder = row[connection_columns.is_folder];

            if (is_connection) {
                host_value_label->set_text(row[connection_columns.host]);
                type_value_label->set_text(row[connection_columns.connection_type]);
                port_value_label->set_text(row[connection_columns.port]);
                is_connection_selected = true;
            } else if (is_folder) {
                host_value_label->set_text("Folder Selected");
                type_value_label->set_text("");
                port_value_label->set_text("");
                is_folder_selected = true;
            } else {
                host_value_label->set_text("");
                type_value_label->set_text("");
                port_value_label->set_text("");
            }
        } else {
            host_value_label->set_text("");
            type_value_label->set_text("");
            port_value_label->set_text("");
        }
    } else {
        host_value_label->set_text("");
        type_value_label->set_text("");
        port_value_label->set_text("");
    }

    // Update sensitivity of edit menu items
    if (edit_folder_menu_item) {
        edit_folder_menu_item->set_sensitive(is_folder_selected);
    }
    if (edit_connection_menu_item) {
        edit_connection_menu_item->set_sensitive(is_connection_selected);
    }

    // Update sensitivity of toolbar buttons
    if (edit_folder_menu_item_toolbar) {
        edit_folder_menu_item_toolbar->set_sensitive(is_folder_selected);
    }
    if (delete_folder_menu_item_toolbar) {
        delete_folder_menu_item_toolbar->set_sensitive(is_folder_selected);
    }
    if (edit_connection_menu_item_toolbar) {
        edit_connection_menu_item_toolbar->set_sensitive(is_connection_selected);
    }
    if (delete_connection_menu_item_toolbar) {
        delete_connection_menu_item_toolbar->set_sensitive(is_connection_selected);
    }
}

// Function to handle editing a connection
void on_edit_connection_activate() {
    if (!connections_treeview || !connections_liststore || !edit_connection_menu_item) return;

    Glib::RefPtr<Gtk::TreeSelection> selection = connections_treeview->get_selection();
    Gtk::TreeModel::iterator iter = selection->get_selected();

    if (!iter) {
        Gtk::MessageDialog warning_dialog("No Connection Selected", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true);
        warning_dialog.set_secondary_text("Please select a connection to edit.");
        warning_dialog.run();
        return;
    }

    Gtk::TreeModel::Row row = *iter;
    if (!row[connection_columns.is_connection]) {
        Gtk::MessageDialog warning_dialog("Not a Connection", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true);
        warning_dialog.set_secondary_text("The selected item is not a connection.");
        warning_dialog.run();
        return;
    }

    ConnectionInfo current_connection;
    current_connection.id = static_cast<Glib::ustring>(row[connection_columns.id]);
    current_connection.name = static_cast<Glib::ustring>(row[connection_columns.name]);
    current_connection.host = static_cast<Glib::ustring>(row[connection_columns.host]);
    // Ensure port column actually has a string that can be converted to int
    Glib::ustring port_ustr = row[connection_columns.port];
    if (!port_ustr.empty()) {
        try {
            current_connection.port = std::stoi(port_ustr.raw());
        } catch (const std::exception& e) {
            // Handle error or set a default port, e.g., 0 or -1
            current_connection.port = 0;
            std::cerr << "Error converting port: " << port_ustr << " for connection " << current_connection.name << std::endl;
        }
    } else {
        current_connection.port = 0; // Default if port string is empty
    }
    current_connection.username = static_cast<Glib::ustring>(row[connection_columns.username]);
    current_connection.connection_type = static_cast<Glib::ustring>(row[connection_columns.connection_type]);
    current_connection.folder_id = static_cast<Glib::ustring>(row[connection_columns.parent_id_col]); // parent_id_col stores folder_id for connections

    Gtk::Dialog dialog("Edit Connection", true /* modal */);
    dialog.set_default_size(400, 350);

    Gtk::Grid* grid = Gtk::manage(new Gtk::Grid());
    grid->set_border_width(10);
    grid->set_column_spacing(10);
    grid->set_row_spacing(10);
    dialog.get_content_area()->pack_start(*grid, Gtk::PACK_EXPAND_WIDGET);

    // Folder selection (ComboBoxText)
    Gtk::Label folder_label("Folder:");
    folder_label.set_halign(Gtk::ALIGN_START);
    Gtk::ComboBoxText folder_combo;
    folder_combo.set_hexpand(true);
    std::vector<FolderInfo> folders = ConnectionManager::load_folders();
    folder_combo.append("", "None (Root Level)"); // ID, Text for root
    int folder_active_idx = 0;
    int current_idx = 1;
    for (const auto& folder : folders) {
        folder_combo.append(folder.id, folder.name);
        if (folder.id == current_connection.folder_id) {
            folder_active_idx = current_idx;
        }
        current_idx++;
    }
    folder_combo.set_active(folder_active_idx);

    // Connection Type (ComboBoxText)
    Gtk::Label type_label("Connection Type:");
    type_label.set_halign(Gtk::ALIGN_START);
    Gtk::ComboBoxText type_combo;
    type_combo.set_hexpand(true);
    const char* types[] = {"SSH", "Telnet", "VNC", "RDP"};
    for (const char* type_str : types) {
        type_combo.append(type_str, type_str);
    }
    type_combo.set_active_text(current_connection.connection_type);

    // Connection Name
    Gtk::Label name_label("Connection Name:");
    name_label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry name_entry;
    name_entry.set_hexpand(true);
    name_entry.set_text(current_connection.name);

    // Host
    Gtk::Label host_label("Host:");
    host_label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry host_entry;
    host_entry.set_hexpand(true);
    host_entry.set_text(current_connection.host);

    // Port
    Gtk::Label port_label("Port:");
    port_label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry port_entry;
    port_entry.set_hexpand(true);
    port_entry.set_text(current_connection.port > 0 ? std::to_string(current_connection.port) : "");

    // Username
    Gtk::Label user_label("Username:");
    user_label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry user_entry;
    user_entry.set_hexpand(true);
    user_entry.set_text(current_connection.username);

    // Attach to grid
    grid->attach(folder_label, 0, 0, 1, 1);
    grid->attach(folder_combo, 1, 0, 1, 1);
    grid->attach(type_label, 0, 1, 1, 1);
    grid->attach(type_combo, 1, 1, 1, 1);
    grid->attach(name_label, 0, 2, 1, 1);
    grid->attach(name_entry, 1, 2, 1, 1);
    grid->attach(host_label, 0, 3, 1, 1);
    grid->attach(host_entry, 1, 3, 1, 1);
    grid->attach(port_label, 0, 4, 1, 1);
    grid->attach(port_entry, 1, 4, 1, 1);
    grid->attach(user_label, 0, 5, 1, 1);
    grid->attach(user_entry, 1, 5, 1, 1);

    // Auto-fill port based on connection type
    type_combo.signal_changed().connect([&type_combo, &port_entry, current_connection]() {
        std::string selected_type = type_combo.get_active_text();
        std::string current_port_text = port_entry.get_text();
        bool port_was_default_for_old_type = false;
        if (current_connection.connection_type == "SSH" && current_port_text == "22") port_was_default_for_old_type = true;
        else if (current_connection.connection_type == "Telnet" && current_port_text == "23") port_was_default_for_old_type = true;
        else if (current_connection.connection_type == "VNC" && current_port_text == "5900") port_was_default_for_old_type = true;
        else if (current_connection.connection_type == "RDP" && current_port_text == "3389") port_was_default_for_old_type = true;

        if (current_port_text.empty() || port_was_default_for_old_type) {
            if (selected_type == "SSH") port_entry.set_text("22");
            else if (selected_type == "Telnet") port_entry.set_text("23");
            else if (selected_type == "VNC") port_entry.set_text("5900");
            else if (selected_type == "RDP") port_entry.set_text("3389");
            else if (port_was_default_for_old_type) port_entry.set_text(""); // Clear if old was default and new has no default
        }
    });

    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    Gtk::Button* save_button = dialog.add_button("_Save", Gtk::RESPONSE_OK);
    save_button->get_style_context()->add_class("suggested-action");

    dialog.show_all();
    int response = dialog.run();

    if (response == Gtk::RESPONSE_OK) {
        ConnectionInfo updated_connection = current_connection; // Keep original ID
        updated_connection.name = name_entry.get_text();
        updated_connection.host = host_entry.get_text();
        updated_connection.username = user_entry.get_text();
        updated_connection.connection_type = type_combo.get_active_text();
        updated_connection.folder_id = folder_combo.get_active_id(); // Get ID from ComboBox

        std::string port_str = port_entry.get_text();
        if (!port_str.empty()) {
            try {
                updated_connection.port = std::stoi(port_str);
            } catch (const std::exception& e) {
                Gtk::MessageDialog error_dialog(dialog, "Invalid Port", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
                error_dialog.set_secondary_text("The port number entered is invalid.");
                error_dialog.run();
                return;
            }
        } else {
            updated_connection.port = 0; // Default if port is empty
        }

        if (updated_connection.name.empty() || updated_connection.host.empty()) {
             Gtk::MessageDialog error_dialog(dialog, "Missing Information", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true);
             error_dialog.set_secondary_text("Connection Name and Host cannot be empty.");
             error_dialog.run();
             return;
        }

        if (ConnectionManager::save_connection(updated_connection)) {
            populate_connections_treeview(connections_liststore, connection_columns, *connections_treeview);
        } else {
            Gtk::MessageDialog error_dialog(dialog, "Save Failed", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
            error_dialog.set_secondary_text("Could not update the connection.");
            error_dialog.run();
        }
    }
}

// Function to handle adding a new connection
void on_add_connection_activated(Gtk::Notebook& notebook) {
    Gtk::Dialog dialog("Add New Connection", true /* modal */);
    dialog.set_default_size(400, 350); // Adjusted default size

    Gtk::Grid* grid = Gtk::manage(new Gtk::Grid());
    grid->set_border_width(10);
    grid->set_column_spacing(10);
    grid->set_row_spacing(10);
    dialog.get_content_area()->pack_start(*grid, Gtk::PACK_EXPAND_WIDGET);

    // --- Determine initially selected folder from TreeView ---
    std::string initially_selected_folder_id = ""; // Default to root/none
    if (connections_treeview) { // Ensure the global treeview pointer is valid
        Glib::RefPtr<Gtk::TreeSelection> selection = connections_treeview->get_selection();
        if (selection) {
            Gtk::TreeModel::iterator iter = selection->get_selected();
            if (iter) {
                Gtk::TreeModel::Row row = *iter;
                if (row[connection_columns.is_folder]) { // Check if it's a folder
                    initially_selected_folder_id = static_cast<Glib::ustring>(row[connection_columns.id]);
                }
            }
        }
    }
    // --- End of determining initially selected folder ---

    // Folder selection (ComboBoxText)
    Gtk::Label folder_label("Folder:");
    folder_label.set_halign(Gtk::ALIGN_START);
    Gtk::ComboBoxText folder_combo;
    folder_combo.set_hexpand(true);
    std::vector<FolderInfo> folders = ConnectionManager::load_folders();
    folder_combo.append("", "None (Root Level)"); // Option for no folder
    for (const auto& folder : folders) {
        folder_combo.append(folder.id, folder.name);
    }
    // folder_combo.set_active(0); // Default to "None" - Replaced by smarter selection

    // Set the active item in the combo box based on treeview selection or default to None (Root Level)
    if (!initially_selected_folder_id.empty()) {
        folder_combo.set_active_id(initially_selected_folder_id); // set_active_id handles non-existent IDs gracefully
    } else {
        folder_combo.set_active(0); // Default to "None (Root Level)" which is the first item (index 0)
    }

    // Connection Type (ComboBoxText) - NEW
    Gtk::Label type_label("Connection Type:");
    type_label.set_halign(Gtk::ALIGN_START);
    Gtk::ComboBoxText type_combo;
    type_combo.set_hexpand(true);
    type_combo.append("SSH", "SSH");
    type_combo.append("Telnet", "Telnet");
    type_combo.append("VNC", "VNC");
    type_combo.append("RDP", "RDP");
    type_combo.set_active_text("SSH"); // Default to SSH

    // Connection Name
    Gtk::Label name_label("Connection Name:");
    name_label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry name_entry;
    name_entry.set_hexpand(true);

    // Host
    Gtk::Label host_label("Host:");
    host_label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry host_entry;
    host_entry.set_hexpand(true);

    // Port
    Gtk::Label port_label("Port:");
    port_label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry port_entry;
    port_entry.set_hexpand(true);

    // Username
    Gtk::Label user_label("Username:");
    user_label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry user_entry;
    user_entry.set_hexpand(true);

    // Attach to grid
    // Row 0: Folder
    grid->attach(folder_label, 0, 0, 1, 1);
    grid->attach(folder_combo, 1, 0, 1, 1);
    // Row 1: Connection Type - NEW
    grid->attach(type_label, 0, 1, 1, 1);
    grid->attach(type_combo, 1, 1, 1, 1);
    // Row 2: Name
    grid->attach(name_label, 0, 2, 1, 1);
    grid->attach(name_entry, 1, 2, 1, 1);
    // Row 3: Host
    grid->attach(host_label, 0, 3, 1, 1);
    grid->attach(host_entry, 1, 3, 1, 1);
    // Row 4: Port
    grid->attach(port_label, 0, 4, 1, 1);
    grid->attach(port_entry, 1, 4, 1, 1);
    // Row 5: Username
    grid->attach(user_label, 0, 5, 1, 1);
    grid->attach(user_entry, 1, 5, 1, 1);

    // Auto-fill port based on connection type - NEW
    type_combo.signal_changed().connect([&type_combo, &port_entry]() {
        std::string selected_type = type_combo.get_active_text();
        std::string current_port = port_entry.get_text();
        if (current_port.empty()) {
            if (selected_type == "SSH") port_entry.set_text("22");
            else if (selected_type == "Telnet") port_entry.set_text("23");
            else if (selected_type == "VNC") port_entry.set_text("5900");
            else if (selected_type == "RDP") port_entry.set_text("3389");
        }
    });
    // Trigger it once to set default port for SSH if port is empty
    if (port_entry.get_text().empty() && type_combo.get_active_text() == "SSH") {
         port_entry.set_text("22");
    }


    // Buttons
    dialog.add_button("Cancel", Gtk::RESPONSE_CANCEL);
    Gtk::Button* save_button = dialog.add_button("Save", Gtk::RESPONSE_OK);
    save_button->get_style_context()->add_class("suggested-action"); // Optional: make save button prominent

    dialog.show_all();

    int result = dialog.run();

    if (result == Gtk::RESPONSE_OK) {
        ConnectionInfo new_connection;
        new_connection.id = ConnectionManager::generate_connection_id();
        new_connection.name = name_entry.get_text();
        new_connection.host = host_entry.get_text();
        new_connection.username = user_entry.get_text();
        new_connection.connection_type = type_combo.get_active_text(); // Save selected type

        std::string port_str = port_entry.get_text();
        if (!port_str.empty()) {
            try {
                new_connection.port = std::stoi(port_str);
            } catch (const std::invalid_argument& ia) {
                std::cerr << "Invalid port number: " << port_str << std::endl;
                // Optionally show an error dialog to the user
                Gtk::MessageDialog error_dialog(dialog, "Invalid Port Number", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
                error_dialog.set_secondary_text("The port number entered is not a valid number.");
                error_dialog.run();
                return; // Don't save if port is invalid
            } catch (const std::out_of_range& oor) {
                std::cerr << "Port number out of range: " << port_str << std::endl;
                Gtk::MessageDialog error_dialog(dialog, "Port Number Out of Range", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
                error_dialog.set_secondary_text("The port number is too large or too small.");
                error_dialog.run();
                return; // Don't save if port is out of range
            }
        } else {
             // If port is empty, attempt to set default based on type again, or handle as error/default
            std::string selected_type = type_combo.get_active_text();
            if (selected_type == "SSH") new_connection.port = 22;
            else if (selected_type == "Telnet") new_connection.port = 23;
            else if (selected_type == "VNC") new_connection.port = 5900;
            else if (selected_type == "RDP") new_connection.port = 3389;
            else new_connection.port = 0; // Or some other default/error indicator
        }

        // Get selected folder ID
        Glib::ustring active_folder_id = folder_combo.get_active_id();
        if (!active_folder_id.empty() && active_folder_id != "") { // Check if a folder is selected
             new_connection.folder_id = active_folder_id;
        }

        if (ConnectionManager::save_connection(new_connection)) {
            // Refresh the connections list
            populate_connections_treeview(connections_liststore, connection_columns, *connections_treeview);
        } else {
            // Optionally show an error dialog
            Gtk::MessageDialog error_dialog(dialog, "Save Failed", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
            error_dialog.set_secondary_text("Could not save the connection details.");
            error_dialog.run();
        }
    }
}

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
        std::filesystem::create_directories(config_file.parent_path());

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

// Function to build the menu
void build_menu(Gtk::Window& parent_window, Gtk::MenuBar& menubar, Gtk::Notebook& notebook, Gtk::TreeView& connections_treeview_ref,
                Glib::RefPtr<Gtk::TreeStore>& liststore_ref, ConnectionColumns& columns_ref) { // Changed to ConnectionColumns
    // Create "Options" menu (formerly "File" menu, renamed and repurposed)
    Gtk::Menu* connections_submenu = Gtk::manage(new Gtk::Menu());
    Gtk::MenuItem* connections_menu_item = Gtk::manage(new Gtk::MenuItem("Options", true)); // Renamed, changed from _Connections
    connections_menu_item->set_submenu(*connections_submenu);

    // Items from former Settings menu - now in Connections
    Gtk::MenuItem* add_folder_item = Gtk::manage(new Gtk::MenuItem("Add Folder"));
    Gtk::MenuItem* add_connection_item = Gtk::manage(new Gtk::MenuItem("Add Connection"));
    Gtk::MenuItem* delete_connection_item = Gtk::manage(new Gtk::MenuItem("Delete Connection"));
    Gtk::SeparatorMenuItem* separator1 = Gtk::manage(new Gtk::SeparatorMenuItem());
    Gtk::MenuItem* preferences_item = Gtk::manage(new Gtk::MenuItem("Preferences"));

    connections_submenu->append(*add_folder_item);
    edit_folder_menu_item = Gtk::manage(new Gtk::MenuItem("Edit Folder")); // Changed text for clarity, removed underscore
    edit_folder_menu_item->signal_activate().connect([&parent_window, &connections_treeview_ref, &liststore_ref, &columns_ref]() {
        FolderOps::edit_folder(parent_window, connections_treeview_ref, liststore_ref, columns_ref);
    });
    connections_submenu->append(*edit_folder_menu_item);

    // Declare and append Delete Folder item
    Gtk::MenuItem* delete_folder_item = Gtk::manage(new Gtk::MenuItem("Delete Folder")); // Removed underscore and fixed typo
    connections_submenu->append(*delete_folder_item);

    connections_submenu->append(*add_connection_item);
    edit_connection_menu_item = Gtk::manage(new Gtk::MenuItem("Edit Connection", true)); // Changed from _Connection
    edit_connection_menu_item->signal_activate().connect(sigc::ptr_fun(&on_edit_connection_activate)); // Connect to handler
    edit_connection_menu_item->set_sensitive(false); // Initially disabled
    connections_submenu->append(*edit_connection_menu_item);

    connections_submenu->append(*delete_connection_item);
    connections_submenu->append(*separator1);
    connections_submenu->append(*preferences_item);

    // Separator before Exit
    Gtk::SeparatorMenuItem* separator2 = Gtk::manage(new Gtk::SeparatorMenuItem());
    connections_submenu->append(*separator2);

    // Exit item (from former File menu)
    Gtk::MenuItem* exit_item = Gtk::manage(new Gtk::MenuItem("_Exit", true));
    exit_item->signal_activate().connect([](){
        gtk_main_quit();
    });
    connections_submenu->append(*exit_item);

    // Create Help menu (remains the same)
    Gtk::MenuItem* help_menu_item = Gtk::manage(new Gtk::MenuItem("Help"));
    Gtk::Menu* help_submenu = Gtk::manage(new Gtk::Menu());
    help_menu_item->set_submenu(*help_submenu);
    Gtk::MenuItem* about_item = Gtk::manage(new Gtk::MenuItem("About"));
    help_submenu->append(*about_item);

    // Add menus to menubar (only Connections and Help)
    menubar.append(*connections_menu_item);
    menubar.append(*help_menu_item);

    // Connect menu item handlers (signal_activate calls remain the same,
    // as the Gtk::MenuItem* variables are still valid)
    add_folder_item->signal_activate().connect([&parent_window, &connections_treeview_ref, &liststore_ref, &columns_ref]() {
        FolderOps::add_folder(parent_window, connections_treeview_ref, liststore_ref, columns_ref);
    });

    // Connect delete_folder_item to FolderOps::delete_folder
    delete_folder_item->signal_activate().connect([&parent_window, &connections_treeview_ref, &liststore_ref, &columns_ref]() {
        FolderOps::delete_folder(parent_window, connections_treeview_ref, liststore_ref, columns_ref);
    });

    add_connection_item->signal_activate().connect(sigc::bind(sigc::ptr_fun(&on_add_connection_activated), std::ref(notebook)));
    delete_connection_item->signal_activate().connect([&connections_treeview_ref, &liststore_ref, &columns_ref, &parent_window]() { // Added parent_window capture
        Glib::RefPtr<Gtk::TreeSelection> selection = connections_treeview_ref.get_selection();
        Gtk::TreeModel::iterator iter = selection->get_selected();

        if (!iter) {
            Gtk::MessageDialog warning_dialog(parent_window, "No Selection", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true);
            warning_dialog.set_secondary_text("Please select a connection to delete.");
            warning_dialog.run();
            return;
        }

        bool is_folder = (*iter)[columns_ref.is_folder];
        if (is_folder) {
            Gtk::MessageDialog warning_dialog(parent_window, "Cannot Delete Folder Here", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true);
            warning_dialog.set_secondary_text("This option is for deleting connections. To delete a folder, use the dedicated 'Delete Folder' option.");
            warning_dialog.run();
            return;
        }

        Glib::ustring connection_name = static_cast<Glib::ustring>((*iter)[columns_ref.name]);
        Glib::ustring connection_id = static_cast<Glib::ustring>((*iter)[columns_ref.id]);

        Gtk::MessageDialog confirm_dialog(parent_window, "Confirm Delete", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);
        confirm_dialog.set_secondary_text("Are you sure you want to delete the connection: '" + connection_name + "'?");
        int response = confirm_dialog.run();

        if (response == Gtk::RESPONSE_YES) {
            if (ConnectionManager::delete_connection(connection_id)) {
                populate_connections_treeview(liststore_ref, columns_ref, connections_treeview_ref);
            } else {
                Gtk::MessageDialog error_dialog(parent_window, "Delete Failed", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
                error_dialog.set_secondary_text("Could not delete the connection: '" + connection_name + "'.");
                error_dialog.run();
            }
        }
    });

    preferences_item->signal_activate().connect([&](){
        Gtk::MessageDialog dialog("Preferences", false, Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK, true);
        dialog.set_secondary_text("Preferences dialog not implemented yet.");
        dialog.run();
    });

    // Connect About handler
    about_item->signal_activate().connect([&]() {
        Gtk::AboutDialog about_dialog;
        about_dialog.set_program_name("ngTerm");
        about_dialog.set_version("0.1.0");
        about_dialog.set_comments("NextGen terminal application with connection management");
        about_dialog.set_copyright(" 2025 Jayan Menon");
        about_dialog.set_website("https://github.com/jmenon-mwp/ngTerm");
        about_dialog.set_website_label("Project Homepage");

        about_dialog.run();
    });
}

// Function to build the left frame
void build_leftFrame(Gtk::Window& parent_window, Gtk::Frame& left_frame, Gtk::ScrolledWindow& left_scrolled_window,
                    Gtk::TreeView& connections_treeview_ref, // Renamed to avoid conflict
                    Glib::RefPtr<Gtk::TreeStore>& connections_liststore_ref, // Renamed
                    ConnectionColumns& columns_ref, Gtk::Notebook& notebook) { // Added notebook and changed to ConnectionColumns
    // Assign global pointers
    connections_treeview = &connections_treeview_ref;
    connections_liststore = connections_liststore_ref;
    // connection_columns is already global

    Gtk::Box* vbox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0));
    // Ensure the frame is empty before adding the new vbox
    Gtk::Widget* current_child = left_frame.get_child();
    if (current_child) {
        left_frame.remove(); // Correct: Gtk::Bin::remove() takes no arguments
    }
    left_frame.add(*vbox);

    // Create Toolbar
    Gtk::Toolbar* toolbar = Gtk::manage(new Gtk::Toolbar());
    toolbar->set_toolbar_style(Gtk::TOOLBAR_ICONS);
    // toolbar->set_show_arrow(false); // Deprecated and not needed with modern GTK
    // toolbar->set_spacing(0); // This property doesn't exist for GtkToolbar / causes warning

    // Add Folder Button
    Gtk::ToolButton* add_folder_button = Gtk::manage(new Gtk::ToolButton());
    Gtk::Image* add_folder_icon = Gtk::manage(new Gtk::Image("images/addfolder.png"));
    add_folder_button->set_icon_widget(*add_folder_icon);
    add_folder_button->set_tooltip_text("Add Folder");
    add_folder_button->set_margin_start(1);
    add_folder_button->set_margin_end(1);
    add_folder_button->set_margin_top(0);
    add_folder_button->set_margin_bottom(0);
    add_folder_button->signal_clicked().connect([&parent_window, &connections_treeview_ref, &connections_liststore_ref, &columns_ref]() {
        FolderOps::add_folder(parent_window, connections_treeview_ref, connections_liststore_ref, columns_ref);
    });
    toolbar->append(*add_folder_button);

    // Edit Folder Button
    Gtk::ToolButton* edit_folder_button = Gtk::manage(new Gtk::ToolButton());
    Gtk::Image* edit_folder_icon = Gtk::manage(new Gtk::Image("images/editfolder.png"));
    edit_folder_button->set_icon_widget(*edit_folder_icon);
    edit_folder_button->set_tooltip_text("Edit Folder");
    edit_folder_button->set_margin_start(1);
    edit_folder_button->set_margin_end(1);
    edit_folder_button->set_margin_top(0);
    edit_folder_button->set_margin_bottom(0);
    edit_folder_button->signal_clicked().connect([&parent_window, &connections_treeview_ref, &connections_liststore_ref, &columns_ref]() {
        FolderOps::edit_folder(parent_window, connections_treeview_ref, connections_liststore_ref, columns_ref);
    });
    edit_folder_menu_item_toolbar = edit_folder_button; // Assign to global pointer
    toolbar->append(*edit_folder_button);

    // Delete Folder Button (3rd button)
    Gtk::ToolButton* delete_folder_button = Gtk::manage(new Gtk::ToolButton());
    Gtk::Image* delete_folder_icon = Gtk::manage(new Gtk::Image("images/rmfolder.png"));
    delete_folder_button->set_icon_widget(*delete_folder_icon);
    delete_folder_button->set_tooltip_text("Delete Folder");
    delete_folder_button->set_margin_start(1);
    delete_folder_button->set_margin_end(1);
    delete_folder_button->set_margin_top(0);
    delete_folder_button->set_margin_bottom(0);
    delete_folder_button->signal_clicked().connect([&parent_window, &connections_treeview_ref, &connections_liststore_ref, &columns_ref]() {
        FolderOps::delete_folder(parent_window, connections_treeview_ref, connections_liststore_ref, columns_ref);
    });
    delete_folder_menu_item_toolbar = delete_folder_button; // Assign to global pointer
    toolbar->append(*delete_folder_button);

    // Add Connection Button
    Gtk::ToolButton* add_conn_button = Gtk::manage(new Gtk::ToolButton());
    Gtk::Image* add_conn_icon = Gtk::manage(new Gtk::Image("images/addconn.png"));
    add_conn_button->set_icon_widget(*add_conn_icon);
    add_conn_button->set_tooltip_text("Add Connection");
    add_conn_button->set_margin_start(1);
    add_conn_button->set_margin_end(1);
    add_conn_button->set_margin_top(0);
    add_conn_button->set_margin_bottom(0);
    add_conn_button->signal_clicked().connect(sigc::bind(sigc::ptr_fun(&on_add_connection_activated), std::ref(notebook)));
    toolbar->append(*add_conn_button);

    // Edit Connection Button
    Gtk::ToolButton* edit_conn_button = Gtk::manage(new Gtk::ToolButton());
    Gtk::Image* edit_conn_icon = Gtk::manage(new Gtk::Image("images/editconn.png"));
    edit_conn_button->set_icon_widget(*edit_conn_icon);
    edit_conn_button->set_tooltip_text("Edit Connection");
    edit_conn_button->set_margin_start(1);
    edit_conn_button->set_margin_end(1);
    edit_conn_button->set_margin_top(0);
    edit_conn_button->set_margin_bottom(0);
    edit_conn_button->signal_clicked().connect(sigc::ptr_fun(&on_edit_connection_activate));
    edit_connection_menu_item_toolbar = edit_conn_button; // Assign to global pointer
    toolbar->append(*edit_conn_button);

    // Delete Connection Button (6th button)
    Gtk::ToolButton* delete_conn_button = Gtk::manage(new Gtk::ToolButton());
    Gtk::Image* delete_conn_icon = Gtk::manage(new Gtk::Image("images/rmconn.png"));
    delete_conn_button->set_icon_widget(*delete_conn_icon);
    delete_conn_button->set_tooltip_text("Delete Connection");
    delete_conn_button->set_margin_start(1);
    delete_conn_button->set_margin_end(1);
    delete_conn_button->set_margin_top(0);
    delete_conn_button->set_margin_bottom(0);
    delete_conn_button->signal_clicked().connect([&connections_treeview_ref, &connections_liststore_ref, &columns_ref, &parent_window]() { // Added parent_window capture
        if (!connections_treeview) return; // Ensure global connections_treeview is used or capture local ref
        Glib::RefPtr<Gtk::TreeSelection> selection = connections_treeview_ref.get_selection();
        if (selection) {
            Gtk::TreeModel::iterator iter = selection->get_selected();
            if (iter) {
                bool is_connection = (*iter)[columns_ref.is_connection];
                if (is_connection) {
                    Glib::ustring conn_id = static_cast<Glib::ustring>((*iter)[columns_ref.id]);
                    Glib::ustring conn_name = static_cast<Glib::ustring>((*iter)[columns_ref.name]);

                    Gtk::MessageDialog confirmation_dialog(parent_window, "Delete Connection", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);
                    confirmation_dialog.set_secondary_text("Are you sure you want to delete the connection '" + conn_name + "'?");
                    if (confirmation_dialog.run() == Gtk::RESPONSE_YES) {
                        if (ConnectionManager::delete_connection(conn_id)) {
                            populate_connections_treeview(connections_liststore_ref, columns_ref, connections_treeview_ref);
                        } else {
                            Gtk::MessageDialog error_dialog(parent_window, "Error", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
                            error_dialog.set_secondary_text("Could not delete the connection '" + conn_name + "'.");
                            error_dialog.run();
                        }
                    }
                } else {
                    Gtk::MessageDialog info_dialog(parent_window, "Not a Connection", false, Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK, true);
                    info_dialog.set_secondary_text("Please select a connection to delete.");
                    info_dialog.run();
                }
            } else {
                 Gtk::MessageDialog info_dialog(parent_window, "No Selection", false, Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK, true);
                 info_dialog.set_secondary_text("Please select an item to delete.");
                 info_dialog.run();
            }
        }
    });
    delete_connection_menu_item_toolbar = delete_conn_button; // Assign to global pointer
    toolbar->append(*delete_conn_button);

    vbox->pack_start(*toolbar, Gtk::PACK_SHRINK); // Add toolbar to vbox

    // Configure ScrolledWindow for TreeView
    left_scrolled_window.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

    // Set the TreeView to fill the scrolled window space
    connections_treeview_ref.set_hexpand(true);
    connections_treeview_ref.set_vexpand(true);

    // Create a TreeStore to hold the data for the TreeView
    connections_treeview_ref.set_model(connections_liststore_ref);

    // Add columns to the TreeView
    // Ensure columns are added only once, TreeView might be reused or repopulated
    if (connections_treeview_ref.get_columns().empty()) {
        connections_treeview_ref.append_column("Name", columns_ref.name);
    }

    // Add the TreeView to the ScrolledWindow
    left_scrolled_window.add(connections_treeview_ref);

    vbox->pack_start(left_scrolled_window, Gtk::PACK_EXPAND_WIDGET); // ScrolledWindow (with TreeView) below toolbar, expands
    vbox->show(); // Explicitly show the vertical box

    // Add the VBox (toolbar + scrolled_window) to the Frame
    left_frame.add(*vbox);
    left_frame.show_all(); // Explicitly show the frame and all its new children
}

// Function to build the right frame
void build_rightFrame(Gtk::Notebook& notebook) {
    // Configure the Notebook widget
    notebook.set_tab_pos(Gtk::POS_TOP);
}

// Updated function to populate the connections TreeView with hierarchy
void populate_connections_treeview(Glib::RefPtr<Gtk::TreeStore>& liststore, ConnectionColumns& cols, Gtk::TreeView& treeview) {
    liststore->clear(); // Clear existing items

    std::vector<FolderInfo> folders = ConnectionManager::load_folders();
    std::vector<ConnectionInfo> connections = ConnectionManager::load_connections();

    // Optional Debugging Output (can be uncommented if needed)
    // std::cout << "Debug: Folders loaded: " << folders.size() << std::endl;
    // for(const auto& f : folders) {
    //     std::cout << "  Folder ID: " << f.id << ", Name: " << f.name << ", Parent ID: " << f.parent_id << std::endl;
    // }
    // std::cout << "Debug: Connections loaded: " << connections.size() << std::endl;
    // for(const auto& c : connections) {
    //     std::cout << "  Conn ID: " << c.id << ", Name: " << c.name << ", Folder ID: " << c.folder_id << std::endl;
    // }

    // Map to store parent iterators for folders: maps folder_id to its Gtk::TreeModel::iterator
    std::map<std::string, Gtk::TreeModel::iterator> folder_iters;
    // Map to store children of each parent: maps parent_id to a vector of pointers to FolderInfo
    std::map<std::string, std::vector<FolderInfo*>> folder_children_map;
    // Set of folder IDs that have been added to the tree store to avoid duplicates or cycles
    std::set<std::string> added_folder_ids;

    // First pass: populate folder_children_map to organize folders by their parent_id
    // Use a plain for loop to iterate with index to get pointers to elements in 'folders' vector
    for (size_t i = 0; i < folders.size(); ++i) {
        folder_children_map[folders[i].parent_id].push_back(&folders[i]);
    }

    // Recursive lambda to add folders and their children to the TreeStore
    std::function<void(const std::string&, const Gtk::TreeModel::iterator*)> add_folders_recursive;
    add_folders_recursive =
        [&](const std::string& current_parent_id, const Gtk::TreeModel::iterator* parent_iter_ptr) {
        if (folder_children_map.count(current_parent_id)) {
            for (FolderInfo* folder_ptr : folder_children_map[current_parent_id]) {
                const auto& folder = *folder_ptr;
                if (added_folder_ids.count(folder.id)) {
                    // std::cerr << "Warning: Folder ID '" << folder.id << "' encountered again or cycle detected. Skipping." << std::endl;
                    continue; // Already added or detected cycle
                }

                Gtk::TreeModel::iterator current_iter; // Declare iterator
                Gtk::TreeModel::Row row;
                if (parent_iter_ptr) { // If it's a child folder (has a parent iterator)
                    current_iter = liststore->append((*parent_iter_ptr)->children()); // Assign iterator from append
                } else { // If it's a root folder (no parent iterator)
                    current_iter = liststore->append(); // Assign iterator from append
                }
                row = *current_iter; // Get row proxy from the iterator

                row[cols.name] = folder.name;
                row[cols.id] = folder.id;
                row[cols.parent_id_col] = folder.parent_id; // Store parent_id in its column
                row[cols.is_folder] = true;
                row[cols.is_connection] = false;
                row[cols.host] = ""; // Folders don't have host/port etc.
                row[cols.port] = "";
                row[cols.username] = "";
                row[cols.connection_type] = "";

                folder_iters[folder.id] = current_iter; // Store the obtained iterator
                added_folder_ids.insert(folder.id);

                // Recursively add children of this folder
                add_folders_recursive(folder.id, &current_iter);
            }
        }
    };

    // Add root folders (those with empty parent_id)
    add_folders_recursive("", nullptr);

    // Second pass for any orphaned folders (parent_id exists but parent folder itself was not found/added)
    // This can happen if parent_id points to a non-existent folder or a folder that couldn't be added due to a cycle.
    for (const auto& folder_pair : folder_children_map) {
        const std::string& parent_id_key = folder_pair.first;
        // Only process if parent_id_key is not empty (root items handled by add_folders_recursive("", nullptr))
        // and if this parent_id_key itself was not added as a folder (meaning it's an orphan parent reference)
        if (!parent_id_key.empty() && folder_iters.find(parent_id_key) == folder_iters.end()) {
            for (FolderInfo* folder_ptr : folder_pair.second) {
                const auto& folder = *folder_ptr;
                if (added_folder_ids.find(folder.id) == added_folder_ids.end()) { // If not already added
                    // std::cerr << "Warning: Folder '" << folder.name << "' (ID: " << folder.id
                    //           << ") has parent_id '" << folder.parent_id << "' but actual parent folder was not found. Adding to root." << std::endl;

                    Gtk::TreeModel::iterator current_iter = liststore->append(); // Add orphan to root and get iterator
                    Gtk::TreeModel::Row row = *current_iter; // Get row proxy from iterator
                    row[cols.name] = folder.name;
                    row[cols.id] = folder.id;
                    row[cols.parent_id_col] = ""; // Treat as root, clear its original parent_id for display
                    row[cols.is_folder] = true;
                    row[cols.is_connection] = false;
                    row[cols.host] = ""; row[cols.port] = ""; row[cols.username] = ""; row[cols.connection_type] = "";

                    folder_iters[folder.id] = current_iter; // Store the obtained iterator
                    added_folder_ids.insert(folder.id);
                    // Recursively add children of this now root-displayed orphaned folder
                    add_folders_recursive(folder.id, &current_iter);
                }
            }
        }
    }

    // Add connections under their respective folders or at the root
    for (const auto& connection : connections) {
        Gtk::TreeModel::Row conn_row;
        Gtk::TreeModel::iterator parent_iter;
        bool parent_found = false;

        if (!connection.folder_id.empty() && folder_iters.count(connection.folder_id)) {
            // Folder_id is specified and the corresponding folder iterator exists
            parent_iter = folder_iters[connection.folder_id];
            parent_found = true;
        }

        if (parent_found) {
            conn_row = *(liststore->append(parent_iter->children()));
        } else {
            if (!connection.folder_id.empty()) {
                // Connection specifies a folder_id, but that folder was not found in folder_iters.
                // This means it's an orphaned connection, or its parent folder was an orphan and placed at root.
                // std::cerr << "Warning: Connection '" << connection.name << "' (ID: " << connection.id
                //           << ") has folder_id '" << connection.folder_id << "' but the folder was not found/placed. Adding connection to root." << std::endl;
            }
            // Add to root if folder_id is empty or if the specified folder was not found.
            conn_row = *(liststore->append());
        }

        conn_row[cols.name] = connection.name;
        conn_row[cols.id] = connection.id;
        conn_row[cols.is_folder] = false;
        conn_row[cols.is_connection] = true;
        conn_row[cols.host] = connection.host;
        conn_row[cols.port] = connection.port > 0 ? std::to_string(connection.port) : "";
        conn_row[cols.username] = connection.username;
        conn_row[cols.connection_type] = connection.connection_type;
        conn_row[cols.parent_id_col] = connection.folder_id; // For connections, parent_id_col stores their folder_id
    }
    treeview.expand_all(); // Expand all nodes to show the structure
}

int main(int argc, char* argv[]) {
    // Initialize the GTK+ application
    Gtk::Main kit(argc, argv);

    // --- Instantiate global GTK widgets AFTER Gtk::Main ---
    connections_treeview = new Gtk::TreeView();
    info_frame = new Gtk::Frame();
    info_frame->set_hexpand(true); // Allow frame to expand horizontally

    info_grid = new Gtk::Grid();
    info_grid->set_hexpand(true); // Allow grid to expand horizontally

    host_value_label = new Gtk::Label();
    host_value_label->set_line_wrap(true);
    host_value_label->set_line_wrap_mode(Pango::WRAP_WORD_CHAR);
    host_value_label->set_hexpand(true); // Allow label to expand horizontally
    host_value_label->set_xalign(0.0f); // Align text to the left within the label
    host_value_label->set_valign(Gtk::ALIGN_START); // Align content to the top

    type_value_label = new Gtk::Label();
    type_value_label->set_line_wrap(true);
    type_value_label->set_line_wrap_mode(Pango::WRAP_WORD_CHAR);
    type_value_label->set_hexpand(true); // Allow label to expand horizontally
    type_value_label->set_xalign(0.0f); // Align text to the left within the label
    type_value_label->set_valign(Gtk::ALIGN_START); // Align content to the top

    port_value_label = new Gtk::Label();
    port_value_label->set_line_wrap(true);
    port_value_label->set_line_wrap_mode(Pango::WRAP_WORD_CHAR);
    port_value_label->set_hexpand(true); // Allow label to expand horizontally
    port_value_label->set_xalign(0.0f); // Align text to the left within the label
    port_value_label->set_valign(Gtk::ALIGN_START); // Align content to the top
    // --- End of instantiation ---

    // Manage configuration
    manage_config();

    // Create the main window
    Gtk::Window window;
    window.set_title("ngTerm");

    // Read configuration
    json config = read_config();
    window.set_default_size(config["window_width"], config["window_height"]);

    // Create a vertical box to hold the main content
    Gtk::VBox main_vbox(false, 0);
    window.add(main_vbox);

    // Create MenuBar
    Gtk::MenuBar menubar;

    // Build menu first
    Gtk::Notebook notebook; // notebook can remain local to main if not needed globally
    // connections_treeview, connection_columns, connections_liststore are now global
    // Initialize global liststore using global connection_columns
    connections_liststore = Gtk::TreeStore::create(connection_columns);
    connections_treeview->set_model(connections_liststore); // Use ->

    build_menu(window, menubar, notebook, *connections_treeview, connections_liststore, connection_columns); // Dereference connections_treeview

    // Pack MenuBar at the TOP of the main_vbox
    main_vbox.pack_start(menubar, false, false, 0);

    // Create a horizontal paned widget
    Gtk::HPaned main_hpaned;
    main_vbox.pack_start(main_hpaned, true, true, 0);

    // Left frame for connections TreeView and Info Section
    Gtk::VBox left_pane_vbox(false, 5); // VBox for TreeView and Info Section, with spacing

    // Instantiate the Frame that build_leftFrame will populate
    Gtk::Frame left_frame_for_toolbar_and_treeview;
    // ScrolledWindow for TreeView - will be passed to build_leftFrame
    Gtk::ScrolledWindow connections_scrolled_window;
    // connections_scrolled_window.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC); // build_leftFrame handles this

    // Call build_leftFrame to populate left_frame_for_toolbar_and_treeview
    // It will add connections_scrolled_window (containing the treeview) and the toolbar to this frame.
    build_leftFrame(window, left_frame_for_toolbar_and_treeview, connections_scrolled_window,
                    *connections_treeview, connections_liststore, connection_columns, notebook);

    // Add the populated frame (which now contains the toolbar and treeview) to the left_pane_vbox
    left_pane_vbox.pack_start(left_frame_for_toolbar_and_treeview, Gtk::PACK_EXPAND_WIDGET);

    // Setup the Info Frame
    Gtk::Label* frame_title_label = Gtk::manage(new Gtk::Label());
    frame_title_label->set_markup("<b>Connection Information</b>");
    info_frame->set_label_widget(*frame_title_label); // Use ->
    info_frame->set_shadow_type(Gtk::SHADOW_ETCHED_IN); // Use ->
    info_frame->set_border_width(5); // Use ->

    // Ensure info_frame is empty before adding info_grid
    Gtk::Widget* current_info_child = info_frame->get_child();
    if (current_info_child) {
        info_frame->remove(); // Correct: Gtk::Bin::remove() takes no arguments
    }
    info_frame->add(*info_grid); // Add the global info_grid to the global info_frame
    left_pane_vbox.pack_start(*info_frame, false, true, 0); // expand=false, fill=true horizontally

    // Add labels to the grid - Type, Hostname, Port order
    Gtk::Label type_label; // Local title label
    type_label.set_markup("<b>Type:</b>");
    type_label.set_halign(Gtk::ALIGN_START);
    type_label.set_valign(Gtk::ALIGN_START); // Align content to the top
    info_grid->attach(type_label, 0, 0, 1, 1); // Row 0
    type_value_label->set_halign(Gtk::ALIGN_START); // Use ->
    info_grid->attach(*type_value_label, 1, 0, 1, 1); // Use -> for info_grid, dereference global label, Row 0

    Gtk::Label host_label; // Local title label
    host_label.set_markup("<b>Hostname:</b>");
    host_label.set_halign(Gtk::ALIGN_START);
    host_label.set_valign(Gtk::ALIGN_START); // Align content to the top
    info_grid->attach(host_label, 0, 1, 1, 1); // Row 1
    host_value_label->set_halign(Gtk::ALIGN_START); // Use ->
    info_grid->attach(*host_value_label, 1, 1, 1, 1); // Use -> for info_grid, dereference global label, Row 1

    Gtk::Label port_label; // Local title label
    port_label.set_markup("<b>Port:</b>");
    port_label.set_halign(Gtk::ALIGN_START);
    port_label.set_valign(Gtk::ALIGN_START); // Align content to the top
    info_grid->attach(port_label, 0, 2, 1, 1); // Row 2
    port_value_label->set_halign(Gtk::ALIGN_START); // Use ->
    info_grid->attach(*port_value_label, 1, 2, 1, 1); // Use -> for info_grid, dereference global label, Row 2

    // Pack the left_pane_vbox (TreeView + Info) into the HPaned
    main_hpaned.add1(left_pane_vbox);

    // Right frame for notebook (terminals)
    main_hpaned.add2(notebook); // Add notebook directly

    main_hpaned.set_position(250); // Initial position of the divider

    // Connections TreeView setup
    // connections_treeview->set_model(connections_liststore); // Already done after liststore creation
    // connections_treeview->append_column("Name", connection_columns.name); // build_leftFrame handles adding the column if empty
    connections_treeview->get_selection()->signal_changed().connect(sigc::ptr_fun(&on_connection_selection_changed)); // Use ->

    // Populate the TreeView after setting up selection handler
    populate_connections_treeview(connections_liststore, connection_columns, *connections_treeview);

    // Add double-click event handler
    connections_treeview->signal_row_activated().connect([&notebook](const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn*) { // Use ->
        if (!connections_liststore) return; // Guard
        Gtk::TreeModel::iterator iter = connections_liststore->get_iter(path);
        if (iter) {
            // Access connection_columns directly as it's a global object (not pointer)
            bool is_folder = (*iter)[connection_columns.is_folder];
            if (is_folder) {
                return; // Do nothing if a folder is double-clicked
            }

            bool is_connection = (*iter)[connection_columns.is_connection];
            if (is_connection) {
                Glib::ustring connection_name = static_cast<Glib::ustring>((*iter)[connection_columns.name]);
                Gtk::VBox* terminal_box = Gtk::manage(new Gtk::VBox());
                GtkWidget* vte_widget = vte_terminal_new();
                Gtk::Widget* terminal_widget = Glib::wrap(vte_widget);
                terminal_box->pack_start(*terminal_widget);
                terminal_widget->show();
                Gtk::Label* tab_label = Gtk::manage(new Gtk::Label(connection_name));
                tab_label->show();
                terminal_box->show();
                int page_num = notebook.append_page(*terminal_box, *tab_label);
                if (page_num == -1) {
                    std::cerr << "Error: Failed to append new terminal tab." << std::endl;
                    return;
                }
                struct TerminalData { Gtk::Notebook* notebook; int page_num; };
                TerminalData* data = new TerminalData{&notebook, page_num};
                notebook.set_current_page(data->page_num);
                notebook.show_all();
                static auto focus_terminal = [](gpointer vte_data) -> gboolean {
                    GtkWidget* widget = GTK_WIDGET(vte_data);
                    if (widget && gtk_widget_get_realized(widget) && gtk_widget_get_visible(widget) && gtk_widget_is_sensitive(widget)) {
                        gtk_widget_grab_focus(widget);
                    }
                    return FALSE;
                };
                g_idle_add(focus_terminal, vte_widget);
                vte_terminal_set_input_enabled(VTE_TERMINAL(vte_widget), TRUE);
                auto child_watch_callback = [](GPid pid, gint status, gpointer user_data) {
                    TerminalData* term_data = static_cast<TerminalData*>(user_data);
                    auto idle_callback = [](gpointer inner_data) -> gboolean {
                        TerminalData* t_data = static_cast<TerminalData*>(inner_data);
                        if (t_data->notebook->get_n_pages() > t_data->page_num) {
                            t_data->notebook->remove_page(t_data->page_num);
                        }
                        delete t_data;
                        return FALSE;
                    };
                    g_idle_add(idle_callback, term_data);
                    g_spawn_close_pid(pid);
                };
                const char* argv[] = {"/bin/bash", nullptr};
                GError* error = nullptr;
                GPid child_pid;
                vte_terminal_spawn_sync(VTE_TERMINAL(vte_widget), VTE_PTY_DEFAULT, nullptr, const_cast<char**>(argv), nullptr, G_SPAWN_DEFAULT, nullptr, nullptr, &child_pid, nullptr, &error);
                if (error) {
                    std::cerr << "Error spawning terminal: " << error->message << std::endl;
                    g_error_free(error);
                }
                vte_terminal_watch_child(VTE_TERMINAL(vte_widget), child_pid);
                g_child_watch_add(child_pid, child_watch_callback, data);
            }
        }
    });

    // Add resize event handler to save window dimensions
    window.signal_size_allocate().connect([&window, &config](Gtk::Allocation& allocation) {
        int width, height;
        window.get_size(width, height);

        // Only save if dimensions have changed
        if (config["window_width"] != width || config["window_height"] != height) {
            // Update window dimensions in config
            config["window_width"] = width;
            config["window_height"] = height;

            // Save updated configuration
            save_config(config);
        }
    });

    // Fallback resize event handler
    window.signal_configure_event().connect([&window, &config](GdkEventConfigure* event) {
        int width, height;
        window.get_size(width, height);

        // Only save if dimensions have changed
        if (config["window_width"] != width || config["window_height"] != height) {
            // Update window dimensions in config
            config["window_width"] = width;
            config["window_height"] = height;

            // Save updated configuration
            save_config(config);
        }

        // Allow event to propagate
        return false;
    });

    // Connect the delete event
    window.signal_delete_event().connect([](GdkEventAny*) {
        // Explicitly close all windows
        gtk_main_quit();
        return false;  // Propagate the event
    });

    // Show all widgets
    window.show_all_children();
    window.show();

    // Start the GTK main loop
    Gtk::Main::run(window);

    return 0;
}