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
        add(parent_id_col); // Add parent_id_col here
    }

    Gtk::TreeModelColumn<Glib::ustring> name;            // Name of folder or connection
    Gtk::TreeModelColumn<Glib::ustring> id;              // Folder or connection ID
    Gtk::TreeModelColumn<bool> is_folder;                // True if it's a folder
    Gtk::TreeModelColumn<bool> is_connection;            // True if it's a connection
    Gtk::TreeModelColumn<Glib::ustring> host;            // Hostname or IP address
    Gtk::TreeModelColumn<Glib::ustring> port;            // Port number (stored as string)
    Gtk::TreeModelColumn<Glib::ustring> username;        // Username
    Gtk::TreeModelColumn<Glib::ustring> connection_type; // Connection type (e.g., SSH, Telnet)
    Gtk::TreeModelColumn<Glib::ustring> parent_id_col;   // Parent ID (for folders and connections)
};

#endif // TREEMODELCOLUMNS_H
