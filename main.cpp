#include "main.h"
#include "icondata.h"
#include "Rdp.h"

// Global variables (definition)
Gtk::TreeView* connections_treeview = nullptr;
Glib::RefPtr<Gtk::TreeStore> connections_liststore;
ConnectionColumns connection_columns;
Gtk::HPaned* main_hpaned = nullptr;
Gtk::Frame* left_frame_top = nullptr;

// Global UI elements for Info Panel
Gtk::Frame* info_frame = nullptr;
Gtk::Grid* info_grid = nullptr;
Gtk::Label* host_value_label = nullptr;
Gtk::Label* type_value_label = nullptr;
Gtk::Label* port_value_label = nullptr;

// Global MenuItems for Edit functionality
Gtk::MenuItem* edit_folder_menu_item = nullptr;
Gtk::MenuItem* edit_connection_menu_item = nullptr;
Gtk::MenuItem* duplicate_connection_item = nullptr;

// Global ToolButtons for Edit/Delete functionality on the toolbar
Gtk::ToolButton* edit_folder_menu_item_toolbar = nullptr;
Gtk::ToolButton* edit_connection_menu_item_toolbar = nullptr;
Gtk::ToolButton* duplicate_connection_menu_item_toolbar = nullptr;
Gtk::ToolButton* delete_folder_menu_item_toolbar = nullptr;
Gtk::ToolButton* delete_connection_menu_item_toolbar = nullptr;
Gtk::ToolButton* add_folder_menu_item_toolbar = nullptr;
Gtk::ToolButton* add_connection_menu_item_toolbar = nullptr;

// Track open connections and their tab indices
std::map<std::string, int> open_connections; // Maps connection ID to tab index

// Define the TerminalData struct
struct TerminalData {
    Gtk::Notebook* notebook;
    int page_num;
};

// Helper function to create a Pixbuf from embedded PNG data
Glib::RefPtr<Gdk::Pixbuf> create_pixbuf_from_data(const unsigned char* data, unsigned int len) {
    Glib::RefPtr<Gdk::PixbufLoader> loader = Gdk::PixbufLoader::create("png");
    loader->write(data, len);
    loader->close();
    return loader->get_pixbuf();
}

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
                int port = row[connection_columns.port];
                port_value_label->set_text(port > 0 ? std::to_string(port) : "");
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

// Helper function to update UI based on connection type
void update_connection_type_ui(
    Gtk::ComboBoxText& type_combo,
    Gtk::Label& auth_method_label,
    Gtk::ComboBoxText& auth_method_combo,
    Gtk::Label& ssh_key_path_label,
    Gtk::Entry& ssh_key_path_entry,
    Gtk::Button& ssh_key_browse_button,
    Gtk::Label& ssh_key_passphrase_label,
    Gtk::Entry& ssh_key_passphrase_entry,
    Gtk::Label& ssh_flags_label,
    Gtk::Entry& ssh_flags_entry,
    Gtk::Label& password_label,
    Gtk::Entry& password_entry,
    Gtk::Label& domain_label,
    Gtk::Entry& domain_entry,
    bool new_connection_flag
)
{
    const std::string conn_type = type_combo.get_active_text();
    const bool is_ssh = (conn_type == "SSH");
    const bool is_rdp = (conn_type == "RDP");
    auth_method_label.set_visible(is_ssh);
    auth_method_combo.set_visible(is_ssh);

    if (new_connection_flag) {
        ssh_flags_label.set_visible(false);
        ssh_flags_entry.set_visible(false);
        ssh_key_path_label.set_visible(false);
        ssh_key_path_entry.set_visible(false);
        ssh_key_browse_button.set_visible(false);
        ssh_key_passphrase_label.set_visible(false);
        ssh_key_passphrase_entry.set_visible(false);
        password_label.set_visible(false);
        password_entry.set_visible(false);
        domain_label.set_visible(false);
        domain_entry.set_visible(false);
    }
    ssh_flags_label.set_visible(is_ssh);
    ssh_flags_entry.set_visible(is_ssh);
    ssh_key_path_label.set_visible(is_ssh && auth_method_combo.get_active_text() == "SSHKey");
    ssh_key_path_entry.set_visible(is_ssh && auth_method_combo.get_active_text() == "SSHKey");
    ssh_key_browse_button.set_visible(is_ssh && auth_method_combo.get_active_text() == "SSHKey");
    ssh_key_passphrase_label.set_visible(is_ssh && auth_method_combo.get_active_text() == "SSHKey");
    ssh_key_passphrase_entry.set_visible(is_ssh && auth_method_combo.get_active_text() == "SSHKey");
    password_label.set_visible(is_rdp || (is_ssh && auth_method_combo.get_active_text() == "Password"));
    password_entry.set_visible(is_rdp || (is_ssh && auth_method_combo.get_active_text() == "Password"));
    domain_label.set_visible(is_rdp);
    domain_entry.set_visible(is_rdp);
}

