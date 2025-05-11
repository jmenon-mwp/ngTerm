#include <gtkmm.h>
#include <vte/vte.h>
#include <iostream>
#include <vector>
#include <string>
#include <glib.h> // For Glib basic types if needed
#include <uuid/uuid.h> // For libuuid
#include <atomic>  // Add this header for std::atomic
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <gtkmm/comboboxtext.h>
// #include <gtkmm/vpaned.h> // This was mistakenly added
#include <gtkmm/frame.h>
#include <gtkmm/grid.h>

#include "Connections.h"

using json = nlohmann::json;

// Define custom columns for the TreeView
struct ConnectionColumns : public Gtk::TreeModel::ColumnRecord {
    ConnectionColumns() {
        add(name);
        add(id);
        add(is_folder);
        add(is_connection);
        add(host);
        add(port);
        add(username);
        add(connection_type);
        add(parent_id_col); // Add parent_id_col here
    }
    Gtk::TreeModelColumn<Glib::ustring> name;   // Name of folder or connection
    Gtk::TreeModelColumn<Glib::ustring> id;     // Folder or connection ID
    Gtk::TreeModelColumn<bool> is_folder;       // True if it's a folder
    Gtk::TreeModelColumn<bool> is_connection;   // True if it's a connection
    Gtk::TreeModelColumn<Glib::ustring> host;   // Hostname or IP address
    Gtk::TreeModelColumn<Glib::ustring> port;   // Port number (stored as string)
    Gtk::TreeModelColumn<Glib::ustring> username; // Username
    Gtk::TreeModelColumn<Glib::ustring> connection_type; // Connection type (e.g., SSH, Telnet)
    Gtk::TreeModelColumn<Glib::ustring> parent_id_col; // Parent ID (for folders and connections)
};

// Function prototypes to allow cross-referencing
void build_menu(Gtk::MenuBar& menubar, Gtk::Notebook& notebook, Gtk::TreeView& connections_treeview,
                Glib::RefPtr<Gtk::TreeStore>& connections_liststore, ConnectionColumns& columns);
void build_leftFrame(Gtk::Frame& left_frame, Gtk::ScrolledWindow& left_scrolled_window,
                    Gtk::TreeView& connections_treeview,
                    Glib::RefPtr<Gtk::TreeStore>& connections_liststore,
                    ConnectionColumns& columns);
void build_rightFrame(Gtk::Notebook& notebook);

// Forward declaration for the new population function
void populate_connections_treeview(Glib::RefPtr<Gtk::TreeStore>& liststore, ConnectionColumns& cols, Gtk::TreeView& treeview);

// Forward declaration for the selection handler
void on_connection_selection_changed();

// Forward declaration for the edit connection handler
void on_edit_connection_activate();

// Global TreeView, Model, and Columns
Gtk::TreeView* connections_treeview = nullptr; // Changed to pointer
ConnectionColumns connection_columns;          // Stays as object
Glib::RefPtr<Gtk::TreeStore> connections_liststore; // Stays as RefPtr

// Global UI elements for Info Panel
Gtk::Frame* info_frame = nullptr;          // Changed to pointer
Gtk::Grid* info_grid = nullptr;               // Changed to pointer
Gtk::Label* host_value_label = nullptr;       // Changed to pointer
Gtk::Label* type_value_label = nullptr;       // Changed to pointer
Gtk::Label* port_value_label = nullptr;       // Changed to pointer

// Global MenuItems for Edit functionality
Gtk::MenuItem* edit_folder_menu_item = nullptr;
Gtk::MenuItem* edit_connection_menu_item = nullptr;

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
}

