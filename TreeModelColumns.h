#ifndef TREEMODELCOLUMNS_H
#define TREEMODELCOLUMNS_H

#include <gtkmm/treemodelcolumn.h>
#include <glibmm/ustring.h>

// Define custom columns for the TreeView
struct ConnectionColumns : public Gtk::TreeModelColumnRecord {
    ConnectionColumns() {
        add(name);
        add(id);
        add(is_folder);
        add(is_connection);
        add(host);
        add(port);
        add(username);
        add(connection_type);
        add(parent_id_col);
        add(auth_method);
        add(password);
        add(ssh_key_path);
        add(ssh_key_passphrase);
        add(additional_ssh_options);
    }

    Gtk::TreeModelColumn<Glib::ustring> name;            // Name of folder or connection
    Gtk::TreeModelColumn<Glib::ustring> id;              // Folder or connection ID
    Gtk::TreeModelColumn<bool> is_folder;                // True if it's a folder
    Gtk::TreeModelColumn<bool> is_connection;            // True if it's a connection
    Gtk::TreeModelColumn<Glib::ustring> host;            // Hostname or IP address
    Gtk::TreeModelColumn<int> port;                      // Port number
    Gtk::TreeModelColumn<Glib::ustring> username;        // Username
    Gtk::TreeModelColumn<Glib::ustring> connection_type; // Connection type (e.g., SSH, Telnet)
    Gtk::TreeModelColumn<Glib::ustring> parent_id_col;   // Parent ID (for folders and connections)
    Gtk::TreeModelColumn<Glib::ustring> auth_method;     // Authentication method (Password or SSHKey)
    Gtk::TreeModelColumn<Glib::ustring> password;        // SSH password (if using password auth)
    Gtk::TreeModelColumn<Glib::ustring> ssh_key_path;    // Path to SSH key file
    Gtk::TreeModelColumn<Glib::ustring> ssh_key_passphrase; // SSH key passphrase
    Gtk::TreeModelColumn<Glib::ustring> additional_ssh_options; // Additional SSH options
};

#endif // TREEMODELCOLUMNS_H