// Function to handle adding, editing, or duplicating a connection
void process_connection_dialog(Gtk::Notebook& notebook, DialogPurpose purpose, const ConnectionInfo* existing_connection = nullptr) {

    std::string dialog_title;
    std::string confirm_button_text;
    bool new_connection_flag;

    switch (purpose) {
        case DialogPurpose::ADD:
            dialog_title = "Add New Connection";
            confirm_button_text = "Add";
            new_connection_flag = true;
            break;
        case DialogPurpose::EDIT:
            dialog_title = "Edit Connection";
            confirm_button_text = "Save";
            new_connection_flag = false;
            break;
        case DialogPurpose::DUPLICATE:
            dialog_title = "Duplicate Connection";
            confirm_button_text = "Duplicate";
            new_connection_flag = false;
            break;
    }

    Gtk::Dialog dialog(dialog_title, true);
    dialog.set_default_size(450, -1); // -1 means size will be determined by content
    dialog.set_resizable(false); // Prevent manual resizing but still allows automatic resizing

    Gtk::Grid* grid = Gtk::manage(new Gtk::Grid());
    grid->set_hexpand(true);
    grid->set_border_width(10);
    grid->set_column_spacing(10);
    grid->set_row_spacing(10);
    dialog.get_content_area()->pack_start(*grid, Gtk::PACK_EXPAND_WIDGET);

    std::string initially_selected_folder_id = "";
    if (existing_connection) {
        initially_selected_folder_id = existing_connection->folder_id;
    } else if (connections_treeview) {
        Glib::RefPtr<Gtk::TreeSelection> selection = connections_treeview->get_selection();
        if (selection) {
            Gtk::TreeModel::iterator iter = selection->get_selected();
            if (iter) {
                Gtk::TreeModel::Row row = *iter;
                if (row[connection_columns.is_folder]) {
                    initially_selected_folder_id = static_cast<Glib::ustring>(row[connection_columns.id]);
                }
            }
        }
    }

    Gtk::Label folder_label("Folder:", Gtk::ALIGN_START);
    Gtk::ComboBoxText folder_combo;
    folder_combo.set_hexpand(true);
    std::vector<FolderInfo> folders = ConnectionManager::load_folders();
    folder_combo.append("", "None (Root Level)");
    for (const auto& folder : folders) {
        folder_combo.append(folder.id, folder.name);
    }
    if (!initially_selected_folder_id.empty()) {
        folder_combo.set_active_id(initially_selected_folder_id);
    } else {
        folder_combo.set_active(0);
    }

    Gtk::Label type_label("Connection Type:", Gtk::ALIGN_START);
    Gtk::ComboBoxText type_combo;
    type_combo.set_hexpand(true);
    type_combo.append("SSH", "SSH");
    type_combo.append("Telnet", "Telnet");
    type_combo.append("VNC", "VNC");
    type_combo.append("RDP", "RDP");
    type_combo.set_active_text(existing_connection ? existing_connection->connection_type : "SSH");

    Gtk::Label name_label("Connection Name:", Gtk::ALIGN_START);
    Gtk::Label host_label("Host:", Gtk::ALIGN_START);
    Gtk::Label port_label("Port:", Gtk::ALIGN_START);
    Gtk::Label ssh_flags_label("SSH Flags:", Gtk::ALIGN_START);
    Gtk::Label user_label("Username:", Gtk::ALIGN_START);
    Gtk::Label domain_label("Domain:", Gtk::ALIGN_START);

    Gtk::Entry name_entry;
    name_entry.set_hexpand(true);
    if (existing_connection) {
        if (purpose == DialogPurpose::DUPLICATE) {
            name_entry.set_text(existing_connection->name + " (Copy)");
        } else {
            name_entry.set_text(existing_connection->name);
        }
    }

    Gtk::Entry host_entry;
    host_entry.set_hexpand(true);
    if (existing_connection) {
        host_entry.set_text(existing_connection->host);
    }

    Gtk::Entry port_entry;
    port_entry.set_hexpand(true);
    if (existing_connection && existing_connection->port > 0) {
        port_entry.set_text(std::to_string(existing_connection->port));
    }

    Gtk::Entry ssh_flags_entry;
    ssh_flags_entry.set_hexpand(true);
    if (existing_connection) {
        ssh_flags_entry.set_text(existing_connection->additional_ssh_options);
    }

    Gtk::Entry user_entry;
    user_entry.set_hexpand(true);
    if (existing_connection) {
        user_entry.set_text(existing_connection->username);
    }

    Gtk::Entry domain_entry;
    domain_entry.set_hexpand(true);
    if (existing_connection) {
        domain_entry.set_text(existing_connection->domain);
    }

    // SSH Specific fields will be added and visibility toggled
    Gtk::Label auth_method_label("Auth Method:", Gtk::ALIGN_START);
    Gtk::ComboBoxText auth_method_combo;
    auth_method_combo.append("Password", "Password");
    auth_method_combo.append("SSHKey", "SSHKey");
    auth_method_combo.set_hexpand(true);
    if (existing_connection) {
        auth_method_combo.set_active_text(existing_connection->auth_method);
    } else {
        auth_method_combo.set_active_text("Password"); // Default for new
    }

    Gtk::Label password_label("Password:", Gtk::ALIGN_START);
    password_label.set_markup("<span foreground='red'>Password:</span>");
    Gtk::Entry password_entry;
    password_entry.set_visibility(false);

    // Set up password security warning tooltip (single instance)
    static const char* password_security_warning_text =
        "WARNING: Password will be stored in plain text and may be visible in the process list. "
        "This is inherently insecure. Consider using SSH keys or do not save the password here. "
        "You will be prompted for the password when connecting.";
    password_entry.set_tooltip_text(password_security_warning_text);

    // Custom popup tooltip window for password (static to maintain state between function calls)
    static Gtk::Window* password_tooltip_window = nullptr;
    // Show tooltip when password field gets focus
    auto on_focus_in = [&password_entry](GdkEventFocus* event) -> bool {
        if (!password_tooltip_window) {
            password_tooltip_window = new Gtk::Window(Gtk::WINDOW_POPUP);
            password_tooltip_window->set_name("tooltip");
            password_tooltip_window->set_border_width(2);

            // Create an event box to handle the background color
            Gtk::EventBox* event_box = Gtk::manage(new Gtk::EventBox());
            event_box->override_background_color(Gdk::RGBA("orange"));

            Gtk::Label* tooltip_label = Gtk::manage(new Gtk::Label());
            tooltip_label->set_text(password_security_warning_text);
            tooltip_label->set_line_wrap(true);
            tooltip_label->set_max_width_chars(40);
            tooltip_label->set_justify(Gtk::JUSTIFY_LEFT);
            tooltip_label->set_margin_start(8);
            tooltip_label->set_margin_end(8);
            tooltip_label->set_margin_top(4);
            tooltip_label->set_margin_bottom(4);

            event_box->add(*tooltip_label);
            password_tooltip_window->add(*event_box);
            password_tooltip_window->set_default_size(350, 60);
            password_tooltip_window->set_resizable(false);
        }

        int x, y;
        password_entry.get_window()->get_origin(x, y);
        int entry_height = password_entry.get_allocation().get_height();
        // Position tooltip 2 pixels below the password field
        password_tooltip_window->move(x, y + entry_height + 2);
        password_tooltip_window->show_all();
        return false;
    };

    // Hide tooltip when password field loses focus
    auto on_focus_out = [](GdkEventFocus* event) -> bool {
        if (password_tooltip_window) {
            password_tooltip_window->hide();
        }
        return false;
    };

    // Connect the signal handlers
    password_entry.signal_focus_in_event().connect(on_focus_in);
    password_entry.signal_focus_out_event().connect(on_focus_out);

    // Set password field based on purpose
    if (purpose == DialogPurpose::DUPLICATE) {
        password_entry.set_text(""); // Clear password for duplicate
    } else if (existing_connection && purpose == DialogPurpose::EDIT) {
        password_entry.set_text(existing_connection->password);
    } else { // ADD or if existing_connection is null for some reason
        password_entry.set_text("");
    }

    // SSH Key Path fields
    Gtk::Label ssh_key_path_label("SSH Key Path:", Gtk::ALIGN_START);
    Gtk::Entry ssh_key_path_entry;
    ssh_key_path_entry.set_hexpand(true);
    Gtk::Button ssh_key_browse_button("Browse...");
    if (existing_connection) {
        ssh_key_path_entry.set_text(existing_connection->ssh_key_path);
    }

    // SSH Key Passphrase fields
    Gtk::Label ssh_key_passphrase_label("Key Passphrase:", Gtk::ALIGN_START);
    Gtk::Entry ssh_key_passphrase_entry;
    ssh_key_passphrase_entry.set_visibility(false); // Mask passphrase
    ssh_key_passphrase_entry.set_hexpand(true);
    ssh_key_passphrase_entry.set_tooltip_text("Enter passphrase for SSH key (if protected).");
    if (existing_connection) {
        ssh_key_passphrase_entry.set_text(existing_connection->ssh_key_passphrase);
    } else {
        ssh_key_passphrase_entry.set_text("");
    }

    // Configure the grid layout
    grid->set_margin_start(12);
    grid->set_margin_end(12);
    grid->set_margin_top(12);
    grid->set_margin_bottom(12);
    grid->set_row_spacing(6);
    grid->set_column_spacing(12);
    grid->set_hexpand(true);
    grid->set_vexpand(true);

    int row = 0;

    // Add widgets to the grid
    grid->attach(folder_label, 0, row, 1, 1);
    grid->attach(folder_combo, 1, row, 2, 1);  // Removed * as folder_combo is not a pointer
    row++;
    grid->attach(type_label, 0, row, 1, 1);
    grid->attach(type_combo, 1, row, 2, 1);
    row++;
    grid->attach(name_label, 0, row, 1, 1);
    grid->attach(name_entry, 1, row, 2, 1);
    row++;
    grid->attach(host_label, 0, row, 1, 1);
    grid->attach(host_entry, 1, row, 2, 1);
    row++;
    grid->attach(port_label, 0, row, 1, 1);
    grid->attach(port_entry, 1, row, 2, 1);
    row++;
    grid->attach(user_label, 0, row, 1, 1);
    grid->attach(user_entry, 1, row, 2, 1);
    row++;
    grid->attach(domain_label, 0, row, 1, 1);
    grid->attach(domain_entry, 1, row, 2, 1);
    row++;
    grid->attach(auth_method_label, 0, row, 1, 1);
    grid->attach(auth_method_combo, 1, row, 2, 1);
    row++;
    Gtk::Box* key_path_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 5));
    key_path_box->pack_start(ssh_key_path_entry, Gtk::PACK_EXPAND_WIDGET);
    key_path_box->pack_start(ssh_key_browse_button, Gtk::PACK_SHRINK);
    grid->attach(ssh_key_path_label, 0, row, 1, 1);
    grid->attach(*key_path_box, 1, row, 2, 1);
    row++;
    grid->attach(ssh_key_passphrase_label, 0, row, 1, 1);
    grid->attach(ssh_key_passphrase_entry, 1, row, 2, 1);
    row++;
    grid->attach(password_label, 0, row, 1, 1);
    grid->attach(password_entry, 1, row, 2, 1);
    row++;
    grid->attach(ssh_flags_label, 0, row, 1, 1);
    grid->attach(ssh_flags_entry, 1, row, 2, 1);
    row++;

    // Connect signals
    type_combo.signal_changed().connect([&]() {
        update_connection_type_ui(
            type_combo,
            auth_method_label,
            auth_method_combo,
            ssh_key_path_label,
            ssh_key_path_entry,
            ssh_key_browse_button,
            ssh_key_passphrase_label,
            ssh_key_passphrase_entry,
            ssh_flags_label,
            ssh_flags_entry,
            password_label,
            password_entry,
            domain_label,
            domain_entry,
            new_connection_flag
        );
    });

    auth_method_combo.signal_changed().connect([&]() {
        update_connection_type_ui(
            type_combo,
            auth_method_label,
            auth_method_combo,
            ssh_key_path_label,
            ssh_key_path_entry,
            ssh_key_browse_button,
            ssh_key_passphrase_label,
            ssh_key_passphrase_entry,
            ssh_flags_label,
            ssh_flags_entry,
            password_label,
            password_entry,
            domain_label,
            domain_entry,
            new_connection_flag
        );
    });

    dialog.add_button("Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("OK", Gtk::RESPONSE_OK);
    dialog.show_all();
    // Set initial UI state
    update_connection_type_ui(
        type_combo,
        auth_method_label,
        auth_method_combo,
        ssh_key_path_label,
        ssh_key_path_entry,
        ssh_key_browse_button,
        ssh_key_passphrase_label,
        ssh_key_passphrase_entry,
        ssh_flags_label,
        ssh_flags_entry,
        password_label,
        password_entry,
        domain_label,
        domain_entry,
        new_connection_flag
    );

    // Show the dialog and get response
    int response = dialog.run();
    if (response == Gtk::RESPONSE_OK) {
        // Create a new connection info object
        ConnectionInfo new_connection;
        if (existing_connection) {
            new_connection = *existing_connection;
        } else {
            new_connection.id = ConnectionManager::generate_connection_id();
        }

        // Set up the connection info
        new_connection.name = name_entry.get_text();
        new_connection.host = host_entry.get_text();
        try {
            new_connection.port = std::stoi(port_entry.get_text());
        } catch (...) {
            new_connection.port = 0; // Default port if invalid
        }
        new_connection.username = user_entry.get_text();
        new_connection.connection_type = type_combo.get_active_text();
        new_connection.folder_id = folder_combo.get_active_id();

        // Set auth method and credentials based on connection type
        if (new_connection.connection_type == "SSH") {
            new_connection.auth_method = auth_method_combo.get_active_text();
            new_connection.additional_ssh_options = ssh_flags_entry.get_text();

            if (new_connection.auth_method == "Password") {
                new_connection.password = password_entry.get_text();
                new_connection.ssh_key_path = "";
                new_connection.ssh_key_passphrase = "";
            } else if (new_connection.auth_method == "SSHKey") {
                new_connection.password = "";
                new_connection.ssh_key_path = ssh_key_path_entry.get_text();
                new_connection.ssh_key_passphrase = ssh_key_passphrase_entry.get_text();
            }
            new_connection.domain = ""; // Clear domain for non-RDP
        } else if (new_connection.connection_type == "RDP") {
            // For RDP, save password and domain, clear SSH specific fields
            new_connection.password = password_entry.get_text();
            new_connection.domain = domain_entry.get_text();
            new_connection.additional_ssh_options = "";
            new_connection.auth_method = "";
            new_connection.ssh_key_path = "";
            new_connection.ssh_key_passphrase = "";
        } else {
            // Clear all SSH specific fields for other connection types
            new_connection.additional_ssh_options = "";
            new_connection.auth_method = "";
            new_connection.password = "";
            new_connection.ssh_key_path = "";
            new_connection.ssh_key_passphrase = "";
            new_connection.domain = "";
        }

        if (new_connection.name.empty() || new_connection.host.empty() || new_connection.connection_type.empty()) {
            Gtk::MessageDialog error_dialog(dialog, "Error", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
            error_dialog.set_secondary_text("Connection Name, Host, and Type cannot be empty.");
            error_dialog.run();
            return;
        }

        bool success = false;
        if (purpose == DialogPurpose::EDIT) {
            success = ConnectionManager::save_connection(new_connection); // Use save_connection
        } else { // ADD or DUPLICATE
            success = ConnectionManager::save_connection(new_connection); // Use save_connection
        }

        if (success) {
            // Refresh the TreeView to show the new/updated connection
            if (connections_liststore && connections_treeview) {
                populate_connections_treeview(connections_liststore, connection_columns, *connections_treeview);
            }
        } else {
            Gtk::MessageDialog error_dialog(dialog, "Save Failed", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
            error_dialog.set_secondary_text("Failed to save the connection to storage.");
            error_dialog.run();
        }
    }

}

// Function to handle duplicating an existing connection
void duplicate_connection_dialog(Gtk::Notebook& notebook) {
    if (!connections_treeview || !connections_liststore) return;

    Glib::RefPtr<Gtk::TreeSelection> selection = connections_treeview->get_selection();
    if (selection) {
        Gtk::TreeModel::iterator iter = selection->get_selected();
        if (iter) {
            Gtk::TreeModel::Row row = *iter;
            std::string connection_id = static_cast<Glib::ustring>(row[connection_columns.id]);
            bool is_folder = row[connection_columns.is_folder];

            if (is_folder) {
                Gtk::Window* top_level_window = dynamic_cast<Gtk::Window*>(notebook.get_toplevel());
                if (top_level_window) {
                    Gtk::MessageDialog warning_dialog(*top_level_window, "Cannot Duplicate Folder", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true);
                    warning_dialog.set_secondary_text("Please select an individual connection to duplicate, not a folder.");
                    warning_dialog.run();
                }
                return;
            }

            // Fetch the full ConnectionInfo object
            ConnectionInfo existing_connection = ConnectionManager::get_connection_by_id(connection_id);
            if (!existing_connection.id.empty()) { // Check if connection was found
                 process_connection_dialog(notebook, DialogPurpose::DUPLICATE, &existing_connection);
            } else {
                Gtk::Window* top_level_window = dynamic_cast<Gtk::Window*>(notebook.get_toplevel());
                if (top_level_window) {
                    Gtk::MessageDialog error_dialog(*top_level_window, "Error", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
                    error_dialog.set_secondary_text("Could not find the selected connection to duplicate.");
                    error_dialog.run();
                }
            }
        } else {
            Gtk::Window* top_level_window = dynamic_cast<Gtk::Window*>(notebook.get_toplevel());
            if (top_level_window) {
                Gtk::MessageDialog info_dialog(*top_level_window, "No Selection", false, Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK, true);
                info_dialog.set_secondary_text("Please select a connection to duplicate.");
                info_dialog.run();
            }
        }
    }
}

// Function to handle editing an existing connection
void edit_connection_dialog(Gtk::Notebook& notebook) {
    if (!connections_treeview || !connections_liststore) return;

    Glib::RefPtr<Gtk::TreeSelection> selection = connections_treeview->get_selection();
    if (selection) {
        Gtk::TreeModel::iterator iter = selection->get_selected();
        if (iter) {
            Gtk::TreeModel::Row row = *iter;
            std::string connection_id = static_cast<Glib::ustring>(row[connection_columns.id]);
            bool is_folder = row[connection_columns.is_folder];

            if (is_folder) {
                Gtk::Window* top_level_window = dynamic_cast<Gtk::Window*>(notebook.get_toplevel());
                if (top_level_window) {
                    Gtk::MessageDialog warning_dialog(*top_level_window, "Cannot Edit Folder", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true);
                    warning_dialog.set_secondary_text("Please select an individual connection to edit, not a folder.");
                    warning_dialog.run();
                }
                return;
            }

            // Fetch the full ConnectionInfo object
            ConnectionInfo existing_connection = ConnectionManager::get_connection_by_id(connection_id);
            if (!existing_connection.id.empty()) { // Check if connection was found
                 process_connection_dialog(notebook, DialogPurpose::EDIT, &existing_connection);
            } else {
                Gtk::Window* top_level_window = dynamic_cast<Gtk::Window*>(notebook.get_toplevel());
                if (top_level_window) {
                    Gtk::MessageDialog error_dialog(*top_level_window, "Error", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
                    error_dialog.set_secondary_text("Could not find the selected connection to edit.");
                    error_dialog.run();
                }
            }
        } else {
            Gtk::Window* top_level_window = dynamic_cast<Gtk::Window*>(notebook.get_toplevel());
            if (top_level_window) {
                Gtk::MessageDialog info_dialog(*top_level_window, "No Selection", false, Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK, true);
                info_dialog.set_secondary_text("Please select a connection to edit.");
                info_dialog.run();
            }
        }
    }
}

// Function to handle adding a new connection
void add_connection_dialog(Gtk::Notebook& notebook) {
    process_connection_dialog(notebook, DialogPurpose::ADD);
}

// Function to launch an RDP session
void launch_rdp_session(Gtk::Notebook& notebook, const std::string& server, const std::string& username, const std::string& password, const std::string& domain) {
    // Get the notebook's allocation for dimensions
    auto allocation = notebook.get_allocation();
    int width = allocation.get_width();
    int height = allocation.get_height();

    std::cout << "Launching RDP session with dimensions: " << width << "x" << height << std::endl;

    // Ensure minimum dimensions
    if (width < 800) width = 1024;
    if (height < 600) height = 768;

    // Create a new tab for the RDP session
    Gtk::Box* rdp_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));

    // Create the RDP session with domain if provided
    Gtk::Socket* rdp_socket = nullptr;
    std::string effective_username = username;

    if (!domain.empty()) {
        // Format username as DOMAIN\username for RDP
        effective_username = domain + "\\" + username;
    }

    std::cout << "Creating RDP session for " << effective_username << "@" << server << std::endl;
    if (password.empty()) {
        std::cerr << "WARNING: No password provided for RDP connection" << std::endl;
    } else {
        std::cout << "Password provided (length: " << password.length() << ")" << std::endl;
    }

    rdp_socket = Rdp::create_rdp_session(*rdp_box, server, effective_username, password, width, height);

    if (rdp_socket) {
        // Add the socket to the box
        rdp_box->pack_start(*rdp_socket, Gtk::PACK_EXPAND_WIDGET);

        // Add to notebook and get the page number
        int page_num = notebook.append_page(*rdp_box, "RDP: " + server);
        notebook.set_current_page(page_num);

        // Connect to the RDP process exit signal using the singleton
        Rdp::instance()->signal_process_exit().connect([&notebook, rdp_box, page_num]() {
            // Use idle to ensure we're in the main thread
            Glib::signal_idle().connect_once([&notebook, rdp_box, page_num]() {
                // Find the page number again in case tabs were reordered
                int current_page = notebook.page_num(*rdp_box);
                if (current_page != -1) {
                    notebook.remove_page(current_page);
                }
            });
        });

        // Show all widgets
        rdp_box->show_all();
    }
}

// Function to handle deleting a connection
void delete_connection_dialog(Gtk::Notebook& notebook, const Glib::ustring& conn_id, const Glib::ustring& conn_name) {
    Gtk::Window* parent_window = dynamic_cast<Gtk::Window*>(notebook.get_toplevel());
    if (!parent_window) {
        std::cerr << "Could not get parent window" << std::endl;
        return;
    }

    Gtk::MessageDialog confirmation_dialog(*parent_window, "Delete Connection", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);
    confirmation_dialog.set_secondary_text("Are you sure you want to delete the connection '" + conn_name + "'?" +
        "\n\nThis action cannot be undone.");

    if (confirmation_dialog.run() == Gtk::RESPONSE_YES) {
        if (ConnectionManager::delete_connection(conn_id)) {
            if (connections_liststore && connections_treeview) {
                populate_connections_treeview(connections_liststore, connection_columns, *connections_treeview);
            }
        } else {
            Gtk::MessageDialog error_dialog(*parent_window, "Error", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
            error_dialog.set_secondary_text("Could not delete the connection '" + conn_name + "'." +
                "\n\nPlease check the error log for more details.");
            error_dialog.run();
        }
    }
}

// Function to build the menu
void build_menu(Gtk::Window& parent_window, Gtk::MenuBar& menubar, Gtk::Notebook& notebook, Gtk::TreeView& connections_treeview_ref,
                Glib::RefPtr<Gtk::TreeStore>& liststore_ref, ConnectionColumns& columns_ref) { // Changed to ConnectionColumns

    Gtk::Menu* options_submenu = Gtk::manage(new Gtk::Menu());
    Gtk::Menu* help_submenu = Gtk::manage(new Gtk::Menu());
    Gtk::MenuItem* options_menu_item = Gtk::manage(new Gtk::MenuItem("Options", true)); // Renamed, changed from _Connections
    Gtk::MenuItem* add_folder_item = Gtk::manage(new Gtk::MenuItem("Add Folder"));
    Gtk::MenuItem* edit_folder_menu_item = Gtk::manage(new Gtk::MenuItem("Edit Folder"));
    Gtk::MenuItem* delete_folder_item = Gtk::manage(new Gtk::MenuItem("Delete Folder"));
    Gtk::MenuItem* add_connection_item = Gtk::manage(new Gtk::MenuItem("Add Connection"));
    Gtk::MenuItem* edit_connection_menu_item = Gtk::manage(new Gtk::MenuItem("Edit Connection", true));
    Gtk::MenuItem* duplicate_connection_item = Gtk::manage(new Gtk::MenuItem("Duplicate Connection"));
    Gtk::MenuItem* delete_connection_item = Gtk::manage(new Gtk::MenuItem("Delete Connection"));
    Gtk::MenuItem* preferences_item = Gtk::manage(new Gtk::MenuItem("Preferences"));
    Gtk::MenuItem* exit_item = Gtk::manage(new Gtk::MenuItem("_Exit", true));
    Gtk::MenuItem* help_menu_item = Gtk::manage(new Gtk::MenuItem("Help"));
    Gtk::MenuItem* about_item = Gtk::manage(new Gtk::MenuItem("About"));
    Gtk::SeparatorMenuItem* separator1 = Gtk::manage(new Gtk::SeparatorMenuItem());
    Gtk::SeparatorMenuItem* separator2 = Gtk::manage(new Gtk::SeparatorMenuItem());

    options_menu_item->set_submenu(*options_submenu);
    options_submenu->append(*add_folder_item);
    edit_folder_menu_item->signal_activate().connect([&parent_window, &connections_treeview_ref, &liststore_ref, &columns_ref]() {
        FolderOps::edit_folder(parent_window, connections_treeview_ref, liststore_ref, columns_ref);
    });
    options_submenu->append(*edit_folder_menu_item);
    options_submenu->append(*delete_folder_item);
    options_submenu->append(*add_connection_item);
    edit_connection_menu_item->signal_activate().connect([&notebook]() {
        edit_connection_dialog(notebook);
    });
    edit_connection_menu_item->set_sensitive(false); // Initially disabled
    options_submenu->append(*edit_connection_menu_item);
    duplicate_connection_item->signal_activate().connect(sigc::bind(sigc::ptr_fun(&duplicate_connection_dialog), std::ref(notebook)));
    options_submenu->append(*duplicate_connection_item);
    options_submenu->append(*delete_connection_item);
    options_submenu->append(*separator1);
    options_submenu->append(*preferences_item);
    options_submenu->append(*separator2);
    exit_item->signal_activate().connect([](){
        gtk_main_quit();
    });
    options_submenu->append(*exit_item);

    help_menu_item->set_submenu(*help_submenu);
    help_submenu->append(*about_item);

    menubar.append(*options_menu_item);
    menubar.append(*help_menu_item);

    add_folder_item->signal_activate().connect([&parent_window, &connections_treeview_ref, &liststore_ref, &columns_ref]() {
        FolderOps::add_folder(parent_window, connections_treeview_ref, liststore_ref, columns_ref);
    });

    delete_folder_item->signal_activate().connect([&parent_window, &connections_treeview_ref, &liststore_ref, &columns_ref]() {
        FolderOps::delete_folder(parent_window, connections_treeview_ref, liststore_ref, columns_ref);
    });

    add_connection_item->signal_activate().connect(sigc::bind(sigc::ptr_fun(&add_connection_dialog), std::ref(notebook)));
    delete_connection_item->signal_activate().connect(
        [&notebook, &columns_ref, &liststore_ref]() {
            if (!connections_treeview) return;
            Glib::RefPtr<Gtk::TreeSelection> selection = connections_treeview->get_selection();
            if (selection) {
                Gtk::TreeModel::iterator iter = selection->get_selected();
                if (iter) {
                    bool is_connection = (*iter)[columns_ref.is_connection];
                    if (is_connection) {
                        Glib::ustring conn_id = static_cast<Glib::ustring>((*iter)[columns_ref.id]);
                        Glib::ustring conn_name = static_cast<Glib::ustring>((*iter)[columns_ref.name]);
                        delete_connection_dialog(notebook, conn_id, conn_name);
                    }
                }
            }
        }
    );

    preferences_item->signal_activate().connect([&parent_window]() {
        Config::show_preferences_dialog(parent_window);
    });

    // Connect About handler
    about_item->signal_activate().connect([&]() {
        Gtk::AboutDialog about_dialog;
        about_dialog.set_program_name("ngTerm");
        about_dialog.set_version("0.1.0");
        about_dialog.set_comments("NextGen terminal application with connection management");
        about_dialog.set_copyright(" 2025, Jayan Menon");
        about_dialog.set_website("https://github.com/jmenon-mwp/ngTerm");
        about_dialog.set_website_label("Project Homepage");
        about_dialog.run();
    });
}

// Function to build the left frame
void build_leftFrame(Gtk::Window& parent_window, Gtk::Frame& left_frame, Gtk::ScrolledWindow& left_scrolled_window,
    Gtk::TreeView& connections_treeview_ref,
    Glib::RefPtr<Gtk::TreeStore>& liststore_ref,
    ConnectionColumns& columns_ref, Gtk::Notebook& notebook) {
    // Assign global pointers
    connections_treeview = &connections_treeview_ref;
    connections_liststore = liststore_ref;

    // Set frame properties
    left_frame.set_border_width(5);

    // Create a vertical box for the frame's contents
    Gtk::Box* vbox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
    vbox->set_hexpand(true);
    vbox->set_vexpand(true);

    // Remove any existing child from the frame
    Gtk::Widget* current_child = left_frame.get_child();
    if (current_child) {
        left_frame.remove();
    }

    // Configure the scrolled window
    left_scrolled_window.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    left_scrolled_window.set_hexpand(true);
    left_scrolled_window.set_vexpand(true);
    left_scrolled_window.set_min_content_width(250);

    // Configure the treeview
    connections_treeview_ref.set_model(liststore_ref);
    connections_treeview_ref.set_hexpand(true);
    connections_treeview_ref.set_vexpand(true);
    connections_treeview_ref.set_halign(Gtk::ALIGN_FILL);
    connections_treeview_ref.set_valign(Gtk::ALIGN_FILL);
    connections_treeview_ref.set_headers_visible(false);

    // Add the treeview to the scrolled window if not already added
    if (!connections_treeview_ref.get_parent()) {
        left_scrolled_window.add(connections_treeview_ref);
    }

    // Add the scrolled window to the vbox
    vbox->pack_start(left_scrolled_window, true, true, 0);

    // Add the vbox to the frame
    left_frame.add(*vbox);

    // Set up the treeview columns if not already done
    if (connections_treeview_ref.get_columns().empty()) {
        connections_treeview_ref.append_column("Connections", columns_ref.name);
    }

    // Show all widgets
    left_frame.show_all_children();
    vbox->show();
    left_frame.show_all();
}

void build_rightFrame(Gtk::Notebook& notebook) {
    // Configure the Notebook widget
    notebook.set_tab_pos(Gtk::POS_TOP);
}

// Updated function to populate the connections TreeView with hierarchy
void populate_connections_treeview(Glib::RefPtr<Gtk::TreeStore>& liststore, ConnectionColumns& cols, Gtk::TreeView& treeview) {
    liststore->clear(); // Clear existing items

    std::vector<FolderInfo> folders = ConnectionManager::load_folders();
    std::vector<ConnectionInfo> connections = ConnectionManager::load_connections();

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
                row[cols.port] = 0; // Use 0 for empty port
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
                    row[cols.host] = ""; row[cols.port] = 0; row[cols.username] = ""; row[cols.connection_type] = "";

                    folder_iters[folder.id] = current_iter; // Store the obtained iterator
                    added_folder_ids.insert(folder.id);
                    // Recursively add children of this now root-displayed orphaned folder
                    add_folders_recursive(folder.id, &current_iter);
                }
            }
        }
    }

    // Add connections under their respective folders or at the root
    for (const auto& conn : connections) {
        Gtk::TreeModel::iterator conn_iter; // Iterator for the new connection row
        Gtk::TreeModel::Row row;

        // Check if the connection has a folder_id and if that parent folder exists in our map
        if (!conn.folder_id.empty() && folder_iters.count(conn.folder_id)) {
            Gtk::TreeModel::iterator parent_iter = folder_iters[conn.folder_id];
            conn_iter = liststore->append(parent_iter->children()); // Add as child
        } else {
            // No valid folder_id or parent folder not found, add to root
            conn_iter = liststore->append();
        }
        row = *conn_iter; // Get row proxy from the iterator

        row[cols.name] = conn.name;
        row[cols.id] = conn.id;
        row[cols.parent_id_col] = conn.folder_id; // Store folder_id in parent_id_col
        row[cols.is_folder] = false;       // Mark as not a folder
        row[cols.is_connection] = true; // Mark as a connection
        row[cols.host] = conn.host;
        row[cols.port] = conn.port;
        row[cols.username] = conn.username;
        row[cols.connection_type] = conn.connection_type; // Use connection_type from ConnectionInfo
        // Other fields like password, ssh_key_path are not displayed in the treeview columns themselves
    }
    // Ensure the treeview is updated and columns are visible
    treeview.expand_all(); // Optional: expand all folders by default

}