// Function to handle editing a folder
void on_edit_folder_activate() {
    if (!connections_treeview || !connections_liststore || !edit_folder_menu_item) return;

    Glib::RefPtr<Gtk::TreeSelection> selection = connections_treeview->get_selection();
    Gtk::TreeModel::iterator iter = selection->get_selected();

    if (!iter) {
        Gtk::MessageDialog warning_dialog("No Folder Selected", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true);
        warning_dialog.set_secondary_text("Please select a folder to edit.");
        warning_dialog.run();
        return;
    }

    Gtk::TreeModel::Row row = *iter;
    if (!row[connection_columns.is_folder]) {
        Gtk::MessageDialog warning_dialog("Not a Folder", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true);
        warning_dialog.set_secondary_text("The selected item is not a folder.");
        warning_dialog.run();
        return;
    }

    FolderInfo current_folder;
    current_folder.id = static_cast<Glib::ustring>(row[connection_columns.id]);
    current_folder.name = static_cast<Glib::ustring>(row[connection_columns.name]);
    current_folder.parent_id = static_cast<Glib::ustring>(row[connection_columns.parent_id_col]);

    Gtk::Dialog dialog("Edit Folder", true /* modal */);
    dialog.set_default_size(350, 200); // Adjusted size for new field

    Gtk::Grid* grid = Gtk::manage(new Gtk::Grid());
    grid->set_border_width(10);
    grid->set_column_spacing(10);
    grid->set_row_spacing(10);
    dialog.get_content_area()->pack_start(*grid, Gtk::PACK_EXPAND_WIDGET);

    // Folder Name
    Gtk::Label name_label("Folder Name:");
    name_label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry name_entry;
    name_entry.set_hexpand(true);
    name_entry.set_text(current_folder.name); // Pre-fill current name

    // Parent Folder selection (ComboBoxText)
    Gtk::Label parent_folder_label("Parent Folder:");
    parent_folder_label.set_halign(Gtk::ALIGN_START);
    Gtk::ComboBoxText parent_folder_combo;
    parent_folder_combo.set_hexpand(true);
    std::vector<FolderInfo> all_folders = ConnectionManager::load_folders();
    parent_folder_combo.append("", "None (Root Level)"); // ID, Text for root
    int parent_folder_active_idx = 0;
    int current_combo_idx = 1; // Start after "None"
    for (const auto& folder_item : all_folders) {
        if (folder_item.id == current_folder.id) {
            continue; // Don't allow a folder to be its own parent
        }
        parent_folder_combo.append(folder_item.id, folder_item.name);
        if (folder_item.id == current_folder.parent_id) {
            parent_folder_active_idx = current_combo_idx;
        }
        current_combo_idx++;
    }
    parent_folder_combo.set_active(parent_folder_active_idx);

    // Attach to grid
    grid->attach(name_label, 0, 0, 1, 1);
    grid->attach(name_entry, 1, 0, 1, 1);
    grid->attach(parent_folder_label, 0, 1, 1, 1);
    grid->attach(parent_folder_combo, 1, 1, 1, 1);

    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    Gtk::Button* save_button = dialog.add_button("_Save", Gtk::RESPONSE_OK);
    save_button->get_style_context()->add_class("suggested-action");

    dialog.show_all();
    int response = dialog.run();

    if (response == Gtk::RESPONSE_OK) {
        std::string new_folder_name = name_entry.get_text();
        if (new_folder_name.empty()) {
            Gtk::MessageDialog warning_dialog(dialog, "Folder Name Empty", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true);
            warning_dialog.set_secondary_text("Folder name cannot be empty.");
            warning_dialog.run();
            return;
        }

        FolderInfo updated_folder = current_folder; // Copy ID
        updated_folder.name = new_folder_name;
        updated_folder.parent_id = parent_folder_combo.get_active_id(); // Get new parent ID

        // Basic circular dependency check: a folder cannot become a child of itself.
        // More complex checks (e.g. child of its own descendant) are harder here.
        if (updated_folder.id == updated_folder.parent_id) {
            Gtk::MessageDialog error_dialog(dialog, "Invalid Parent", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
            error_dialog.set_secondary_text("A folder cannot be its own parent.");
            error_dialog.run();
            return;
        }

        if (ConnectionManager::save_folder(updated_folder)) {
            populate_connections_treeview(connections_liststore, connection_columns, *connections_treeview);
        } else {
            Gtk::MessageDialog error_dialog(dialog, "Save Failed", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
            error_dialog.set_secondary_text("Could not update the folder.");
            error_dialog.run();
        }
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

    // Attach to grid (similar to Add Connection dialog)
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
void build_menu(Gtk::MenuBar& menubar, Gtk::Notebook& notebook, Gtk::TreeView& connections_treeview_ref,
                Glib::RefPtr<Gtk::TreeStore>& liststore_ref, ConnectionColumns& columns_ref) {
    // Create Options menu (formerly File menu)
    Gtk::Menu* options_submenu = Gtk::manage(new Gtk::Menu());
    Gtk::MenuItem* options_menu_item = Gtk::manage(new Gtk::MenuItem("_Options", true)); // Renamed
    options_menu_item->set_submenu(*options_submenu);

    // Items from former Settings menu - now in Options
    Gtk::MenuItem* add_folder_item = Gtk::manage(new Gtk::MenuItem("Add Folder"));
    Gtk::MenuItem* add_connection_item = Gtk::manage(new Gtk::MenuItem("Add Connection"));
    Gtk::MenuItem* delete_connection_item = Gtk::manage(new Gtk::MenuItem("Delete Connection"));
    Gtk::SeparatorMenuItem* separator1 = Gtk::manage(new Gtk::SeparatorMenuItem());
    Gtk::MenuItem* preferences_item = Gtk::manage(new Gtk::MenuItem("Preferences"));

    options_submenu->append(*add_folder_item);
    edit_folder_menu_item = Gtk::manage(new Gtk::MenuItem("Edit _Folder...")); // Adjusted mnemonic
    edit_folder_menu_item->signal_activate().connect(sigc::ptr_fun(&on_edit_folder_activate));
    edit_folder_menu_item->set_sensitive(false); // Initially disabled
    options_submenu->append(*edit_folder_menu_item);

    options_submenu->append(*add_connection_item);
    edit_connection_menu_item = Gtk::manage(new Gtk::MenuItem("Edit _Connection...")); // Adjusted mnemonic
    edit_connection_menu_item->signal_activate().connect(sigc::ptr_fun(&on_edit_connection_activate)); // Connect to handler
    edit_connection_menu_item->set_sensitive(false); // Initially disabled
    options_submenu->append(*edit_connection_menu_item);

    options_submenu->append(*delete_connection_item);
    options_submenu->append(*separator1);
    options_submenu->append(*preferences_item);

    // Separator before Exit
    Gtk::SeparatorMenuItem* separator2 = Gtk::manage(new Gtk::SeparatorMenuItem());
    options_submenu->append(*separator2);

    // Exit item (from former File menu)
    Gtk::MenuItem* exit_item = Gtk::manage(new Gtk::MenuItem("_Exit", true));
    exit_item->signal_activate().connect([](){
        gtk_main_quit();
    });
    options_submenu->append(*exit_item);

    // Create Help menu (remains the same)
    Gtk::MenuItem* help_menu_item = Gtk::manage(new Gtk::MenuItem("Help"));
    Gtk::Menu* help_submenu = Gtk::manage(new Gtk::Menu());
    help_menu_item->set_submenu(*help_submenu);
    Gtk::MenuItem* about_item = Gtk::manage(new Gtk::MenuItem("About"));
    help_submenu->append(*about_item);

    // Add menus to menubar (only Options and Help)
    menubar.append(*options_menu_item);
    menubar.append(*help_menu_item);

    // Connect menu item handlers (signal_activate calls remain the same,
    // as the Gtk::MenuItem* variables are still valid)
    add_connection_item->signal_activate().connect([&notebook, &connections_treeview_ref, &liststore_ref, &columns_ref]() {
        Gtk::Dialog dialog("Add New Connection", true /* modal */);
        dialog.set_default_size(400, 350); // Adjusted default size

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
        folder_combo.append("", "None (Root Level)"); // Option for no folder
        for (const auto& folder : folders) {
            folder_combo.append(folder.id, folder.name);
        }
        folder_combo.set_active(0); // Default to "None"

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
                if (selected_type == "SSH") {
                    port_entry.set_text("22");
                } else if (selected_type == "Telnet") {
                    port_entry.set_text("23");
                } else if (selected_type == "VNC") {
                    port_entry.set_text("5900");
                } else if (selected_type == "RDP") {
                    port_entry.set_text("3389");
                }
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
                populate_connections_treeview(liststore_ref, columns_ref, connections_treeview_ref);
            } else {
                // Optionally show an error dialog
                Gtk::MessageDialog error_dialog(dialog, "Save Failed", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
                error_dialog.set_secondary_text("Could not save the connection details.");
                error_dialog.run();
            }
        }
    });

    delete_connection_item->signal_activate().connect([&connections_treeview_ref, &liststore_ref, &columns_ref]() {
        Gtk::TreeModel::iterator iter = connections_treeview_ref.get_selection()->get_selected();

        if (!iter) {
            Gtk::MessageDialog warning_dialog("No Selection", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true);
            warning_dialog.set_secondary_text("Please select a connection to delete.");
            warning_dialog.run();
            return;
        }

        bool is_folder = (*iter)[columns_ref.is_folder];
        if (is_folder) {
            Gtk::MessageDialog warning_dialog("Cannot Delete Folder", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true);
            warning_dialog.set_secondary_text("This option is for deleting connections only. Please select a connection item.");
            warning_dialog.run();
            return;
        }

        Glib::ustring connection_name = (*iter)[columns_ref.name];
        Glib::ustring connection_id = (*iter)[columns_ref.id];

        Gtk::MessageDialog confirm_dialog("Confirm Delete", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);
        confirm_dialog.set_secondary_text("Are you sure you want to delete the connection: '" + connection_name + "'?");
        int response = confirm_dialog.run();

        if (response == Gtk::RESPONSE_YES) {
            if (ConnectionManager::delete_connection(connection_id.raw())) {
                populate_connections_treeview(liststore_ref, columns_ref, connections_treeview_ref);
            } else {
                Gtk::MessageDialog error_dialog("Delete Failed", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
                error_dialog.set_secondary_text("Could not delete the connection: '" + connection_name + "'.");
                error_dialog.run();
            }
        }
    });

    // Add folder menu item
    add_folder_item->signal_activate().connect([&liststore_ref, &columns_ref, &connections_treeview_ref]() {
        Gtk::Dialog dialog("Add New Folder", true /* modal */);
        dialog.set_default_size(350, 200); // Adjusted size

        Gtk::Grid* grid = Gtk::manage(new Gtk::Grid());
        grid->set_border_width(10);
        grid->set_column_spacing(10);
        grid->set_row_spacing(10);
        dialog.get_content_area()->pack_start(*grid, Gtk::PACK_EXPAND_WIDGET);

        // Folder Name
        Gtk::Label name_label("Folder Name:");
        name_label.set_halign(Gtk::ALIGN_START);
        Gtk::Entry name_entry;
        name_entry.set_hexpand(true);

        // Parent Folder
        Gtk::Label parent_label("Parent Folder:");
        parent_label.set_halign(Gtk::ALIGN_START);
        Gtk::ComboBoxText parent_combo;
        parent_combo.set_hexpand(true);
        parent_combo.append("", "(Root Level)"); // ID, Text. Empty ID for root.

        // Populate parent_combo with existing folders
        std::vector<FolderInfo> existing_folders = ConnectionManager::load_folders();
        for (const auto& folder : existing_folders) {
            parent_combo.append(folder.id, folder.name);
        }
        parent_combo.set_active(0); // Default to (Root Level)

        // Attach to grid
        grid->attach(name_label,   0, 0, 1, 1);
        grid->attach(name_entry,   1, 0, 1, 1);
        grid->attach(parent_label, 0, 1, 1, 1);
        grid->attach(parent_combo, 1, 1, 1, 1);

        dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
        dialog.add_button("_Save", Gtk::RESPONSE_OK);
        dialog.show_all();

        int response = dialog.run();
        if (response == Gtk::RESPONSE_OK) {
            std::string folder_name = name_entry.get_text();
            std::string parent_id = parent_combo.get_active_id();

            if (folder_name.empty()) {
                Gtk::MessageDialog warning_dialog("Folder Name Empty", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true);
                warning_dialog.set_secondary_text("Folder name cannot be empty.");
                warning_dialog.run();
                return;
            }

            FolderInfo new_folder;
            uuid_t uuid; // libuuid type
            uuid_generate_random(uuid); // Generate a random UUID
            char uuid_str[37]; // Buffer for string representation (36 chars + null)
            uuid_unparse_lower(uuid, uuid_str); // Convert to string
            new_folder.id = uuid_str; // Assign to FolderInfo

            new_folder.name = folder_name;
            new_folder.parent_id = parent_id;

            if (ConnectionManager::save_folder(new_folder)) {
                populate_connections_treeview(liststore_ref, columns_ref, connections_treeview_ref);
            } else {
                Gtk::MessageDialog error_dialog("Save Failed", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
                error_dialog.set_secondary_text("Could not save the new folder.");
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
void build_leftFrame(Gtk::Frame& left_frame, Gtk::ScrolledWindow& left_scrolled_window,
                    Gtk::TreeView& connections_treeview,
                    Glib::RefPtr<Gtk::TreeStore>& connections_liststore,
                    ConnectionColumns& columns) {
    left_frame.set_shadow_type(Gtk::SHADOW_IN);

    // Configure the scrolled window
    left_scrolled_window.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

    // Set the TreeView to fill the scrolled window space
    connections_treeview.set_hexpand(true);
    connections_treeview.set_vexpand(true);

    // Create a TreeStore to hold the data for the TreeView
    connections_treeview.set_model(connections_liststore);

    // Add columns to the TreeView
    connections_treeview.append_column("Name", columns.name);

    // Add the TreeView to the ScrolledWindow
    left_scrolled_window.add(connections_treeview);

    // Add the ScrolledWindow to the Frame
    left_frame.add(left_scrolled_window);
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

    build_menu(menubar, notebook, *connections_treeview, connections_liststore, connection_columns); // Dereference connections_treeview

    // Pack MenuBar at the TOP of the main_vbox
    main_vbox.pack_start(menubar, false, false, 0);

    // Create a horizontal paned widget
    Gtk::HPaned main_hpaned;
    main_vbox.pack_start(main_hpaned, true, true, 0);

    // Left frame for connections TreeView and Info Section
    Gtk::VBox left_pane_vbox(false, 5); // VBox for TreeView and Info Section, with spacing

    // ScrolledWindow for TreeView
    Gtk::ScrolledWindow connections_scrolled_window;
    connections_scrolled_window.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    connections_scrolled_window.add(*connections_treeview); // Dereference
    left_pane_vbox.pack_start(connections_scrolled_window, Gtk::PACK_EXPAND_WIDGET);

    // Setup the Info Frame
    Gtk::Label* frame_title_label = Gtk::manage(new Gtk::Label());
    frame_title_label->set_markup("<b>Connection Information</b>");
    info_frame->set_label_widget(*frame_title_label); // Use ->
    info_frame->set_shadow_type(Gtk::SHADOW_ETCHED_IN); // Use ->
    info_frame->set_border_width(5); // Use ->
    left_pane_vbox.pack_start(*info_frame, false, true, 0); // expand=false, fill=true horizontally

    // Setup the Grid inside the Info Frame
    info_grid->set_border_width(5); // Use ->
    info_grid->set_row_spacing(5); // Use ->
    info_grid->set_column_spacing(10); // Use ->
    info_frame->add(*info_grid); // Dereference

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
    connections_treeview->append_column("Name", connection_columns.name); // Use ->
    // Add other columns if they were previously defined and needed for display, otherwise keep it simple.
    // connections_treeview.append_column("ID", connection_columns.id); // Example, usually hidden

    // Set selection mode and connect signal
    connections_treeview->get_selection()->set_mode(Gtk::SELECTION_BROWSE); // Use ->
    connections_treeview->get_selection()->signal_changed().connect(sigc::ptr_fun(&on_connection_selection_changed)); // Use ->

    // Populate the TreeStore
    populate_connections_treeview(connections_liststore, connection_columns, *connections_treeview); // Dereference

    // Add double-click event handler
    connections_treeview->signal_row_activated().connect([&notebook](const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn*) { // Use ->
        if (!connections_liststore) return; // Guard
        Gtk::TreeModel::iterator iter = connections_liststore->get_iter(path);
        if (iter) {
            // Access connection_columns directly as it's a global object (not pointer)
            bool is_folder = (*iter)[connection_columns.is_folder];
            if (is_folder) {
                // If it's a folder, do nothing on double-click for now
                return;
            }

            // If it's a connection, proceed to open a new terminal tab
            bool is_connection = (*iter)[connection_columns.is_connection];
            if (is_connection) {
                // Get the connection name
                Glib::ustring connection_name = (*iter)[connection_columns.name];

                // Create a new terminal tab
                Gtk::VBox* terminal_box = Gtk::manage(new Gtk::VBox());

                // Create VTE terminal widget
                GtkWidget* vte_widget = vte_terminal_new();
                Gtk::Widget* terminal_widget = Glib::wrap(vte_widget);
                terminal_box->pack_start(*terminal_widget);
                terminal_widget->show(); // Explicitly show the VTE widget

                // Create a label to the notebook tab
                Gtk::Label* tab_label = Gtk::manage(new Gtk::Label(connection_name));
                tab_label->show(); // Explicitly show the tab label

                terminal_box->show(); // Explicitly show the page content (VBox)

                // Append the new page and get its number
                int page_num = notebook.append_page(*terminal_box, *tab_label);

                // Check if appending the page was successful
                if (page_num == -1) {
                    std::cerr << "Error: Failed to append new terminal tab." << std::endl;
                    return; // Exit the lambda if page append failed
                }

                // Capture necessary data for child watch
                struct TerminalData {
                    Gtk::Notebook* notebook;
                    int page_num;
                };

                TerminalData* data = new TerminalData{&notebook, page_num};

                notebook.set_current_page(data->page_num); // Set the new tab as current
                notebook.show_all(); // Ensure tab visibility before VTE focus

                // Static function to set focus
                static auto focus_terminal = [](gpointer data) -> gboolean {
                    GtkWidget* widget = GTK_WIDGET(data);
                    // Check if widget is valid, realized, visible, and sensitive before grabbing focus
                    if (widget && gtk_widget_get_realized(widget) && gtk_widget_get_visible(widget) && gtk_widget_is_sensitive(widget)) {
                        gtk_widget_grab_focus(widget);
                    }
                    return FALSE; // run only once
                };

                // Pass the VTE widget to the idle callback
                g_idle_add(focus_terminal, vte_widget);

                vte_terminal_set_input_enabled(VTE_TERMINAL(vte_widget), TRUE);

                // Static callback function
                auto child_watch_callback = [](GPid pid, gint status, gpointer user_data) {
                    TerminalData* data = static_cast<TerminalData*>(user_data);

                    // Use g_idle_add to safely remove the page from the main thread
                    auto idle_callback = [](gpointer user_data) -> gboolean {
                        TerminalData* data = static_cast<TerminalData*>(user_data);

                        // Only remove page if it still exists
                        if (data->notebook->get_n_pages() > data->page_num) {
                            data->notebook->remove_page(data->page_num);
                        }

                        delete data;
                        return FALSE; // run only once
                    };

                    g_idle_add(idle_callback, data);
                    g_spawn_close_pid(pid);
                };

                // Spawn a shell in the terminal
                const char* argv[] = {"/bin/bash", nullptr};
                GError* error = nullptr;
                GPid child_pid;
                vte_terminal_spawn_sync(
                    VTE_TERMINAL(vte_widget),
                    VTE_PTY_DEFAULT,
                    nullptr,  // working directory
                    const_cast<char**>(argv),
                    nullptr,  // environment
                    G_SPAWN_DEFAULT,
                    nullptr,  // child setup function
                    nullptr,  // child setup data
                    &child_pid,  // child pid
                    nullptr,  // cancellable
                    &error
                );

                // Check for spawn errors
                if (error) {
                    std::cerr << "Error spawning terminal: " << error->message << std::endl;
                    g_error_free(error);
                }

                // Watch the child process using VTE
                vte_terminal_watch_child(VTE_TERMINAL(vte_widget), child_pid);

                // Add child process exit handler
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