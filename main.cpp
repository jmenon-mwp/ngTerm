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

using json = nlohmann::json;

// Define a Column Record to hold the data for the TreeView
class ConnectionColumns : public Gtk::TreeModel::ColumnRecord {
public:
    ConnectionColumns() {
        add(name);
    }
    Gtk::TreeModelColumn<Glib::ustring> name; // Column to hold connection names (as strings)
};

// Function prototypes to allow cross-referencing
void build_menu(Gtk::MenuBar& menubar, Gtk::Notebook& notebook, Gtk::TreeView& connections_treeview,
                Glib::RefPtr<Gtk::ListStore>& connections_liststore, ConnectionColumns& columns);
void build_leftFrame(Gtk::Frame& left_frame, Gtk::ScrolledWindow& left_scrolled_window,
                     Gtk::TreeView& connections_treeview,
                     Glib::RefPtr<Gtk::ListStore>& connections_liststore,
                     ConnectionColumns& columns);
void build_rightFrame(Gtk::Notebook& notebook);

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

        std::cout << "Saved config: width=" << config["window_width"]
                  << ", height=" << config["window_height"] << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error writing config file: " << e.what() << std::endl;
    }
}

// Function to build the menu
void build_menu(Gtk::MenuBar& menubar, Gtk::Notebook& notebook, Gtk::TreeView& connections_treeview,
                Glib::RefPtr<Gtk::ListStore>& connections_liststore, ConnectionColumns& columns) {

    // Create File menu
    Gtk::Menu* file_submenu = Gtk::manage(new Gtk::Menu());
    Gtk::MenuItem* file_menu_item = Gtk::manage(new Gtk::MenuItem("_File", true));
    file_menu_item->set_submenu(*file_submenu);
    Gtk::MenuItem* exit_item = Gtk::manage(new Gtk::MenuItem("_Exit", true));
    exit_item->signal_activate().connect([](){
        gtk_main_quit();
    });
    file_submenu->append(*exit_item);

    // Create Settings menu
    Gtk::MenuItem* settings_menu_item = Gtk::manage(new Gtk::MenuItem("Settings"));
    Gtk::Menu* settings_submenu = Gtk::manage(new Gtk::Menu());
    settings_menu_item->set_submenu(*settings_submenu);
    Gtk::MenuItem* add_connection_item = Gtk::manage(new Gtk::MenuItem("Add Connection"));
    Gtk::MenuItem* preferences_item = Gtk::manage(new Gtk::MenuItem("Preferences"));
    settings_submenu->append(*add_connection_item);
    settings_submenu->append(*preferences_item);

    // Create Help menu
    Gtk::MenuItem* help_menu_item = Gtk::manage(new Gtk::MenuItem("Help"));
    Gtk::Menu* help_submenu = Gtk::manage(new Gtk::Menu());
    help_menu_item->set_submenu(*help_submenu);
    Gtk::MenuItem* about_item = Gtk::manage(new Gtk::MenuItem("About"));
    help_submenu->append(*about_item);

    // Add menus to menubar
    menubar.append(*file_menu_item);
    menubar.append(*settings_menu_item);
    menubar.append(*help_menu_item);

    // Connect menu item handlers
    add_connection_item->signal_activate().connect([&]() {
        // Create an input dialog for connection name
        Gtk::Dialog dialog("Add Connection", true);
        dialog.set_default_size(300, 100);

        // Create a vertical box for the dialog
        Gtk::Box* content_area = dialog.get_content_area();

        // Create a label and entry for connection name
        Gtk::Label name_label("Connection Name:");
        Gtk::Entry name_entry;

        // Pack the label and entry into the dialog
        content_area->pack_start(name_label);
        content_area->pack_start(name_entry);

        // Add standard dialog buttons
        dialog.add_button("Cancel", Gtk::RESPONSE_CANCEL);
        dialog.add_button("Add", Gtk::RESPONSE_OK);

        // Show all widgets in the dialog
        dialog.show_all();

        // Run the dialog and get the response
        int response = dialog.run();

        // Process the response
        if (response == Gtk::RESPONSE_OK) {
            Glib::ustring connection_name = name_entry.get_text();

            // Validate the connection name
            if (!connection_name.empty()) {
                // Add the new connection to the list store
                Gtk::TreeModel::Row row = *(connections_liststore->append());
                row[columns.name] = connection_name;
            }
        }
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
                     Glib::RefPtr<Gtk::ListStore>& connections_liststore,
                     ConnectionColumns& columns) {
    // Configure the left frame
    left_frame.set_label("Connections");
    left_frame.set_shadow_type(Gtk::SHADOW_IN);

    // Configure the scrolled window
    left_scrolled_window.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

    // Set the TreeView to fill the scrolled window space
    connections_treeview.set_hexpand(true);
    connections_treeview.set_vexpand(true);

    // Create a ListStore to hold the data for the TreeView
    connections_liststore = Gtk::ListStore::create(columns);
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
    Glib::RefPtr<Gtk::ListStore> connections_liststore =
        Gtk::ListStore::create(columns);
    connections_treeview.set_model(connections_liststore);

    build_menu(menubar, notebook, connections_treeview, connections_liststore, columns);

    // Pack MenuBar at the TOP of the main_vbox
    main_vbox.pack_start(menubar, false, false, 0);

    // Create horizontal paned widget
    Gtk::HPaned main_hpaned;
    main_vbox.pack_start(main_hpaned, true, true, 0);

    // Create left frame components
    Gtk::Frame left_frame("Connections");
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

                // Add a label to the notebook tab
                Gtk::Label* tab_label = Gtk::manage(new Gtk::Label(connection_name));

                // Capture necessary data for child watch
                struct TerminalData {
                    Gtk::Notebook* notebook;
                    int page_num;
                };

                TerminalData* data = new TerminalData{&notebook, notebook.append_page(*terminal_box, *tab_label)};
                notebook.show_all();

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