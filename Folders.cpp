#include "Folders.h"
#include "Connections.h"      // For ConnectionManager and FolderInfo/ConnectionInfo structs
#include "TreeModelColumns.h" // For ConnectionColumns (though passed as Gtk::TreeModel::ColumnRecord)

#include <gtkmm/dialog.h>
#include <gtkmm/grid.h>
#include <gtkmm/label.h>
#include <gtkmm/entry.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/messagedialog.h>
#include <uuid/uuid.h> // For uuid_generate_random, uuid_unparse_lower

namespace FolderOps {

void add_folder(Gtk::Window& parent_window,
                Gtk::TreeView& connections_treeview,
                Glib::RefPtr<Gtk::TreeStore>& connections_liststore,
                ConnectionColumns& columns) {

    Gtk::Dialog dialog("Add New Folder", parent_window, true /* modal */);
    dialog.set_default_size(350, 200);

    Gtk::Grid* grid = Gtk::manage(new Gtk::Grid());
    grid->set_border_width(10);
    grid->set_column_spacing(10);
    grid->set_row_spacing(10);
    dialog.get_content_area()->pack_start(*grid, Gtk::PACK_EXPAND_WIDGET);

    // Folder Name
    Gtk::Label name_label("Folder Name:");
    name_label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry name_entry;
    name_entry.set_hexpand(true);

    // Parent Folder
    Gtk::Label parent_label("Parent Folder:");
    parent_label.set_halign(Gtk::ALIGN_START);
    Gtk::ComboBoxText parent_combo;
    parent_combo.set_hexpand(true);
    parent_combo.append("", "(Root Level)"); // ID, Text. Empty ID for root.

    // Populate parent_combo with existing folders
    std::vector<FolderInfo> existing_folders = ConnectionManager::load_folders();
    for (const auto& folder : existing_folders) {
        parent_combo.append(folder.id, folder.name);
    }
    parent_combo.set_active(0); // Default to (Root Level)

    // Attach to grid
    grid->attach(name_label,   0, 0, 1, 1);
    grid->attach(name_entry,   1, 0, 1, 1);
    grid->attach(parent_label, 0, 1, 1, 1);
    grid->attach(parent_combo, 1, 1, 1, 1);

    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Save", Gtk::RESPONSE_OK);
    dialog.show_all();

    int response = dialog.run();
    if (response == Gtk::RESPONSE_OK) {
        std::string folder_name = name_entry.get_text();
        std::string parent_id = parent_combo.get_active_id();

        if (folder_name.empty()) {
            Gtk::MessageDialog warning_dialog(parent_window, "Folder Name Empty", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true);
            warning_dialog.set_secondary_text("Folder name cannot be empty.");
            warning_dialog.run();
            return;
        }

        FolderInfo new_folder;
        uuid_t uuid; // libuuid type
        uuid_generate_random(uuid); // Generate a random UUID
        char uuid_str[37]; // Buffer for string representation (36 chars + null)
        uuid_unparse_lower(uuid, uuid_str); // Convert to string
        new_folder.id = uuid_str; // Assign to FolderInfo

        new_folder.name = folder_name;
        new_folder.parent_id = parent_id;

        if (ConnectionManager::save_folder(new_folder)) {
            // Note: populate_connections_treeview is forward-declared in Folders.h
            // Its definition is expected to be in main.cpp or another linked file.
            populate_connections_treeview(connections_liststore, columns, connections_treeview);
        } else {
            Gtk::MessageDialog error_dialog(parent_window, "Save Failed", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
            error_dialog.set_secondary_text("Could not save the new folder.");
            error_dialog.run();
        }
    }
}

void edit_folder(Gtk::Window& parent_window,
                 Gtk::TreeView& connections_treeview,
                 Glib::RefPtr<Gtk::TreeStore>& connections_liststore,
                 ConnectionColumns& columns) {

    Glib::RefPtr<Gtk::TreeSelection> selection = connections_treeview.get_selection();
    Gtk::TreeModel::iterator iter = selection->get_selected();

    if (!iter) {
        Gtk::MessageDialog warning_dialog(parent_window, "No Folder Selected", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true);
        warning_dialog.set_secondary_text("Please select a folder to edit.");
        warning_dialog.run();
        return;
    }

    Gtk::TreeModel::Row row = *iter;
    if (!row[columns.is_folder]) {
        Gtk::MessageDialog warning_dialog(parent_window, "Not a Folder", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true);
        warning_dialog.set_secondary_text("The selected item is not a folder.");
        warning_dialog.run();
        return;
    }

    FolderInfo current_folder;
    current_folder.id = static_cast<Glib::ustring>(row[columns.id]);
    current_folder.name = static_cast<Glib::ustring>(row[columns.name]);
    current_folder.parent_id = static_cast<Glib::ustring>(row[columns.parent_id_col]);

    Gtk::Dialog dialog("Edit Folder", parent_window, true /* modal */);
    dialog.set_default_size(350, 200);

    Gtk::Grid* grid = Gtk::manage(new Gtk::Grid());
    grid->set_border_width(10);
    grid->set_column_spacing(10);
    grid->set_row_spacing(10);
    dialog.get_content_area()->pack_start(*grid, Gtk::PACK_EXPAND_WIDGET);

    // Folder Name
    Gtk::Label name_label("Folder Name:");
    name_label.set_halign(Gtk::ALIGN_START);
    Gtk::Entry name_entry;
    name_entry.set_hexpand(true);
    name_entry.set_text(current_folder.name); // Pre-fill current name

    // Parent Folder selection
    Gtk::Label parent_folder_label("Parent Folder:");
    parent_folder_label.set_halign(Gtk::ALIGN_START);
    Gtk::ComboBoxText parent_folder_combo;
    parent_folder_combo.set_hexpand(true);
    std::vector<FolderInfo> all_folders = ConnectionManager::load_folders();
    parent_folder_combo.append("", "None (Root Level)"); // ID, Text for root

    int active_idx = 0; // Default to "None (Root Level)"
    int current_combo_idx = 0;

    // Add "None (Root Level)"
    if (current_folder.parent_id.empty()) {
        active_idx = current_combo_idx;
    }
    current_combo_idx++;

    for (const auto& folder_item : all_folders) {
        if (folder_item.id == current_folder.id) {
            continue; // Don't allow a folder to be its own parent in the list
        }
        parent_folder_combo.append(folder_item.id, folder_item.name);
        if (folder_item.id == current_folder.parent_id) {
            active_idx = current_combo_idx;
        }
        current_combo_idx++;
    }
    parent_folder_combo.set_active(active_idx);

    grid->attach(name_label, 0, 0, 1, 1);
    grid->attach(name_entry, 1, 0, 1, 1);
    grid->attach(parent_folder_label, 0, 1, 1, 1);
    grid->attach(parent_folder_combo, 1, 1, 1, 1);

    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    Gtk::Button* save_button = dialog.add_button("_Save", Gtk::RESPONSE_OK);
    save_button->get_style_context()->add_class("suggested-action");

    dialog.show_all();
    int response = dialog.run();

    if (response == Gtk::RESPONSE_OK) {
        std::string new_folder_name = name_entry.get_text();
        if (new_folder_name.empty()) {
            Gtk::MessageDialog warning_dialog(parent_window, "Folder Name Empty", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true);
            warning_dialog.set_secondary_text("Folder name cannot be empty.");
            warning_dialog.run();
            return; // Or re-show dialog, but for now just return
        }

        FolderInfo updated_folder = current_folder; // Keep original ID
        updated_folder.name = new_folder_name;
        updated_folder.parent_id = parent_folder_combo.get_active_id();

        if (updated_folder.id == updated_folder.parent_id) {
            Gtk::MessageDialog error_dialog(parent_window, "Invalid Parent", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
            error_dialog.set_secondary_text("A folder cannot be its own parent.");
            error_dialog.run();
            return; // Or re-show dialog
        }

        // More sophisticated cycle detection would be needed for a full solution
        // (e.g., checking if new parent is a descendant of the current folder).
        // For now, only direct self-parenting is checked.

        if (ConnectionManager::save_folder(updated_folder)) {
            populate_connections_treeview(connections_liststore, columns, connections_treeview);
        } else {
            Gtk::MessageDialog error_dialog(parent_window, "Save Failed", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
            error_dialog.set_secondary_text("Could not update the folder.");
            error_dialog.run();
        }
    }
}

void delete_folder(Gtk::Window& parent_window,
                   Gtk::TreeView& connections_treeview,
                   Glib::RefPtr<Gtk::TreeStore>& connections_liststore,
                   ConnectionColumns& columns) {
    Glib::RefPtr<Gtk::TreeSelection> selection = connections_treeview.get_selection();
    Gtk::TreeModel::iterator iter = selection->get_selected();

    if (!iter) {
        Gtk::MessageDialog warning_dialog(parent_window, "No Folder Selected", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true);
        warning_dialog.set_secondary_text("Please select a folder to delete.");
        warning_dialog.run();
        return;
    }

    Gtk::TreeModel::Row row = *iter;
    if (!row[columns.is_folder]) {
        Gtk::MessageDialog warning_dialog(parent_window, "Not a Folder", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true);
        warning_dialog.set_secondary_text("The selected item is not a folder. Please select a folder to delete.");
        warning_dialog.run();
        return;
    }

    std::string folder_id = static_cast<Glib::ustring>(row[columns.id]);
    std::string folder_name = static_cast<Glib::ustring>(row[columns.name]);

    Gtk::MessageDialog confirmation_dialog(parent_window, "Delete Folder", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);
    confirmation_dialog.set_secondary_text("Are you sure you want to delete the folder '" + folder_name + "' and all its contents (sub-folders and connections)? This action cannot be undone.");

    if (confirmation_dialog.run() == Gtk::RESPONSE_YES) {
        // Call the public ConnectionManager::delete_folder, which handles recursion and saving.
        if (ConnectionManager::delete_folder(folder_id)) {
            populate_connections_treeview(connections_liststore, columns, connections_treeview);
        } else {
            Gtk::MessageDialog error_dialog(parent_window, "Error Deleting Folder", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
            error_dialog.set_secondary_text("Could not delete the folder '" + folder_name + "'. Check logs for details.");
            error_dialog.run();
        }
    }
}

} // namespace FolderOps