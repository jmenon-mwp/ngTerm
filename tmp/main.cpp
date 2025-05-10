#include <gtkmm.h>
#include <vte/vte.h>
#include <iostream>
#include <vector>
#include <string>
#include <glib.h>
#include <atomic>  // Add this header for std::atomic
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include "Connections.h"

using json = nlohmann::json;

// Define a Column Record to hold the data for the TreeView
class ConnectionColumns : public Gtk::TreeModel::ColumnRecord {
public:
    ConnectionColumns() {
        add(name);
        add(id);
        add(is_folder);
    }
    Gtk::TreeModelColumn<Glib::ustring> name;   // Name of folder or connection
    Gtk::TreeModelColumn<Glib::ustring> id;     // Folder or connection ID
    Gtk::TreeModelColumn<bool> is_folder;       // Whether this row is a folder
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
void build_menu(Gtk::MenuBar& menubar, Gtk::Notebook& notebook, Gtk::TreeView& connections_treeview,
                Glib::RefPtr<Gtk::TreeStore>& connections_liststore, ConnectionColumns& columns) {

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
    options_submenu->append(*add_connection_item);
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
    add_connection_item->signal_activate().connect([&notebook, &connections_treeview, &connections_liststore, &columns]() {
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
                populate_connections_treeview(connections_liststore, columns, connections_treeview);
            } else {
                // Optionally show an error dialog
                Gtk::MessageDialog error_dialog(dialog, "Save Failed", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
                error_dialog.set_secondary_text("Could not save the connection details.");
                error_dialog.run();
            }
        }
    });

