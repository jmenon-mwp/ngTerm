#ifndef FOLDERS_H
#define FOLDERS_H

#include <gtkmm/window.h>
#include <gtkmm/treeview.h>
#include <gtkmm/treestore.h>
#include <glibmm/refptr.h>

#include "TreeModelColumns.h"

// Forward declaration for populate_connections_treeview.
// This function is currently defined in main.cpp. For Folders.cpp to call it,
// its declaration must be visible, and it must be linked correctly.
void populate_connections_treeview(Glib::RefPtr<Gtk::TreeStore>& liststore,
                                   ConnectionColumns& cols,
                                   Gtk::TreeView& treeview);

namespace FolderOps {

// Functions to handle folder operations.
// These will be called from main.cpp (e.g., from menu actions or toolbar buttons).

void add_folder(Gtk::Window& parent_window,
                Gtk::TreeView& connections_treeview,
                Glib::RefPtr<Gtk::TreeStore>& connections_liststore,
                ConnectionColumns& columns);

void edit_folder(Gtk::Window& parent_window,
                 Gtk::TreeView& connections_treeview,
                 Glib::RefPtr<Gtk::TreeStore>& connections_liststore,
                 ConnectionColumns& columns);

void delete_folder(Gtk::Window& parent_window,
                   Gtk::TreeView& connections_treeview,
                   Glib::RefPtr<Gtk::TreeStore>& connections_liststore,
                   ConnectionColumns& columns);

} // namespace FolderOps

#endif // FOLDERS_H