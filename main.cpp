#include <gtkmm.h>
#include <vte/vte.h> // Include the VTE header
#include <glib.h>    // Include glib.h for G_OBJECT, GError, g_strsplit, g_strfreev, gboolean

// Define a Column Record to hold the data for the TreeView
class ConnectionColumns : public Gtk::TreeModel::ColumnRecord {
public:
    ConnectionColumns() {
        add(name);
    }
    Gtk::TreeModelColumn<Glib::ustring> name; // Column to hold connection names (as strings)
};

int main(int argc, char* argv[]) { // function parameter argv
    // Initialize the GTK+ application
    auto app = Gtk::Application::create(argc, argv, "org.yourdomain.ngterm"); // Use the application ID

    // Create the main window
    Gtk::Window window;
    window.set_title("ngTerm"); // Set the window title
    window.set_default_size(800, 600); // Set a default size

    // Create a vertical box to hold the toolbar and the main content
    Gtk::VBox main_vbox;
    window.add(main_vbox); // Add the main_vbox to the window

    // Create a horizontal paned widget for the left and right frames
    Gtk::HPaned main_hpaned;
    main_vbox.pack_start(main_hpaned, true, true, 0); // Pack paned widget below toolbar, expanding

    // --- Left Pane: Frame with TreeView ---

    // Create a Frame for the left pane with a title
    Gtk::Frame left_frame;
    left_frame.set_label("Connections");
    // Set a size request on the frame to influence the initial pane size
    left_frame.set_size_request(250, -1);
    left_frame.set_border_width(5); // Add a little padding around the content

    // Create a ScrolledWindow to hold the TreeView
    Gtk::ScrolledWindow left_scrolled_window;
    // Add the scrolled window to the frame
    left_frame.add(left_scrolled_window);

    // Create the TreeView
    Gtk::TreeView connections_treeview;
    // Set the TreeView to fill the scrolled window space
    connections_treeview.set_hexpand(true);
    connections_treeview.set_vexpand(true);

    // Create a ListStore to hold the data for the TreeView
    ConnectionColumns columns; // Instantiate the column record
    Glib::RefPtr<Gtk::ListStore> connections_liststore = Gtk::ListStore::create(columns);
    connections_treeview.set_model(connections_liststore); // Set the model for the TreeView

    // Add columns to the TreeView
    connections_treeview.append_column("Name", columns.name); // Display the 'name' column

    // Add rows to the ListStore
    Gtk::TreeModel::Row row = *(connections_liststore->append()); // Append a new row
    row[columns.name] = "connection1"; // Set the data for the 'name' column in this row

    row = *(connections_liststore->append()); // Append another new row
    row[columns.name] = "connection2"; // Set the data for the 'name' column in this row

    // Add the TreeView to the ScrolledWindow
    left_scrolled_window.add(connections_treeview);

    // Add the Frame to the left side of the Paned widget
    main_hpaned.pack1(left_frame, false, false);


    // --- Right Pane: Terminal Widget ---

    // Create a ScrolledWindow for the terminal
    Gtk::ScrolledWindow right_scrolled_window;
    // The terminal should expand and fill the available space
    right_scrolled_window.set_hexpand(true);
    right_scrolled_window.set_vexpand(true);

    // Create the VTE Terminal widget (using the C API function)
    GtkWidget *vte_widget = vte_terminal_new(); // Get as a generic GtkWidget*

    // Add the widget to the scrolled window
    // Wrap the GtkWidget* using Glib::wrap, which is designed to handle GtkWidget*
    // Pass true as the second argument to indicate ownership transfer
    right_scrolled_window.add(*Glib::wrap(vte_widget, true));

    // Now, cast the GtkWidget* to VteTerminal* for VTE-specific functions
    // Use the VTE_TERMINAL macro for casting
    VteTerminal *terminal = VTE_TERMINAL(vte_widget);


    // Spawn a shell inside the terminal using the previously working function
    GError *error = nullptr;
    gchar **shell_args = g_strsplit("/bin/bash", " ", 0); // Command to run

    // Using the deprecated but available function vte_terminal_spawn_sync
    gboolean success = vte_terminal_spawn_sync(
        terminal,            // 1: VteTerminal*
        VTE_PTY_DEFAULT,     // 2: VtePtyFlags
        nullptr,             // 3: const char* working_directory
        shell_args,          // 4: char** argv (using the new name)
        nullptr,             // 5: char** envv
        G_SPAWN_DEFAULT,     // 6: GSpawnFlags
        nullptr,             // 7: GSpawnChildSetupFunc
        nullptr,             // 8: gpointer child_setup_data
        nullptr,             // 9: GPid* child_pid
        nullptr,             // 10: GCancellable* cancellable
        &error               // 11: GError** error
    );
     g_strfreev(shell_args); // Free the argument array

    if (!success) { // Check the boolean return value for success
        g_warning("Failed to spawn terminal: %s", error->message);
        g_error_free(error);
    }


    // Add the scrolled window (containing the terminal) to the right pane
    main_hpaned.pack2(right_scrolled_window, true, true); // pack2 is for the right side, expanding

    // Set the initial position of the paned widget
    // The position is measured from the left edge of the widget
    main_hpaned.set_position(250); // Set the divider position to 250 pixels

    // Show all widgets
    window.show_all_children(); // Show widgets packed inside the window
    window.show(); // Show the window itself

    // Start the GTK+ main loop
    return app->run(window);
}