    delete_connection_item->signal_activate().connect([&connections_treeview, &connections_liststore, &columns]() {
        Gtk::TreeModel::iterator iter = connections_treeview.get_selection()->get_selected();

        if (!iter) {
            Gtk::MessageDialog warning_dialog("No Selection", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true);
            warning_dialog.set_secondary_text("Please select a connection to delete.");
            warning_dialog.run();
            return;
        }

        bool is_folder = (*iter)[columns.is_folder];
        if (is_folder) {
            Gtk::MessageDialog warning_dialog("Cannot Delete Folder", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true);
            warning_dialog.set_secondary_text("This option is for deleting connections only. Please select a connection item.");
            warning_dialog.run();
            return;
        }

        Glib::ustring connection_name = (*iter)[columns.name];
        Glib::ustring connection_id = (*iter)[columns.id];

        Gtk::MessageDialog confirm_dialog("Confirm Delete", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);
        confirm_dialog.set_secondary_text("Are you sure you want to delete the connection: '" + connection_name + "'?");
        int response = confirm_dialog.run();

        if (response == Gtk::RESPONSE_YES) {
            if (ConnectionManager::delete_connection(connection_id.raw())) {
                populate_connections_treeview(connections_liststore, columns, connections_treeview);
            } else {
                Gtk::MessageDialog error_dialog("Delete Failed", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
                error_dialog.set_secondary_text("Could not delete the connection: '" + connection_name + "'.");
                error_dialog.run();
            }
        }
    });

    // Add folder menu item
    add_folder_item->signal_activate().connect([&]() {
        // Create a dialog for new folder
        Gtk::Dialog folder_dialog("New Folder", true);
        folder_dialog.set_default_size(250, 150);

        Gtk::Box* folder_content_area = folder_dialog.get_content_area();
        Gtk::Label folder_name_label("Folder Name:");
        Gtk::Entry folder_name_entry;

        folder_content_area->pack_start(folder_name_label);
        folder_content_area->pack_start(folder_name_entry);

        folder_dialog.add_button("Cancel", Gtk::RESPONSE_CANCEL);
        folder_dialog.add_button("Create", Gtk::RESPONSE_OK);

        folder_content_area->show_all();

        int folder_response = folder_dialog.run();
        if (folder_response == Gtk::RESPONSE_OK) {
            std::string new_folder_name = folder_name_entry.get_text();
            if (!new_folder_name.empty()) {
                // Create new folder
                FolderInfo new_folder{
                    ConnectionManager::generate_folder_id(),
                    new_folder_name
                };

                if (ConnectionManager::save_folder(new_folder)) {
                    // Add to treeview
                    Gtk::TreeModel::Row row = *(connections_liststore->append());
                    row[columns.name] = new_folder_name;
                    row[columns.id] = new_folder.id;
                    row[columns.is_folder] = true;
                }
            }
        }
        folder_dialog.close();
    });

    // Preferences handler (placeholder)
    preferences_item->signal_activate().connect([&]() {
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

// Function to populate the connections TreeView
void populate_connections_treeview(Glib::RefPtr<Gtk::TreeStore>& liststore, ConnectionColumns& cols, Gtk::TreeView& treeview) {
    // Preload saved connections
    std::vector<FolderInfo> folders = ConnectionManager::load_folders();
    std::vector<ConnectionInfo> connections = ConnectionManager::load_connections();

    // Populate the TreeStore with folders and connections
    liststore->clear(); // Clear existing items before populating

    for (const auto& folder : folders) {
        Gtk::TreeModel::Row folder_row = *(liststore->append());
        folder_row[cols.name] = folder.name;
        folder_row[cols.id] = folder.id;
        folder_row[cols.is_folder] = true;

        // Add connections under this folder
        for (const auto& connection : connections) {
            if (connection.folder_id == folder.id) {
                Gtk::TreeModel::Row conn_row = *(liststore->append(folder_row.children()));
                conn_row[cols.name] = connection.name;
                conn_row[cols.id] = connection.id;
                conn_row[cols.is_folder] = false;
            }
        }
    }

    // Add connections without a folder
    for (const auto& connection : connections) {
        if (connection.folder_id.empty()) {
            Gtk::TreeModel::Row conn_row = *(liststore->append());
            conn_row[cols.name] = connection.name;
            conn_row[cols.id] = connection.id;
            conn_row[cols.is_folder] = false;
        }
    }

    // Expand all rows in the TreeView
    treeview.expand_all();
}

int main(int argc, char* argv[]) {
    // Initialize the GTK+ application
    Gtk::Main kit(argc, argv);

    // Manage configuration
    manage_config();

    // Create the main window
    Gtk::Window window;
    window.set_title("ngTerm");

    // Read configuration
    json config = read_config();
    window.set_default_size(config["window_width"], config["window_height"]);

    // Create a vertical box to hold the main content
    Gtk::VBox main_vbox;
    window.add(main_vbox);

    // Create MenuBar
    Gtk::MenuBar menubar;

    // Build menu first
    Gtk::Notebook notebook;
    Gtk::TreeView connections_treeview;
    ConnectionColumns columns;
    Glib::RefPtr<Gtk::TreeStore> connections_liststore = Gtk::TreeStore::create(columns);
    connections_treeview.set_model(connections_liststore);

    build_menu(menubar, notebook, connections_treeview, connections_liststore, columns);

    // Pack MenuBar at the TOP of the main_vbox
    main_vbox.pack_start(menubar, false, false, 0);

    // Create horizontal paned widget
    Gtk::HPaned main_hpaned;
    main_vbox.pack_start(main_hpaned, true, true, 0);

    // Create left frame components
    Gtk::Frame left_frame;
    Gtk::ScrolledWindow left_scrolled_window;

    // Build left frame
    build_leftFrame(left_frame, left_scrolled_window, connections_treeview, connections_liststore, columns);

    // Build right frame
    build_rightFrame(notebook);

    // Add frames to paned widget
    main_hpaned.pack1(left_frame, false, false);
    main_hpaned.pack2(notebook, true, true);

    // Set the initial position of the paned widget
    main_hpaned.set_position(250);

    // Preload saved connections and populate the TreeStore
    populate_connections_treeview(connections_liststore, columns, connections_treeview);

    // Add double-click event handler for connections_treeview
    connections_treeview.signal_row_activated().connect([&notebook, &columns, &connections_liststore](const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn*) {
        try {
            // Get the selected row
            auto iter = connections_liststore->get_iter(path);
            if (iter) {
                // Get the connection name
                Glib::ustring connection_name = (*iter)[columns.name];

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
        } catch (const Glib::Exception& ex) {
            std::cerr << "GTKmm Exception: " << ex.what() << std::endl;
        } catch (const std::exception& ex) {
            std::cerr << "Standard Exception: " << ex.what() << std::endl;
        } catch (...) {
            std::cerr << "Unknown error occurred while creating terminal" << std::endl;
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