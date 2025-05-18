#include "main.h"
#include "icondata.h"

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

// Function to handle adding, editing, or duplicating a connection
void process_connection_dialog(Gtk::Notebook& notebook, DialogPurpose purpose, const ConnectionInfo* existing_connection = nullptr) {
    std::string dialog_title;
    std::string confirm_button_text;

    switch (purpose) {
        case DialogPurpose::ADD:
            dialog_title = "Add New Connection";
            confirm_button_text = "Add";
            break;
        case DialogPurpose::EDIT:
            dialog_title = "Edit Connection";
            confirm_button_text = "Save";
            break;
        case DialogPurpose::DUPLICATE:
            dialog_title = "Duplicate Connection";
            confirm_button_text = "Duplicate";
            break;
    }

    Gtk::Dialog dialog(dialog_title, true /* modal */);
    dialog.set_default_size(450, 0); // Adjusted default width, height will adapt

    Gtk::Grid* grid = Gtk::manage(new Gtk::Grid());
    grid->set_hexpand(true); // Ensure the main grid expands horizontally
    grid->set_border_width(10);
    grid->set_column_spacing(10);
    grid->set_row_spacing(10);
    dialog.get_content_area()->pack_start(*grid, Gtk::PACK_EXPAND_WIDGET);

    std::string initially_selected_folder_id = "";
    if (existing_connection) {
        initially_selected_folder_id = existing_connection->folder_id;
    } else if (connections_treeview) { // Assuming connections_treeview is globally accessible or passed in
        Glib::RefPtr<Gtk::TreeSelection> selection = connections_treeview->get_selection();
        if (selection) {
            Gtk::TreeModel::iterator iter = selection->get_selected();
            if (iter) {
                Gtk::TreeModel::Row row = *iter;
                // Assuming connection_columns is globally accessible or passed in
                if (row[connection_columns.is_folder]) {
                    initially_selected_folder_id = static_cast<Glib::ustring>(row[connection_columns.id]);
                }
            }
        }
    }

    Gtk::Label folder_label("Folder:", Gtk::ALIGN_START);
    Gtk::ComboBoxText folder_combo;
    folder_combo.set_hexpand(true);
    std::vector<FolderInfo> folders = ConnectionManager::load_folders(); // Assumes FolderInfo struct/class
    folder_combo.append("", "None (Root Level)");
    for (const auto& folder : folders) {
        folder_combo.append(folder.id, folder.name);
    }
    if (!initially_selected_folder_id.empty()) {
        folder_combo.set_active_id(initially_selected_folder_id);
    } else {
        folder_combo.set_active(0); // Select "None (Root Level)" if no folder pre-selected
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
    Gtk::Entry name_entry;
    name_entry.set_hexpand(true);
    if (existing_connection) {
        if (purpose == DialogPurpose::DUPLICATE) {
            name_entry.set_text(existing_connection->name + " (Copy)");
        } else {
            name_entry.set_text(existing_connection->name);
        }
    }

    Gtk::Label host_label("Host:", Gtk::ALIGN_START);
    Gtk::Entry host_entry;
    host_entry.set_hexpand(true);
    if (existing_connection) {
        host_entry.set_text(existing_connection->host);
    }

    Gtk::Label port_label("Port:", Gtk::ALIGN_START);
    Gtk::Entry port_entry;
    port_entry.set_hexpand(true);
    if (existing_connection && existing_connection->port > 0) {
        port_entry.set_text(std::to_string(existing_connection->port));
    }

    Gtk::Label ssh_flags_label("SSH Flags:", Gtk::ALIGN_START);
    Gtk::Entry ssh_flags_entry;
    ssh_flags_entry.set_hexpand(true);
    if (existing_connection) {
        ssh_flags_entry.set_text(existing_connection->additional_ssh_options);
    }

    Gtk::Label user_label("Username:", Gtk::ALIGN_START);
    Gtk::Entry user_entry;
    user_entry.set_hexpand(true);
    if (existing_connection) {
        user_entry.set_text(existing_connection->username);
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
    password_entry.set_visibility(false); // Mask password
    password_entry.set_hexpand(true);
    const char* password_security_warning_text =
        "WARNING: Password will be stored in plain text and may be visible in the process list. "
        "This is inherently insecure. Consider using SSH keys or do not save the password here. "
        "You will be prompted for the password when connecting.";
    password_entry.set_tooltip_text(password_security_warning_text);

    // Custom popup tooltip window for password - declared here, connected in update_visibility
    Gtk::Window* password_tooltip_window = nullptr;

    // Create a tooltip window
    password_entry.signal_focus_in_event().connect([password_security_warning_text, &password_tooltip_window, &password_entry](GdkEventFocus*) -> bool {
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

            // Set a fixed size for the tooltip
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
    });

    // Hide tooltip when password field loses focus
    password_entry.signal_focus_out_event().connect([&password_tooltip_window](GdkEventFocus*) -> bool {
        if (password_tooltip_window) {
            password_tooltip_window->hide();
        }
        return false;
    });

    if (purpose == DialogPurpose::DUPLICATE) {
        password_entry.set_text(""); // Clear password for duplicate
    } else if (existing_connection && purpose == DialogPurpose::EDIT) {
        password_entry.set_text(existing_connection->password);
    } else { // ADD or if existing_connection is null for some reason
        password_entry.set_text("");
    }

    Gtk::Label ssh_key_path_label("SSH Key Path:", Gtk::ALIGN_START);
    Gtk::Entry ssh_key_path_entry;
    ssh_key_path_entry.set_hexpand(true);
    Gtk::Button ssh_key_browse_button("Browse..."); // Added browse button
     if (existing_connection) {
        ssh_key_path_entry.set_text(existing_connection->ssh_key_path);
    }

    Gtk::Label ssh_key_passphrase_label("SSH Key Passphrase:", Gtk::ALIGN_START);
    Gtk::Entry ssh_key_passphrase_entry;
    ssh_key_passphrase_entry.set_visibility(false); // Mask passphrase
    ssh_key_passphrase_entry.set_hexpand(true);
    ssh_key_passphrase_entry.set_tooltip_text("Enter passphrase for SSH key (if protected).");

    if (existing_connection) { // EDIT or DUPLICATE
        ssh_key_passphrase_entry.set_text(existing_connection->ssh_key_passphrase); // Retain for DUPLICATE, show for EDIT
    } else { // ADD
        ssh_key_passphrase_entry.set_text("");
    }

    // Arrange widgets in the grid
    int current_row = 0;
    grid->attach(folder_label,               0, current_row, 1, 1);
    grid->attach(folder_combo,               1, current_row++, 2, 1); // Spanning 2 columns for entry/combo
    grid->attach(type_label,                 0, current_row, 1, 1);
    grid->attach(type_combo,                 1, current_row++, 2, 1);
    grid->attach(name_label,                 0, current_row, 1, 1);
    grid->attach(name_entry,                 1, current_row++, 2, 1);
    grid->attach(host_label,                 0, current_row, 1, 1);
    grid->attach(host_entry,                 1, current_row++, 2, 1);
    grid->attach(port_label,                 0, current_row, 1, 1);
    grid->attach(port_entry,                 1, current_row++, 2, 1);

    // SSH Specific fields will be added and visibility toggled
    grid->attach(ssh_flags_label,            0, current_row, 1, 1);
    grid->attach(ssh_flags_entry,            1, current_row++, 2, 1);
    grid->attach(user_label,                 0, current_row, 1, 1);
    grid->attach(user_entry,                 1, current_row++, 2, 1);
    grid->attach(auth_method_label,          0, current_row, 1, 1);
    grid->attach(auth_method_combo,          1, current_row++, 2, 1);
    grid->attach(password_label,             0, current_row, 1, 1);
    grid->attach(password_entry,             1, current_row++, 2, 1);
    grid->attach(ssh_key_path_label,         0, current_row, 1, 1);
    grid->attach(ssh_key_path_entry,         1, current_row,   1, 1); // Entry takes 1 column
    grid->attach(ssh_key_browse_button,      2, current_row++, 1, 1); // Button takes 1 column
    grid->attach(ssh_key_passphrase_label,   0, current_row, 1, 1);
    grid->attach(ssh_key_passphrase_entry,   1, current_row++, 2, 1);

    ssh_key_browse_button.signal_clicked().connect([&dialog, &ssh_key_path_entry]() {
        Gtk::FileChooserDialog fc_dialog(dialog, "Select SSH Private Key", Gtk::FILE_CHOOSER_ACTION_OPEN);
        fc_dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
        fc_dialog.add_button("_Open", Gtk::RESPONSE_OK);
        auto filter_any = Gtk::FileFilter::create();
        filter_any->set_name("All files");
        filter_any->add_pattern("*");
        fc_dialog.add_filter(filter_any);
        const char* home_dir = std::getenv("HOME");
        if (home_dir) fc_dialog.set_current_folder(home_dir);
        if (fc_dialog.run() == Gtk::RESPONSE_OK) {
            ssh_key_path_entry.set_text(fc_dialog.get_filename());
        }
    });

    // Function to toggle SSH specific fields based on auth method and connection type
    auto toggle_ssh_fields = [&](const Glib::ustring& active_type, const Glib::ustring& active_auth_method) {
        bool is_ssh = (active_type == "SSH");
        bool use_ssh_key = (active_auth_method == "SSHKey");

        // SSH Flags, Username, Auth Method are always visible for SSH (or controlled by is_ssh)
        ssh_flags_label.set_visible(is_ssh);
        ssh_flags_entry.set_visible(is_ssh);
        // Username might be relevant for non-SSH too, so keep it generally visible or adapt
        // user_label.set_visible(is_ssh);
        // user_entry.set_visible(is_ssh);
        auth_method_label.set_visible(is_ssh);
        auth_method_combo.set_visible(is_ssh);

        password_label.set_visible(is_ssh && !use_ssh_key);
        password_entry.set_visible(is_ssh && !use_ssh_key);

        ssh_key_path_label.set_visible(is_ssh && use_ssh_key);
        ssh_key_path_entry.set_visible(is_ssh && use_ssh_key);
        ssh_key_browse_button.set_visible(is_ssh && use_ssh_key);
        ssh_key_passphrase_label.set_visible(is_ssh && use_ssh_key);
        ssh_key_passphrase_entry.set_visible(is_ssh && use_ssh_key);

        // For non-SSH types, hide all SSH-specific auth fields
        // Username and port are generally applicable. Host, Name, Type, Folder are always applicable.
        if (!is_ssh) {
             // SSH Flags are definitely SSH only
            ssh_flags_label.set_visible(false);
            ssh_flags_entry.set_visible(false);
            auth_method_label.set_visible(false);
            auth_method_combo.set_visible(false);
            password_label.set_visible(false);
            password_entry.set_visible(false);
            ssh_key_path_label.set_visible(false);
            ssh_key_path_entry.set_visible(false);
            ssh_key_browse_button.set_visible(false);
            ssh_key_passphrase_label.set_visible(false);
            ssh_key_passphrase_entry.set_visible(false);
        }
        dialog.queue_resize(); // Advise dialog to re-calculate size
    };

    // Initial state based on connection type and auth method
    toggle_ssh_fields(type_combo.get_active_text(), auth_method_combo.get_active_text());

    type_combo.signal_changed().connect([&]() {
        std::string selected_type = type_combo.get_active_text();
        std::string current_port_text = port_entry.get_text();
        // Auto-fill default port only if port is empty or was a known default
         bool port_is_empty_or_default = current_port_text.empty() ||
                                        current_port_text == "22" || // SSH
                                        current_port_text == "23" || // Telnet
                                        current_port_text == "5900" || // VNC
                                        current_port_text == "3389";  // RDP
        if (port_is_empty_or_default) {
            if (selected_type == "SSH") port_entry.set_text("22");
            else if (selected_type == "Telnet") port_entry.set_text("23");
            else if (selected_type == "VNC") port_entry.set_text("5900");
            else if (selected_type == "RDP") port_entry.set_text("3389");
            else port_entry.set_text("");
        }
        toggle_ssh_fields(type_combo.get_active_text(), auth_method_combo.get_active_text());
    });
    auth_method_combo.signal_changed().connect([&]() {
        toggle_ssh_fields(type_combo.get_active_text(), auth_method_combo.get_active_text());
    });

    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button(confirm_button_text, Gtk::RESPONSE_OK);

    dialog.get_content_area()->show_all_children(); // Show all children first
    dialog.show_all(); // Show the dialog itself
    toggle_ssh_fields(type_combo.get_active_text(), auth_method_combo.get_active_text()); // Then set visibility

    int result = dialog.run();
    if (result == Gtk::RESPONSE_OK) {
        ConnectionInfo new_connection; // Use ConnectionInfo as discussed
        // Assign ID: existing for EDIT, new for ADD/DUPLICATE
        if (purpose == DialogPurpose::EDIT && existing_connection) {
            new_connection.id = existing_connection->id;
        } else {
            new_connection.id = ConnectionManager::generate_connection_id();
        }

        new_connection.name = name_entry.get_text();
        new_connection.host = host_entry.get_text();
        new_connection.connection_type = type_combo.get_active_text();
        new_connection.username = user_entry.get_text();
        new_connection.folder_id = folder_combo.get_active_id(); // Make sure get_active_id() returns string

        std::string port_str = port_entry.get_text();
        if (!port_str.empty()) {
            try {
                new_connection.port = std::stoi(port_str);
            } catch (const std::exception& e) {
                // Handle invalid port format
                 Gtk::MessageDialog err_dialog(dialog, "Invalid Port", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
                 err_dialog.set_secondary_text("Port number is invalid. Please enter a valid number or leave it empty.");
                 err_dialog.run();
                 return; // Or re-show dialog
            }
        } else {
            new_connection.port = 0; // Default or indicate no port
        }

        if (new_connection.connection_type == "SSH") {
            new_connection.additional_ssh_options = ssh_flags_entry.get_text();
            new_connection.auth_method = auth_method_combo.get_active_text();
            if (new_connection.auth_method == "Password") {
                new_connection.password = password_entry.get_text();
                new_connection.ssh_key_path = ""; // Clear SSH key info
                new_connection.ssh_key_passphrase = "";
            } else if (new_connection.auth_method == "SSHKey") {
                new_connection.password = ""; // Clear password info
                new_connection.ssh_key_path = ssh_key_path_entry.get_text();
                new_connection.ssh_key_passphrase = ssh_key_passphrase_entry.get_text();
            }
        } else {
            // Clear all SSH specific fields if not SSH type
            new_connection.additional_ssh_options = "";
            new_connection.auth_method = "";
            new_connection.password = "";
            new_connection.ssh_key_path = "";
            new_connection.ssh_key_passphrase = "";
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

    Gtk::Box* vbox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0));
    Gtk::Widget* current_child = left_frame.get_child();
    if (current_child) {
        left_frame.remove();
    }
    left_frame.add(*vbox);
    left_scrolled_window.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    int min_width = 250;
    int width = std::max(Config::get()["left_frame_width"].get<int>(), min_width);
    left_frame.set_size_request(width, -1);

    connections_treeview_ref.set_hexpand(true);
    connections_treeview_ref.set_vexpand(true);
    connections_treeview_ref.set_model(liststore_ref);
    if (connections_treeview_ref.get_columns().empty()) {
        connections_treeview_ref.append_column("Connections", columns_ref.name);
    }
    left_scrolled_window.add(connections_treeview_ref);
    vbox->pack_start(left_scrolled_window, Gtk::PACK_EXPAND_WIDGET);
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
        new_config["left_frame_width"] = frame_width;
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

    // Pack Toolbar at the TOP of the main_vbox
    main_vbox.pack_start(*toolbar, false, false, 0);

    // Create a horizontal paned widget
    main_hpaned = new Gtk::HPaned();
    main_vbox.pack_start(*main_hpaned, true, true, 0);

    // Set initial position from config
    int initial_width = Config::get()["left_frame_width"].get<int>();
    if (initial_width < 250) {
        initial_width = 250;
    }
    main_hpaned->set_position(initial_width);

    // Connect position changed signal using property
    main_hpaned->property_position().signal_changed().connect(
        [main_hpaned = std::ref(main_hpaned)]() {
            int current_pos = main_hpaned.get()->get_position();
            if (current_pos < 250) {
                main_hpaned.get()->set_position(250);
            }
        }
    );

    // Left frame for connections TreeView and Info Section
    Gtk::Box* left_side_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
    left_side_box->set_size_request(250, -1); // Set minimum width

    // Instantiate the Frame that build_leftFrame will populate
    left_frame_top = new Gtk::Frame();
    left_frame_top->set_vexpand(true); // Allow vertical expansion
    left_frame_top->set_hexpand(true); // Allow horizontal expansion

    // ScrolledWindow for TreeView - will be passed to build_leftFrame
    Gtk::ScrolledWindow connections_scrolled_window;
    connections_scrolled_window.set_vexpand(true); // Allow vertical expansion
    connections_scrolled_window.set_hexpand(true); // Allow horizontal expansion

    // Call build_leftFrame to populate left_frame_top
    build_leftFrame(window, *left_frame_top, connections_scrolled_window,
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

    // Connect position changed signal using property
    main_hpaned->property_position().signal_changed().connect(
        [main_hpaned = std::ref(main_hpaned)]() {
            int current_pos = main_hpaned.get()->get_position();
            if (current_pos < 250) {
                main_hpaned.get()->set_position(250);
            }
        }
    );

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

                // Start the SSH process
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
                }
            }
        }
    );

    // Add cleanup when tab is closed
    notebook.signal_page_removed().connect([](Gtk::Widget* page, guint page_num) {
        // Cleanup will be handled by child-exited signal
    });

    // Add resize event handler to save window dimensions
    window.signal_size_allocate().connect([&window](Gtk::Allocation& allocation) {
        if (Config::get_save_window_coords()) {
            int width = allocation.get_width();
            int height = allocation.get_height();

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