// C-style callback for key press event
gboolean on_terminal_key_press(GtkWidget* widget, GdkEventKey* event, gpointer user_data) {
    TerminalData* td = static_cast<TerminalData*>(user_data);

    // Schedule cleanup for the next idle moment
    g_idle_add([](gpointer data) -> gboolean {
        TerminalData* td = static_cast<TerminalData*>(data);
        if (td->notebook && td->notebook->get_n_pages() > td->page_num) {
            Gtk::Widget* page_widget = td->notebook->get_nth_page(td->page_num);
            if (page_widget) {
                // Remove the connection from our tracking map
                for (auto it = open_connections.begin(); it != open_connections.end(); ) {
                    if (it->second == td->page_num) {
                        it = open_connections.erase(it);
                    } else {
                        // Update indices for tabs that come after this one
                        if (it->second > td->page_num) {
                            it->second--;
                        }
                        ++it;
                    }
                }
                td->notebook->remove_page(td->page_num);
            }
        }
        delete td;
        return G_SOURCE_REMOVE;
    }, td);

    return TRUE; // Stop event propagation
}

// Callback function for VTE's "child-exited" signal
void on_terminal_child_exited(GtkWidget* widget, gint status, gpointer user_data) {
    TerminalData* term_data = static_cast<TerminalData*>(user_data);
    if (!term_data) return;

    // Display a message in the terminal
    VteTerminal* terminal = VTE_TERMINAL(widget);
    const char* exit_message = "\r\n\033[1;31mProcess completed.\033[0m Press any key to close this terminal...\r\n";
    vte_terminal_feed(terminal, exit_message, strlen(exit_message));

    // Connect a key press event to close the terminal when any key is pressed
    g_signal_connect(widget, "key-press-event", G_CALLBACK(on_terminal_key_press), term_data);

    // Ensure focus is on the terminal so it can receive key events
    gtk_widget_grab_focus(widget);
}

