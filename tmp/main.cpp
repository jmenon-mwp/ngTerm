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
#include "Ssh.h" // Include Ssh.h
#include "Config.h" // Include the new Config.h
#include <sys/wait.h> // For wait status macros (WIFEXITED, WEXITSTATUS, etc.)

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
    grid->set_hexpand(true); // Ensure the main grid expands horizontally
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

    // SSH Flags
    Gtk::Label ssh_flags_label("SSH Flags:");
    ssh_flags_label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry ssh_flags_entry;
    ssh_flags_entry.set_hexpand(true);
    ssh_flags_entry.set_text(current_connection.additional_ssh_options);

    // Username
    Gtk::Label user_label("Username:");
    user_label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry user_entry;
    user_entry.set_hexpand(true);
    user_entry.set_text(current_connection.username);

    // SSH Specific Fields
    Gtk::Label auth_method_label("Auth Method:");
    auth_method_label.set_halign(Gtk::ALIGN_START);
    Gtk::ComboBoxText auth_method_combo;
    auth_method_combo.append("Password", "Password");
    auth_method_combo.append("SSHKey", "SSH Key");
    auth_method_combo.set_active_text(current_connection.auth_method);
    auth_method_combo.set_hexpand(true);

    Gtk::Label password_label("Password:");
    password_label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry password_entry;
    password_entry.set_visibility(false); // Mask password
    password_entry.set_hexpand(true);
    password_entry.set_text(current_connection.password);

    Gtk::Label ssh_key_label("SSH Key File:");
    ssh_key_label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry ssh_key_path_entry;
    ssh_key_path_entry.set_hexpand(true);
    ssh_key_path_entry.set_editable(false); // User should use browse button
    ssh_key_path_entry.set_text(current_connection.ssh_key_path);
    Gtk::Button ssh_key_browse_button("Browse...");
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

    Gtk::Label ssh_key_passphrase_label("SSH Passphrase:");
    ssh_key_passphrase_label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry ssh_key_passphrase_entry;
    ssh_key_passphrase_entry.set_visibility(false); // Mask passphrase
    ssh_key_passphrase_entry.set_hexpand(true);
    ssh_key_passphrase_entry.set_text(current_connection.ssh_key_passphrase);

    // Attach to grid
    int currentRow = 0;
    grid->attach(folder_label, 0, currentRow, 1, 1);
    grid->attach(folder_combo, 1, currentRow++, 1, 1);
    grid->attach(type_label, 0, currentRow, 1, 1);
    grid->attach(type_combo, 1, currentRow++, 1, 1);
    grid->attach(name_label, 0, currentRow, 1, 1);
    grid->attach(name_entry, 1, currentRow++, 1, 1);
    grid->attach(host_label, 0, currentRow, 1, 1);
    grid->attach(host_entry, 1, currentRow++, 1, 1);
    grid->attach(port_label, 0, currentRow, 1, 1);
    grid->attach(port_entry, 1, currentRow++, 1, 1);
    grid->attach(ssh_flags_label, 0, currentRow, 1, 1);
    grid->attach(ssh_flags_entry, 1, currentRow++, 1, 1);
    grid->attach(user_label, 0, currentRow, 1, 1);
    grid->attach(user_entry, 1, currentRow++, 1, 1);
    grid->attach(auth_method_label, 0, currentRow, 1, 1);
    grid->attach(auth_method_combo, 1, currentRow++, 1, 1);
    grid->attach(password_label, 0, currentRow, 1, 1);
    grid->attach(password_entry, 1, currentRow++, 1, 1);
    grid->attach(ssh_key_label, 0, currentRow, 1, 1);
    grid->attach(ssh_key_path_entry, 1, currentRow, 1, 1);
    grid->attach(ssh_key_browse_button, 2, currentRow++, 1, 1);
    grid->attach(ssh_key_passphrase_label, 0, currentRow, 1, 1);
    grid->attach(ssh_key_passphrase_entry, 1, currentRow++, 1, 1);

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

    dialog.show_all(); // Show the dialog and its initial contents first

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

        if (updated_connection.connection_type == "SSH") {
            updated_connection.auth_method = auth_method_combo.get_active_text();
            if (updated_connection.auth_method == "Password") {
                updated_connection.password = password_entry.get_text();
                // Clear SSH key fields for hygiene
                updated_connection.ssh_key_path = "";
                updated_connection.ssh_key_passphrase = "";
            } else if (updated_connection.auth_method == "SSHKey") {
                updated_connection.ssh_key_path = ssh_key_path_entry.get_text();
                updated_connection.ssh_key_passphrase = ssh_key_passphrase_entry.get_text();
                // Clear password field for hygiene
                updated_connection.password = "";
            }
            updated_connection.additional_ssh_options = ssh_flags_entry.get_text();
        } else {
            // Clear SSH specific fields if not an SSH connection
            updated_connection.auth_method = "";
            updated_connection.password = "";
            updated_connection.ssh_key_path = "";
            updated_connection.ssh_key_passphrase = "";
            updated_connection.additional_ssh_options = "";
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
    dialog.set_default_size(450, 0); // Adjusted default width, height will adapt

    Gtk::Grid* grid = Gtk::manage(new Gtk::Grid());
    grid->set_hexpand(true); // Ensure the main grid expands horizontally
    grid->set_border_width(10);
    grid->set_column_spacing(10);
    grid->set_row_spacing(10);
    dialog.get_content_area()->pack_start(*grid, Gtk::PACK_EXPAND_WIDGET);

    std::string initially_selected_folder_id = "";
    if (connections_treeview) {
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

    Gtk::Label folder_label("Folder:");
    folder_label.set_halign(Gtk::ALIGN_START);
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

    Gtk::Label type_label("Connection Type:");
    type_label.set_halign(Gtk::ALIGN_START);
    Gtk::ComboBoxText type_combo;
    type_combo.set_hexpand(true);
    type_combo.append("SSH", "SSH");
    type_combo.append("Telnet", "Telnet");
    type_combo.append("VNC", "VNC");
    type_combo.append("RDP", "RDP");
    type_combo.set_active_text("SSH");

    Gtk::Label name_label("Connection Name:");
    name_label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry name_entry;
    name_entry.set_hexpand(true);

    Gtk::Label host_label("Host:");
    host_label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry host_entry;
    host_entry.set_hexpand(true);

    Gtk::Label port_label("Port:");
    port_label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry port_entry;
    port_entry.set_hexpand(true);

    // SSH Flags
    Gtk::Label ssh_flags_label("SSH Flags:");
    ssh_flags_label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry ssh_flags_entry;
    ssh_flags_entry.set_hexpand(true);

    // Username
    Gtk::Label user_label("Username:");
    user_label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry user_entry;
    user_entry.set_hexpand(true);

    // SSH Specific Fields
    Gtk::Label auth_method_label("Auth Method:");
    auth_method_label.set_halign(Gtk::ALIGN_START);
    Gtk::ComboBoxText auth_method_combo;
    auth_method_combo.append("Password", "Password");
    auth_method_combo.append("SSHKey", "SSH Key");
    auth_method_combo.set_active_text("Password"); // Default for SSH
    auth_method_combo.set_hexpand(true);

    Gtk::Label password_label("Password:");
    password_label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry password_entry;
    password_entry.set_visibility(false); // Mask password
    password_entry.set_hexpand(true);

    Gtk::Label ssh_key_label("SSH Key File:");
    ssh_key_label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry ssh_key_path_entry;
    ssh_key_path_entry.set_hexpand(true);
    ssh_key_path_entry.set_editable(false); // User should use browse button
    Gtk::Button ssh_key_browse_button("Browse...");

    Gtk::Label ssh_key_passphrase_label("SSH Passphrase:");
    ssh_key_passphrase_label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry ssh_key_passphrase_entry;
    ssh_key_passphrase_entry.set_visibility(false); // Mask passphrase
    ssh_key_passphrase_entry.set_hexpand(true);

    // Attach to grid
    int currentRow = 0;
    grid->attach(folder_label, 0, currentRow, 1, 1);
    grid->attach(folder_combo, 1, currentRow++, 2, 1);
    grid->attach(type_label, 0, currentRow, 1, 1);
    grid->attach(type_combo, 1, currentRow++, 2, 1);
    grid->attach(name_label, 0, currentRow, 1, 1);
    grid->attach(name_entry, 1, currentRow++, 2, 1);
    grid->attach(host_label, 0, currentRow, 1, 1);
    grid->attach(host_entry, 1, currentRow++, 2, 1);
    grid->attach(port_label, 0, currentRow, 1, 1);
    grid->attach(port_entry, 1, currentRow++, 2, 1);
    grid->attach(ssh_flags_label, 0, currentRow, 1, 1);
    grid->attach(ssh_flags_entry, 1, currentRow++, 2, 1);
    grid->attach(user_label, 0, currentRow, 1, 1);
    grid->attach(user_entry, 1, currentRow++, 2, 1);
    grid->attach(auth_method_label, 0, currentRow, 1, 1);
    grid->attach(auth_method_combo, 1, currentRow++, 2, 1);
    grid->attach(password_label, 0, currentRow, 1, 1);
    grid->attach(password_entry, 1, currentRow++, 2, 1);
    grid->attach(ssh_key_label, 0, currentRow, 1, 1);
    grid->attach(ssh_key_path_entry, 1, currentRow, 1, 1);
    grid->attach(ssh_key_browse_button, 2, currentRow++, 1, 1);
    grid->attach(ssh_key_passphrase_label, 0, currentRow, 1, 1);
    grid->attach(ssh_key_passphrase_entry, 1, currentRow++, 2, 1);

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

    auto update_visibility = [&]() {
        bool is_ssh = (type_combo.get_active_text() == "SSH");

        // Set visibility for all SSH-related labels and input widgets
        ssh_flags_label.set_visible(is_ssh);
        ssh_flags_entry.set_visible(is_ssh);
        auth_method_label.set_visible(is_ssh);
        auth_method_combo.set_visible(is_ssh);

        if (is_ssh) {
            bool is_password_auth = (auth_method_combo.get_active_text() == "Password");

            password_label.set_visible(is_password_auth);
            password_entry.set_visible(is_password_auth);

            ssh_key_label.set_visible(!is_password_auth);
            ssh_key_path_entry.set_visible(!is_password_auth);
            ssh_key_browse_button.set_visible(!is_password_auth);
            ssh_key_passphrase_label.set_visible(!is_password_auth);
            ssh_key_passphrase_entry.set_visible(!is_password_auth);

            // Clear fields that are being hidden based on auth method
            if (is_password_auth) {
                ssh_key_path_entry.set_text("");
                ssh_key_passphrase_entry.set_text("");
            } else {
                password_entry.set_text("");
            }
        } else {
            // Not SSH: hide all specific SSH input fields and their labels
            password_label.set_visible(false);
            password_entry.set_visible(false);
            ssh_key_label.set_visible(false);
            ssh_key_path_entry.set_visible(false);
            ssh_key_browse_button.set_visible(false);
            ssh_key_passphrase_label.set_visible(false);
            ssh_key_passphrase_entry.set_visible(false);

            // Also clear their content and reset auth_method_combo
            auth_method_combo.set_active_text("Password"); // Reset to default
            password_entry.set_text("");
            ssh_key_path_entry.set_text("");
            ssh_key_passphrase_entry.set_text("");
            ssh_flags_entry.set_text(""); // Clear SSH Flags content if not SSH
        }
        // dialog.queue_resize(); // Optional: if layout changes are complex
    };

    // Auto-fill port based on connection type
    type_combo.signal_changed().connect([&]() {
        std::string selected_type = type_combo.get_active_text();
        std::string current_port_text = port_entry.get_text();
        bool port_is_default_or_empty = current_port_text.empty() || current_port_text == "22" || current_port_text == "23" || current_port_text == "5900" || current_port_text == "3389";

        if (port_is_default_or_empty) {
            if (selected_type == "SSH") port_entry.set_text("22");
            else if (selected_type == "Telnet") port_entry.set_text("23");
            else if (selected_type == "VNC") port_entry.set_text("5900");
            else if (selected_type == "RDP") port_entry.set_text("3389");
            else if (port_is_default_or_empty) port_entry.set_text("");
        }
        update_visibility(); // Call after potentially changing connection type
    });

    auth_method_combo.signal_changed().connect(update_visibility);

    // Ensure all widgets that might be initially hidden by update_visibility are shown first,
    // then allow update_visibility to hide them as needed.
    // This means all child widgets of the dialog's content area should be shown.
    dialog.get_content_area()->show_all_children();
    // If buttons are in action area, ensure they are shown too, though add_button usually handles this.
    // dialog.get_action_area()->show_all_children(); // Less common to need this explicitly

    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Save", Gtk::RESPONSE_OK);

    dialog.show_all(); // Show the dialog itself and its initial contents

    // Set initial visibility correctly after dialog is shown and all widgets are realized
    update_visibility();

    // Auto-fill port if empty and type is SSH (for the very first time dialog appears)
    if (port_entry.get_text().empty() && type_combo.get_active_text() == "SSH") {
         port_entry.set_text("22");
    }

    int response = dialog.run();

    if (response == Gtk::RESPONSE_OK) {
        if (name_entry.get_text().empty()) {
            Gtk::MessageDialog md(dialog, "Connection name cannot be empty.", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
            md.run();
            return; // Or re-show dialog, or specific handling
        }
        ConnectionInfo new_connection;
        new_connection.id = ConnectionManager::generate_connection_id();
        new_connection.name = name_entry.get_text();
        new_connection.host = host_entry.get_text();

        std::string port_str = port_entry.get_text();
        if (!port_str.empty()) {
            try {
                new_connection.port = std::stoi(port_str);
            } catch (const std::invalid_argument& ia) {
                Gtk::MessageDialog err_dialog(dialog, "Invalid Port Number", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
                err_dialog.run();
                return;
            } catch (const std::out_of_range& oor) {
                Gtk::MessageDialog err_dialog(dialog, "Port Number Out of Range", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
                err_dialog.run();
                return;
            }
        } else {
            new_connection.port = 0; // Or a default based on type if desired
        }

        new_connection.username = user_entry.get_text();
        new_connection.connection_type = type_combo.get_active_text();

        Glib::ustring selected_folder_id_str = folder_combo.get_active_id();
        if (!selected_folder_id_str.empty()) {
            new_connection.folder_id = selected_folder_id_str;
        } else {
            new_connection.folder_id = ""; // Explicitly set to empty if "None" was chosen
        }

        if (new_connection.connection_type == "SSH") {
            new_connection.auth_method = auth_method_combo.get_active_text();
            if (new_connection.auth_method == "Password") {
                new_connection.password = password_entry.get_text();
                // Clear SSH key fields for hygiene
                new_connection.ssh_key_path = "";
                new_connection.ssh_key_passphrase = "";
            } else if (new_connection.auth_method == "SSHKey") {
                new_connection.ssh_key_path = ssh_key_path_entry.get_text();
                new_connection.ssh_key_passphrase = ssh_key_passphrase_entry.get_text();
                // Clear password field for hygiene
                new_connection.password = "";
            }
            new_connection.additional_ssh_options = ssh_flags_entry.get_text();
        } else {
            // Clear SSH specific fields if not an SSH connection
            new_connection.auth_method = "";
            new_connection.password = "";
            new_connection.ssh_key_path = "";
            new_connection.ssh_key_passphrase = "";
            new_connection.additional_ssh_options = "";
        }

        if (ConnectionManager::save_connection(new_connection)) {
            populate_connections_treeview(connections_liststore, connection_columns, *connections_treeview); // Refresh TreeView
        } else {
            Gtk::MessageDialog err_dialog(dialog, "Failed to save connection.", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
            err_dialog.run();
        }
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
                    Glib::RefPtr<Gtk::TreeStore>& liststore_ref, // Renamed
                    ConnectionColumns& columns_ref, Gtk::Notebook& notebook) { // Added notebook and changed to ConnectionColumns
    // Assign global pointers
    connections_treeview = &connections_treeview_ref;
    connections_liststore = liststore_ref;
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
    add_folder_button->signal_clicked().connect([&parent_window, &connections_treeview_ref, &liststore_ref, &columns_ref]() {
        FolderOps::add_folder(parent_window, connections_treeview_ref, liststore_ref, columns_ref);
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
    edit_folder_button->signal_clicked().connect([&parent_window, &connections_treeview_ref, &liststore_ref, &columns_ref]() {
        FolderOps::edit_folder(parent_window, connections_treeview_ref, liststore_ref, columns_ref);
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
    delete_folder_button->signal_clicked().connect([&parent_window, &connections_treeview_ref, &liststore_ref, &columns_ref]() {
        FolderOps::delete_folder(parent_window, connections_treeview_ref, liststore_ref, columns_ref);
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
    delete_conn_button->signal_clicked().connect([&connections_treeview_ref, &liststore_ref, &columns_ref, &parent_window]() { // Added parent_window capture
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
                            populate_connections_treeview(liststore_ref, columns_ref, connections_treeview_ref);
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
    connections_treeview_ref.set_model(liststore_ref);

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

// Define the TerminalData struct globally so its members are known to the callback
struct TerminalData {
    Gtk::Notebook* notebook;
    int page_num;
    // We might need to add VteTerminal* vte_widget; if we need to interact with it directly in the callback
};

// Callback function for VTE's "child-exited" signal
static void on_terminal_child_exited(GtkWidget* widget, gint status, gpointer user_data) {
    TerminalData* term_data = static_cast<TerminalData*>(user_data);

    // Log the exit status
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        if (exit_code != 0) {
            std::cerr << "  WARNING: Terminal child process exited with non-zero status: " << exit_code << std::endl;
        }
    } else if (WIFSIGNALED(status)) {
        std::cerr << "  ERROR: Terminal child process terminated by signal: " << WTERMSIG(status) << std::endl;
    } else {
        std::cerr << "  INFO: Terminal child process exited with unusual status: " << status << std::endl;
    }

    auto idle_cleanup_callback = [](gpointer data_to_cleanup) -> gboolean {
        TerminalData* td = static_cast<TerminalData*>(data_to_cleanup);
        if (td->notebook && td->notebook->get_n_pages() > td->page_num) {
            Gtk::Widget* page_widget = td->notebook->get_nth_page(td->page_num);
            if (page_widget) { // Check if the page still exists
                td->notebook->remove_page(td->page_num);
            }
        }
        delete td;
        return FALSE;
    };
    g_idle_add(idle_cleanup_callback, term_data);
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

    // Read configuration
    json config = read_config();

    // Create the main window
    Gtk::Window window;
    window.set_title("ngTerm");

    // Set the window size based on the configuration
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
    connections_treeview->signal_row_activated().connect([&notebook](const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn*) {
        if (!connections_liststore) return;
        Gtk::TreeModel::iterator iter = connections_liststore->get_iter(path);
        if (iter) {
            bool is_folder = (*iter)[connection_columns.is_folder];
            if (is_folder) {
                return; // Do nothing if a folder is double-clicked
            }

            bool is_connection = (*iter)[connection_columns.is_connection];
            if (is_connection) {
                Glib::ustring connection_name = static_cast<Glib::ustring>((*iter)[connection_columns.name]);
                Glib::ustring conn_type_ustring = static_cast<Glib::ustring>((*iter)[connection_columns.connection_type]);
                std::string connection_type_str = conn_type_ustring.raw();

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

                std::vector<char*> argv_vec;
                std::vector<std::string> command_args_str;

                if (connection_type_str == "SSH") {
                    ConnectionInfo conn_info;
                    conn_info.id = static_cast<Glib::ustring>((*iter)[connection_columns.id]);
                    conn_info.name = connection_name;
                    conn_info.host = static_cast<Glib::ustring>((*iter)[connection_columns.host]);
                    Glib::ustring port_ustr = (*iter)[connection_columns.port];
                    if (!port_ustr.empty()) {
                        try {
                            conn_info.port = std::stoi(port_ustr.raw());
                        } catch (const std::exception& e) {
                            conn_info.port = 0;
                            std::cerr << "Error converting port for SSH: " << port_ustr << " - " << e.what() << std::endl;
                        }
                    } else {
                        conn_info.port = 0;
                    }
                    conn_info.username = static_cast<Glib::ustring>((*iter)[connection_columns.username]);
                    conn_info.connection_type = connection_type_str;

                    std::vector<std::string> ssh_command_parts = Ssh::generate_ssh_command_args(conn_info);
                    std::string full_ssh_command_str;
                    for (size_t i = 0; i < ssh_command_parts.size(); ++i) {
                        full_ssh_command_str += ssh_command_parts[i];
                        if (i < ssh_command_parts.size() - 1) {
                            full_ssh_command_str += " "; // Add spaces between parts
                        }
                    }

                    full_ssh_command_str += "; read -p \'[ngTerm] SSH session ended. Press any key to close this tab...\' -n 1 -s";

                    command_args_str.clear();
                    command_args_str.push_back("/bin/bash");
                    command_args_str.push_back("-c");
                    command_args_str.push_back(full_ssh_command_str); // Pass the full ssh command as a single quoted argument to -c
                } else { // Non-SSH connection type
                    command_args_str.clear();
                    command_args_str.push_back("/bin/bash");
                }

                for (const std::string& s : command_args_str) {
                    argv_vec.push_back(const_cast<char*>(s.c_str()));
                }
                argv_vec.push_back(nullptr);

                const char** argv = const_cast<const char**>(argv_vec.data());

                GError* error = nullptr;
                GPid child_pid;
                vte_terminal_spawn_sync(VTE_TERMINAL(vte_widget), VTE_PTY_DEFAULT, nullptr, (char**)argv , nullptr, G_SPAWN_DEFAULT, nullptr, nullptr, &child_pid, nullptr, &error);

                if (error) {
                    std::cerr << "Error spawning terminal: " << error->message << std::endl;
                    g_error_free(error);
                    // If spawn fails, remove the tab that was optimistically added
                    if (notebook.get_n_pages() > page_num && notebook.get_nth_page(page_num) == terminal_box) {
                         notebook.remove_page(page_num);
                    }
                    delete data; // Clean up TerminalData
                } else {
                    // vte_terminal_watch_child(VTE_TERMINAL(vte_widget), child_pid); // VTE's own watch
                    // Connect to VTE's child-exited signal using g_signal_connect
                    g_signal_connect(vte_widget, "child-exited", G_CALLBACK(on_terminal_child_exited), data);
                }
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