#ifndef MAIN_H
#define MAIN_H

#include <gtkmm.h>
#include <vte/vte.h>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/frame.h>
#include <gtkmm/grid.h>
#include <gtkmm/box.h>
#include <gtkmm/toolbar.h>
#include <gtkmm/toolbutton.h>
#include <gdkmm/pixbuf.h>
#include "TreeModelColumns.h"
#include "Connections.h"
#include "Folders.h"
#include "Ssh.h"
#include "Config.h"
#include <sys/wait.h>
#include <map>
#include <gdkmm/pixbufloader.h>

using json = nlohmann::json;

// Define an enum for the process_connection_dialog purpose
enum class DialogPurpose {
    ADD,
    EDIT,
    DUPLICATE
};

// Forward declarations
struct TerminalData;

// Global variables (declaration only)
extern Gtk::TreeView* connections_treeview;
extern Glib::RefPtr<Gtk::TreeStore> connections_liststore;
extern ConnectionColumns connection_columns;

// Track open connections and their tab indices
extern std::map<std::string, int> open_connections; // Maps connection ID to tab index

// Global UI elements for Info Panel
extern Gtk::Frame* info_frame;
extern Gtk::Grid* info_grid;
extern Gtk::Label* host_value_label;
extern Gtk::Label* type_value_label;
extern Gtk::Label* port_value_label;

// Global MenuItems for Edit functionality
extern Gtk::MenuItem* edit_folder_menu_item;
extern Gtk::MenuItem* edit_connection_menu_item;
extern Gtk::MenuItem* duplicate_connection_item;

// Global ToolButtons for Edit/Delete functionality on the toolbar
extern Gtk::ToolButton* edit_folder_menu_item_toolbar;
extern Gtk::ToolButton* edit_connection_menu_item_toolbar;
extern Gtk::ToolButton* duplicate_connection_menu_item_toolbar;
extern Gtk::ToolButton* delete_folder_menu_item_toolbar;
extern Gtk::ToolButton* delete_connection_menu_item_toolbar;
extern Gtk::ToolButton* add_folder_menu_item_toolbar;
extern Gtk::ToolButton* add_connection_menu_item_toolbar;

// Function declarations
void on_connection_selection_changed();
void process_connection_dialog(Gtk::Notebook& notebook, DialogPurpose purpose, const ConnectionInfo* existing_connection);
void duplicate_connection_dialog(Gtk::Notebook& notebook);
void edit_connection_dialog(Gtk::Notebook& notebook);
void add_connection_dialog(Gtk::Notebook& notebook);
void delete_connection_dialog(Gtk::Notebook& notebook, const Glib::ustring& conn_id, const Glib::ustring& conn_name);
void build_menu(Gtk::Window& parent_window, Gtk::MenuBar& menubar, Gtk::Notebook& notebook, Gtk::TreeView& connections_treeview_ref,
                Glib::RefPtr<Gtk::TreeStore>& liststore_ref, ConnectionColumns& columns_ref);
void build_leftFrame(Gtk::Window& parent_window, Gtk::Frame& left_frame, Gtk::ScrolledWindow& left_scrolled_window,
                    Gtk::TreeView& connections_treeview_ref,
                    Glib::RefPtr<Gtk::TreeStore>& liststore_ref,
                    ConnectionColumns& columns_ref, Gtk::Notebook& notebook);
void build_rightFrame(Gtk::Notebook& notebook);
void populate_connections_treeview(Glib::RefPtr<Gtk::TreeStore>& liststore, ConnectionColumns& cols, Gtk::TreeView& treeview);

// C-style callback for key press event
gboolean on_terminal_key_press(GtkWidget* widget, GdkEventKey* event, gpointer user_data);

// Callback function for VTE's "child-exited" signal
void on_terminal_child_exited(GtkWidget* widget, gint status, gpointer user_data);

#endif // MAIN_H