// Create a toolbar button with embedded icon
Gtk::ToolButton* create_toolbar_button(const char* label, const unsigned char* icon_data, unsigned int icon_len) {
    auto pixbuf = create_pixbuf_from_data(icon_data, icon_len);
    if (!pixbuf) {
        std::cerr << "Failed to create pixbuf from embedded icon data" << std::endl;
        return nullptr;
    }

    auto button = Gtk::manage(new Gtk::ToolButton(label));
    auto image = Gtk::manage(new Gtk::Image(pixbuf));
    button->set_icon_widget(*image);  // Pass the widget reference
    return button;
}

void save_frame_width(Gtk::Window& window) {
    if (main_hpaned) {
        int frame_width = main_hpaned->get_position();
        json new_config = Config::get();
        new_config["left_frame_width"] = ((frame_width -15) < 250) ? 250 : (frame_width - 14);
        Config::update(new_config);
    }
}

int main(int argc, char* argv[]) {
    // Initialize the GTK+ application
    Gtk::Main kit(argc, argv);

    // Initialize configuration
    Config::init();

    // Create the main window
    Gtk::Window window;
    window.set_title("ngTerm");

    // Set initial window size from config
    const auto& config = Config::get();
    window.set_default_size(
        config.value("window_width", 1024),
        config.value("window_height", 768)
    );

    // If we have saved coordinates, use them
    if (config.contains("window_x") && config.contains("window_y")) {
        window.move(config["window_x"], config["window_y"]);
    }

    // --- Instantiate global GTK widgets AFTER Gtk::Main ---
    connections_treeview = new Gtk::TreeView();
    info_frame = new Gtk::Frame();
    info_frame->set_hexpand(true);

    info_grid = new Gtk::Grid();
    info_grid->set_hexpand(true);

    host_value_label = new Gtk::Label("", Gtk::ALIGN_START);
    host_value_label->set_line_wrap(true);
    host_value_label->set_line_wrap_mode(Pango::WRAP_WORD_CHAR);
    host_value_label->set_hexpand(true);
    host_value_label->set_xalign(0.0f);

    type_value_label = new Gtk::Label("", Gtk::ALIGN_START);
    type_value_label->set_line_wrap(true);
    type_value_label->set_line_wrap_mode(Pango::WRAP_WORD_CHAR);
    type_value_label->set_hexpand(true);
    type_value_label->set_xalign(0.0f);

    port_value_label = new Gtk::Label("", Gtk::ALIGN_START);
    port_value_label->set_line_wrap(true);
    port_value_label->set_line_wrap_mode(Pango::WRAP_WORD_CHAR);
    port_value_label->set_hexpand(true);
    port_value_label->set_xalign(0.0f);
    // --- End of instantiation ---

    // Create a vertical box to hold the main content
    Gtk::VBox main_vbox(false, 0);
    window.add(main_vbox);

    // Create MenuBar
    Gtk::MenuBar menubar;

    // Create notebook
    Gtk::Notebook notebook;

    // Create TreeView references
    Glib::RefPtr<Gtk::TreeStore> connections_liststore;
    ConnectionColumns columns_ref;

    // Initialize global liststore using global connection_columns
    connections_liststore = Gtk::TreeStore::create(connection_columns);
    connections_treeview->set_model(connections_liststore);

    // Build menu first
    build_menu(window, menubar, notebook, *connections_treeview, connections_liststore, connection_columns);

    // Pack MenuBar at the TOP of the main_vbox
    main_vbox.pack_start(menubar, false, false, 0);

    // Create Toolbar
    Gtk::Toolbar* toolbar = Gtk::manage(new Gtk::Toolbar());
    toolbar->set_toolbar_style(Gtk::TOOLBAR_ICONS);  // Show only icons
    toolbar->set_icon_size(Gtk::ICON_SIZE_SMALL_TOOLBAR);

    // Add Folder Button
    add_folder_menu_item_toolbar = create_toolbar_button("Add Folder", addfolder_png, addfolder_png_len);
    add_folder_menu_item_toolbar->set_tooltip_text("Add Folder");
    add_folder_menu_item_toolbar->set_margin_start(1);
    add_folder_menu_item_toolbar->set_margin_end(1);
    add_folder_menu_item_toolbar->set_margin_top(0);
    add_folder_menu_item_toolbar->set_margin_bottom(0);
    add_folder_menu_item_toolbar->signal_clicked().connect(
        [window_ref = std::ref(window), &columns_ref, &connections_liststore]() {
            FolderOps::add_folder(window_ref.get(), *connections_treeview, connections_liststore, columns_ref);
        }
    );
    toolbar->append(*add_folder_menu_item_toolbar);

    // Edit Folder Button
    edit_folder_menu_item_toolbar = create_toolbar_button("Edit Folder", editfolder_png, editfolder_png_len);
    edit_folder_menu_item_toolbar->set_tooltip_text("Edit Folder");
    edit_folder_menu_item_toolbar->set_margin_start(1);
    edit_folder_menu_item_toolbar->set_margin_end(1);
    edit_folder_menu_item_toolbar->set_margin_top(0);
    edit_folder_menu_item_toolbar->set_margin_bottom(0);
    edit_folder_menu_item_toolbar->signal_clicked().connect(
        [window_ref = std::ref(window), &columns_ref, &connections_liststore]() {
            FolderOps::edit_folder(window_ref.get(), *connections_treeview, connections_liststore, columns_ref);
        }
    );
    toolbar->append(*edit_folder_menu_item_toolbar);

    // Delete Folder Button
    delete_folder_menu_item_toolbar = create_toolbar_button("Delete Folder", rmfolder_png, rmfolder_png_len);
    delete_folder_menu_item_toolbar->set_tooltip_text("Delete Folder");
    delete_folder_menu_item_toolbar->set_margin_start(1);
    delete_folder_menu_item_toolbar->set_margin_end(1);
    delete_folder_menu_item_toolbar->set_margin_top(0);
    delete_folder_menu_item_toolbar->set_margin_bottom(0);
    delete_folder_menu_item_toolbar->signal_clicked().connect(
        [window_ref = std::ref(window), &columns_ref, &connections_liststore]() {
            FolderOps::delete_folder(window_ref.get(), *connections_treeview, connections_liststore, columns_ref);
        }
    );
    toolbar->append(*delete_folder_menu_item_toolbar);

    // Add Connection Button
    add_connection_menu_item_toolbar = create_toolbar_button("Add Connection", addconn_png, addconn_png_len);
    add_connection_menu_item_toolbar->set_tooltip_text("Add Connection");
    add_connection_menu_item_toolbar->set_margin_start(1);
    add_connection_menu_item_toolbar->set_margin_end(1);
    add_connection_menu_item_toolbar->set_margin_top(0);
    add_connection_menu_item_toolbar->set_margin_bottom(0);
    add_connection_menu_item_toolbar->signal_clicked().connect(sigc::bind(sigc::ptr_fun(&add_connection_dialog), std::ref(notebook)));
    toolbar->append(*add_connection_menu_item_toolbar);

    // Edit Connection Button
    edit_connection_menu_item_toolbar = create_toolbar_button("Edit Connection", editconn_png, editconn_png_len);
    edit_connection_menu_item_toolbar->set_tooltip_text("Edit Connection");
    edit_connection_menu_item_toolbar->set_margin_start(1);
    edit_connection_menu_item_toolbar->set_margin_end(1);
    edit_connection_menu_item_toolbar->set_margin_top(0);
    edit_connection_menu_item_toolbar->set_margin_bottom(0);
    edit_connection_menu_item_toolbar->signal_clicked().connect(
        [&notebook]() { edit_connection_dialog(notebook); }
    );
    toolbar->append(*edit_connection_menu_item_toolbar);

    // Duplicate Connection Button
    duplicate_connection_menu_item_toolbar = create_toolbar_button("Duplicate Connection", dupeconn_png, dupeconn_png_len);
    duplicate_connection_menu_item_toolbar->set_tooltip_text("Duplicate Connection");
    duplicate_connection_menu_item_toolbar->set_margin_start(1);
    duplicate_connection_menu_item_toolbar->set_margin_end(1);
    duplicate_connection_menu_item_toolbar->set_margin_top(0);
    duplicate_connection_menu_item_toolbar->set_margin_bottom(0);
    duplicate_connection_menu_item_toolbar->signal_clicked().connect(
        [&notebook]() {
            duplicate_connection_dialog(notebook);
        }
    );
    toolbar->append(*duplicate_connection_menu_item_toolbar);

    // Delete Connection Button
    delete_connection_menu_item_toolbar = create_toolbar_button("Delete Connection", rmconn_png, rmconn_png_len);
    delete_connection_menu_item_toolbar->set_tooltip_text("Delete Connection");
    delete_connection_menu_item_toolbar->set_margin_start(1);
    delete_connection_menu_item_toolbar->set_margin_end(1);
    delete_connection_menu_item_toolbar->set_margin_top(0);
    delete_connection_menu_item_toolbar->set_margin_bottom(0);
    delete_connection_menu_item_toolbar->signal_clicked().connect(
        [&notebook, &columns_ref, &connections_liststore]() {
            if (!connections_treeview) return;
            Glib::RefPtr<Gtk::TreeSelection> selection = connections_treeview->get_selection();
            if (selection) {
                Gtk::TreeModel::iterator iter = selection->get_selected();
                if (iter) {
                    bool is_connection = (*iter)[columns_ref.is_connection];
                    if (is_connection) {
                        Glib::ustring conn_id = static_cast<Glib::ustring>((*iter)[columns_ref.id]);
                        Glib::ustring conn_name = static_cast<Glib::ustring>((*iter)[columns_ref.name]);
                        delete_connection_dialog(notebook, conn_id, conn_name);
                    }
                }
            }
        }
    );
    toolbar->append(*delete_connection_menu_item_toolbar);

    // Add a separator
    Gtk::SeparatorToolItem* sep = Gtk::manage(new Gtk::SeparatorToolItem());
    toolbar->append(*sep);

    // RDP connections are now handled by double-clicking on RDP connections in the treeview

    // Pack Toolbar at the TOP of the main_vbox
    main_vbox.pack_start(*toolbar, false, false, 0);

    // Create a horizontal paned widget
    main_hpaned = new Gtk::HPaned();

    // Pack the paned widget into the main vbox
    main_vbox.pack_start(*main_hpaned, true, true, 0);

    // Calculate total width needed for 250px content + margins + borders
    const int content_width = 250;  // Desired content width
    const int frame_margin = 10;    // 5px margin on each side
    const int border_width = 2;     // 1px border on each side
    const int total_min_width = content_width + frame_margin + border_width * 2;

    // Set initial position from config
    int initial_width = Config::get()["left_frame_width"].get<int>();
    main_hpaned->set_position(initial_width + frame_margin + border_width * 2);

    // Connect position changed signal using property
    main_hpaned->property_position().signal_changed().connect(
        [main_hpaned = std::ref(main_hpaned), total_min_width, content_width]() {
            int current_pos = main_hpaned.get()->get_position();
            if (current_pos < total_min_width) {
                main_hpaned.get()->set_position(total_min_width);
            }
        }
    );

    // Left frame for connections TreeView and Info Section
    Gtk::Box* left_side_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
    left_side_box->set_hexpand(false);  // Don't expand horizontally beyond minimum
    left_side_box->set_halign(Gtk::ALIGN_FILL);
    left_side_box->set_valign(Gtk::ALIGN_FILL);
    left_side_box->set_size_request(250, -1);  // Set minimum width
    left_side_box->set_margin_start(0);
    left_side_box->set_margin_end(0);
    left_side_box->set_margin_top(0);
    left_side_box->set_margin_bottom(0);
    left_side_box->property_width_request() = 250;  // Explicitly set width request

    // Instantiate the Frame that build_leftFrame will populate
    left_frame_top = Gtk::manage(new Gtk::Frame());
    left_frame_top->set_shadow_type(Gtk::SHADOW_ETCHED_IN);
    left_frame_top->set_vexpand(true);  // Allow vertical expansion
    left_frame_top->set_hexpand(false);  // Don't expand horizontally
    left_frame_top->set_halign(Gtk::ALIGN_FILL);
    left_frame_top->set_valign(Gtk::ALIGN_FILL);
    left_frame_top->set_size_request(250, -1);  // Set minimum width

    // ScrolledWindow for TreeView - will be passed to build_leftFrame
    Gtk::ScrolledWindow* connections_scrolled_window = Gtk::manage(new Gtk::ScrolledWindow());
    connections_scrolled_window->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    connections_scrolled_window->set_vexpand(true);
    connections_scrolled_window->set_hexpand(true);
    connections_scrolled_window->set_min_content_width(250);  // Minimum width for content

    // Call build_leftFrame to populate left_frame_top
    build_leftFrame(window, *left_frame_top, *connections_scrolled_window,
                   *connections_treeview, connections_liststore, connection_columns, notebook);

    // Setup the Info Frame
    Gtk::Label* frame_title_label = Gtk::manage(new Gtk::Label("", Gtk::ALIGN_START));
    frame_title_label->set_markup("<b>Connection Information</b>");
    info_frame->set_label_widget(*frame_title_label);
    info_frame->set_shadow_type(Gtk::SHADOW_ETCHED_IN);
    info_frame->set_border_width(5);

    // Ensure info_frame is empty before adding info_grid
    Gtk::Widget* current_info_child = info_frame->get_child();
    if (current_info_child) {
        info_frame->remove();
    }
    info_frame->add(*info_grid);

    // Add labels to the grid - Type, Hostname, Port order
    Gtk::Label type_label("Type", Gtk::ALIGN_START, Gtk::ALIGN_START);
    type_label.set_markup("<b>Type:</b>");
    info_grid->attach(type_label, 0, 0, 1, 1);
    info_grid->attach(*type_value_label, 1, 0, 1, 1);

    Gtk::Label host_label("Hostname", Gtk::ALIGN_START, Gtk::ALIGN_START);
    host_label.set_markup("<b>Hostname:</b>");
    info_grid->attach(host_label, 0, 1, 1, 1);
    info_grid->attach(*host_value_label, 1, 1, 1, 1);

    Gtk::Label port_label("Port", Gtk::ALIGN_START, Gtk::ALIGN_START);
    port_label.set_markup("<b>Port:</b>");
    info_grid->attach(port_label, 0, 2, 1, 1);
    info_grid->attach(*port_value_label, 1, 2, 1, 1);

    // Add vertical spacing between rows
    info_grid->set_row_spacing(10); // 10 pixels between rows
    info_grid->set_column_spacing(10); // 10 pixels between columns

    // Pack both frames into the left side box
    left_side_box->pack_start(*left_frame_top, true, true, 0); // Set both expand and fill to true
    left_side_box->pack_start(*info_frame, false, true, 0); // Set expand to false, fill to true

    // Add the box containing both frames to the HPaned
    main_hpaned->add1(*left_side_box);

    // Right frame for notebook (terminals)
    main_hpaned->add2(notebook);

    // Connections TreeView setup
    connections_treeview->get_selection()->signal_changed().connect(sigc::ptr_fun(&on_connection_selection_changed)); // Use ->

    // Populate the TreeView after setting up selection handler
    populate_connections_treeview(connections_liststore, connection_columns, *connections_treeview);

    // Add double-click event handler
    Glib::RefPtr<Gtk::TreeModel> treemodel = connections_treeview->get_model();

    connections_treeview->signal_row_activated().connect(
        [&notebook, treemodel](const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* column) {

            if (!treemodel) {
                return;
            }

            auto iter = treemodel->get_iter(path);
            if (!iter) {
                return;
            }

            Gtk::TreeModel::Row row = *iter;
            bool is_folder = row[connection_columns.is_folder];

            if (is_folder) {
                return; // Do nothing if a folder is double-clicked
            }

            bool is_connection = row[connection_columns.is_connection];

            if (is_connection) {
                Glib::ustring conn_id = row[connection_columns.id];
                Glib::ustring conn_name = row[connection_columns.name];

                // Check if this connection is already open
                if (!Config::get_always_new_connection()) {
                    // Try to find an existing tab for this connection
                    for (int i = 0; i < notebook.get_n_pages(); ++i) {
                        Gtk::Widget* page = notebook.get_nth_page(i);
                        Glib::ustring tab_text = notebook.get_tab_label_text(*page);

                        // If we find a matching tab, switch to it and return
                        if (tab_text == conn_name) {
                            notebook.set_current_page(i);
                            // Get the terminal widget and focus it
                            if (Gtk::Widget* page = notebook.get_nth_page(i)) {
                                if (GtkWidget* terminal = GTK_WIDGET(VTE_TERMINAL(page->gobj()))) {
                                    gtk_widget_grab_focus(terminal);
                                }
                            }
                            return;
                        }
                    }
                }
                // Create new terminal for the connection
                GtkWidget* terminal = vte_terminal_new();
                vte_terminal_set_scrollback_lines(VTE_TERMINAL(terminal), 10000);

                // Load the full saved connection details
                std::vector<ConnectionInfo> saved_connections = ConnectionManager::load_connections();
                auto conn_it = std::find_if(saved_connections.begin(), saved_connections.end(),
                    [&conn_id](const ConnectionInfo& ci) { return ci.id == conn_id; });

                if (conn_it == saved_connections.end()) {
                    std::cerr << "Error: Could not find connection details for ID: " << conn_id << std::endl;
                    return;
                }

                // Create tab label with connection name
                Gtk::Label* label = Gtk::manage(new Gtk::Label(conn_name, Gtk::ALIGN_START));
                // Create terminal data for cleanup
                TerminalData* term_data = new TerminalData();
                term_data->notebook = &notebook;

                // Add terminal to notebook in new tab
                Gtk::Widget* term_widget = Gtk::manage(Glib::wrap(terminal));
                int page_num = notebook.append_page(*term_widget, *label);
                term_data->page_num = page_num;

                // Show all widgets
                notebook.show_all();
                term_widget->show();

                // Force UI update
                while (Gtk::Main::events_pending()) {
                    Gtk::Main::iteration();
                }

                // Make sure the new tab is visible and selected
                notebook.set_current_page(page_num);

                // Ensure the terminal has focus
                gtk_widget_grab_focus(terminal);

                // Store the connection in our tracking map
                open_connections[conn_id.raw()] = page_num;

                // Connect to child-exited signal to handle cleanup
                g_signal_connect(terminal, "child-exited", G_CALLBACK(on_terminal_child_exited), term_data);

                // Handle different connection types
                if (conn_it->connection_type == "SSH") {
                    std::vector<std::string> command_args = Ssh::generate_ssh_command_args(*conn_it);
                    if (!command_args.empty()) {
                        std::vector<char*> argv;
                        for (const auto& arg : command_args) {
                            argv.push_back(const_cast<char*>(arg.c_str()));
                        }
                        argv.push_back(nullptr);

                        // Set up the terminal
                        char** env = g_get_environ();
                        vte_terminal_spawn_async(
                            VTE_TERMINAL(terminal),
                            VTE_PTY_DEFAULT,
                            nullptr,     // working directory
                            argv.data(), // command
                            env,         // environment
                            G_SPAWN_SEARCH_PATH,
                            nullptr, nullptr, nullptr, // child setup
                            -1,         // timeout
                            nullptr,    // cancellable
                            nullptr,    // callback
                            nullptr     // user_data
                        );
                        g_strfreev(env);
                    } else {
                        std::cerr << "Error: Empty command args for SSH connection" << std::endl;
                    }
                } else if (conn_it->connection_type == "RDP") {
                    // For RDP connections, launch the RDP session
                    // First, remove the terminal tab we just created
                    notebook.remove_page(page_num);

                    // Launch the RDP session with domain if available
                    launch_rdp_session(notebook,
                                     conn_it->host,
                                     conn_it->username,
                                     conn_it->password,
                                     conn_it->domain);
                }
            }
        }
    );

    // Add cleanup when tab is closed
    notebook.signal_page_removed().connect([](Gtk::Widget* page, guint page_num) {
        // Cleanup will be handled by child-exited signal
    });

    // Add resize event handler to save window dimensions and log sizes
    window.signal_size_allocate().connect([&window, &notebook](Gtk::Allocation& allocation) {
        // Get window dimensions
        int width = allocation.get_width();
        int height = allocation.get_height();

        // Get active page (if any)
        auto page_num = notebook.get_current_page();
        if (page_num >= 0) {
            auto page = notebook.get_nth_page(page_num);
            if (page) {
                // Check if this is an RDP page
                auto rdp_box = dynamic_cast<Gtk::Box*>(page);
                if (rdp_box) {
                    // Just check if we have any children, but don't store the allocation
                    auto children = rdp_box->get_children();
                    if (!children.empty()) {
                        // We have children but don't need to store their allocations
                    }
                }
            }
        }

        // Save window size to config if needed
        if (Config::get_save_window_coords()) {
            // Get current config and update it
            json new_config = Config::get();
            if (new_config["window_width"] != width || new_config["window_height"] != height) {
                new_config["window_width"] = width;
                new_config["window_height"] = height;
                Config::update(new_config);
            }
        }
    });

    // Fallback resize event handler
    window.signal_configure_event().connect([&window](GdkEventConfigure* event) {
        if (Config::get_save_window_coords()) {
            // Get current config
            json new_config = Config::get();

            // Update window coordinates
            int x, y;
            window.get_position(x, y);

            // Save the new coordinates
            new_config["window_x"] = x;
            new_config["window_y"] = y;
            new_config["window_width"] = event->width;
            new_config["window_height"] = event->height;
            Config::update(new_config);
        }
        return false;
    });

    // Connect the delete event
    window.signal_delete_event().connect([&window](GdkEventAny*) {
        save_frame_width(window);
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