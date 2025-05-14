#include "Connections.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <uuid/uuid.h>
#include <algorithm>
#include <nlohmann/json.hpp>

std::filesystem::path ConnectionManager::get_connections_dir() {
    const char* home_dir = std::getenv("HOME");
    std::filesystem::path config_dir = std::filesystem::path(home_dir) / ".config" / "ngTerm" / "connections";
    std::filesystem::create_directories(config_dir);
    return config_dir;
}

std::filesystem::path ConnectionManager::get_connections_file() {
    return get_connections_dir() / "connections.json";
}

std::filesystem::path ConnectionManager::get_folders_file() {
    return get_connections_dir() / "folders.json";
}

void ConnectionManager::ensure_parent_directory_exists(const std::filesystem::path& file_path) {
    if (!file_path.has_parent_path()) {
        return;
    }
    std::filesystem::path parent_dir = file_path.parent_path();
    if (!std::filesystem::exists(parent_dir)) {
        std::filesystem::create_directories(parent_dir);
    }
}

Glib::ustring ConnectionManager::generate_connection_id() {
    uuid_t uuid;
    uuid_generate_random(uuid);
    char uuid_str[37];
    uuid_unparse_lower(uuid, uuid_str);
    return Glib::ustring(uuid_str);
}

Glib::ustring ConnectionManager::generate_folder_id() {
    uuid_t uuid;
    uuid_generate_random(uuid);
    char uuid_str[37];
    uuid_unparse_lower(uuid, uuid_str);
    return Glib::ustring(uuid_str);
}

bool ConnectionManager::save_connection(const ConnectionInfo& connection) {
    std::vector<ConnectionInfo> connections = load_connections();
    std::filesystem::path file_path = get_connections_file();
    std::filesystem::create_directories(file_path.parent_path());

    json json_array = json::array();
    bool found = false;
    for (const auto& conn : connections) {
        if (conn.id == connection.id) {
            // Update existing connection
            json json_conn;
            json_conn["id"] = connection.id.raw();
            json_conn["name"] = connection.name.raw();
            json_conn["host"] = connection.host.raw();
            json_conn["port"] = connection.port;
            json_conn["username"] = connection.username.raw();
            json_conn["connection_type"] = connection.connection_type.raw();
            json_conn["folder_id"] = connection.folder_id.raw();
            if (connection.connection_type == "SSH") {
                json_conn["auth_method"] = connection.auth_method.raw();
                json_conn["password"] = connection.password.raw();
                json_conn["ssh_key_path"] = connection.ssh_key_path.raw();
                json_conn["ssh_key_passphrase"] = connection.ssh_key_passphrase.raw();
                json_conn["additional_ssh_options"] = connection.additional_ssh_options.raw();
            }
            json_array.push_back(json_conn);
            found = true;
        } else {
            // Add other connections back
            json json_other_conn;
            json_other_conn["id"] = conn.id.raw();
            json_other_conn["name"] = conn.name.raw();
            json_other_conn["host"] = conn.host.raw();
            json_other_conn["port"] = conn.port;
            json_other_conn["username"] = conn.username.raw();
            json_other_conn["connection_type"] = conn.connection_type.raw();
            json_other_conn["folder_id"] = conn.folder_id.raw();
            if (conn.connection_type == "SSH") {
                json_other_conn["auth_method"] = conn.auth_method.raw();
                json_other_conn["password"] = conn.password.raw();
                json_other_conn["ssh_key_path"] = conn.ssh_key_path.raw();
                json_other_conn["ssh_key_passphrase"] = conn.ssh_key_passphrase.raw();
                json_other_conn["additional_ssh_options"] = conn.additional_ssh_options.raw();
            }
            json_array.push_back(json_other_conn);
        }
    }

    if (!found) {
        // Add new connection
        json json_conn;
        json_conn["id"] = connection.id.raw();
        json_conn["name"] = connection.name.raw();
        json_conn["host"] = connection.host.raw();
        json_conn["port"] = connection.port;
        json_conn["username"] = connection.username.raw();
        json_conn["connection_type"] = connection.connection_type.raw();
        json_conn["folder_id"] = connection.folder_id.raw();
        if (connection.connection_type == "SSH") {
            json_conn["auth_method"] = connection.auth_method.raw();
            json_conn["password"] = connection.password.raw();
            json_conn["ssh_key_path"] = connection.ssh_key_path.raw();
            json_conn["ssh_key_passphrase"] = connection.ssh_key_passphrase.raw();
            json_conn["additional_ssh_options"] = connection.additional_ssh_options.raw();
        }
        json_array.push_back(json_conn);
    }

    std::ofstream file(file_path);
    if (file.is_open()) {
        file << json_array.dump(4);
        file.close();
        return true;
    }
    return false;
}

bool ConnectionManager::save_folder(const FolderInfo& folder) {
    try {
        std::vector<FolderInfo> folders = load_folders();

        // Check if folder with same ID exists
        auto it = std::find_if(folders.begin(), folders.end(),
            [&folder](const FolderInfo& f) { return f.id == folder.id; });

        if (it != folders.end()) {
            // Replace existing folder
            *it = folder;
        } else {
            // Add the new folder to the list
            folders.push_back(folder);
        }

        // Write the updated list back to the file
        std::filesystem::path file_path = get_folders_file();
        ConnectionManager::ensure_parent_directory_exists(file_path);
        json j_folders = json::array();
        for (const auto& f : folders) {
            j_folders.push_back({
                {"id", f.id.raw()},
                {"name", f.name.raw()},
                {"parent_id", f.parent_id.raw()}
            });
        }

        std::ofstream file(file_path);
        if (file.is_open()) {
            file << j_folders.dump(4);
            file.close();
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Error saving folder: " << e.what() << std::endl;
        return false;
    }
}

std::vector<ConnectionInfo> ConnectionManager::load_connections() {
    std::vector<ConnectionInfo> connections;
    std::filesystem::path file_path = get_connections_file();

    if (!std::filesystem::exists(file_path)) {
        return connections; // Return empty if file doesn't exist
    }

    std::ifstream file(file_path);
    if (file.is_open()) {
        json json_array;
        try {
            file >> json_array;
            if (json_array.is_array()) {
                for (const auto& j_conn : json_array) {
                    ConnectionInfo conn;
                    conn.id = Glib::ustring(j_conn.value("id", ""));
                    conn.name = Glib::ustring(j_conn.value("name", ""));
                    conn.host = Glib::ustring(j_conn.value("host", ""));
                    conn.port = j_conn.value("port", 0);
                    conn.username = Glib::ustring(j_conn.value("username", ""));
                    conn.connection_type = Glib::ustring(j_conn.value("connection_type", ""));
                    conn.folder_id = Glib::ustring(j_conn.value("folder_id", ""));
                    if (conn.connection_type == "SSH") {
                        conn.auth_method = Glib::ustring(j_conn.value("auth_method", ""));
                        conn.password = Glib::ustring(j_conn.value("password", ""));
                        conn.ssh_key_path = Glib::ustring(j_conn.value("ssh_key_path", ""));
                        conn.ssh_key_passphrase = Glib::ustring(j_conn.value("ssh_key_passphrase", ""));
                        conn.additional_ssh_options = Glib::ustring(j_conn.value("additional_ssh_options", ""));
                    }
                    connections.push_back(conn);
                }
            }
        } catch (const json::parse_error& e) {
            std::cerr << "Error parsing connections.json: " << e.what() << std::endl;
            // Optionally, handle corrupt file (e.g., backup and create new)
        }
        file.close();
    }
    return connections;
}

std::vector<FolderInfo> ConnectionManager::load_folders() {
    std::vector<FolderInfo> folders;
    std::filesystem::path file_path = get_folders_file();

    if (!std::filesystem::exists(file_path)) {
        return folders;
    }

    std::ifstream file(file_path);
    try {
        json j_folders;
        file >> j_folders;

        for (const auto& j_folder : j_folders) {
            FolderInfo folder;
            folder.id = Glib::ustring(j_folder.value("id", ""));
            folder.name = Glib::ustring(j_folder.value("name", ""));
            folder.parent_id = Glib::ustring(j_folder.value("parent_id", ""));
            folders.push_back(folder);
        }
    } catch (const json::parse_error& e) {
        std::cerr << "Error parsing folders.json: " << e.what() << std::endl;
    }
    return folders;
}

bool ConnectionManager::delete_connection(const Glib::ustring& connection_id) {
    try {
        std::vector<ConnectionInfo> connections = load_connections();

        connections.erase(
            std::remove_if(connections.begin(), connections.end(),
                [&connection_id](const ConnectionInfo& conn) {
                    return conn.id == connection_id;
                }),
            connections.end());

        // Save the modified list back to the file
        std::filesystem::path file_path = get_connections_file();
        json j_connections = json::array();
        for (const auto& conn : connections) {
            json json_conn;
            json_conn["id"] = conn.id.raw();
            json_conn["name"] = conn.name.raw();
            json_conn["host"] = conn.host.raw();
            json_conn["port"] = conn.port;
            json_conn["username"] = conn.username.raw();
            json_conn["connection_type"] = conn.connection_type.raw();
            json_conn["folder_id"] = conn.folder_id.raw();

            if (conn.connection_type == "SSH") {
                json_conn["auth_method"] = conn.auth_method.raw();
                json_conn["password"] = conn.password.raw();
                json_conn["ssh_key_path"] = conn.ssh_key_path.raw();
                json_conn["ssh_key_passphrase"] = conn.ssh_key_passphrase.raw();
                json_conn["additional_ssh_options"] = conn.additional_ssh_options.raw();
            }
            j_connections.push_back(json_conn);
        }

        std::ofstream file(file_path);
        if (file.is_open()) {
            file << j_connections.dump(4);
            file.close();
            return true;
        }
        return false; // Failed to save after deletion
    } catch (const std::exception& e) {
        std::cerr << "Error deleting connection: " << e.what() << std::endl;
        return false;
    }
}

void ConnectionManager::delete_folder_recursive(const Glib::ustring& folder_id_to_delete, std::vector<FolderInfo>& all_folders, std::vector<ConnectionInfo>& all_connections) {
    // Find sub-folders of the folder to delete
    std::vector<Glib::ustring> sub_folder_ids;
    for (const auto& folder : all_folders) {
        if (folder.parent_id == folder_id_to_delete) {
            sub_folder_ids.push_back(folder.id);
        }
    }

    // Recursively delete sub-folders
    for (const auto& sub_folder_id : sub_folder_ids) {
        delete_folder_recursive(sub_folder_id, all_folders, all_connections);
    }

    // Delete connections within the current folder_id_to_delete
    all_connections.erase(
        std::remove_if(all_connections.begin(), all_connections.end(),
            [&folder_id_to_delete](const ConnectionInfo& conn) {
                return conn.folder_id == folder_id_to_delete;
            }),
        all_connections.end());

    // Remove the folder itself from all_folders
    all_folders.erase(
        std::remove_if(all_folders.begin(), all_folders.end(),
            [&folder_id_to_delete](const FolderInfo& folder) {
                return folder.id == folder_id_to_delete;
            }),
        all_folders.end());
}

bool ConnectionManager::delete_folder(const Glib::ustring& folder_id) {
    try {
        std::vector<FolderInfo> all_folders = load_folders();
        std::vector<ConnectionInfo> all_connections = load_connections();

        size_t initial_folders_size = all_folders.size();

        delete_folder_recursive(folder_id, all_folders, all_connections);

        if (all_folders.size() < initial_folders_size) { // Check if any folder was actually removed
            // Save the modified folders list
            std::filesystem::path folders_file_path = get_folders_file();
            json folders_json_array = json::array();
            for (const auto& folder : all_folders) {
                folders_json_array.push_back({
                    {"id", folder.id.raw()},
                    {"name", folder.name.raw()},
                    {"parent_id", folder.parent_id.raw()}
                });
            }
            std::ofstream folders_file(folders_file_path);
            if (!folders_file.is_open()) return false; // Failed to open folders file
            folders_file << folders_json_array.dump(4);
            folders_file.close();

            // Save the modified connections list (as connections might have been deleted)
            std::filesystem::path connections_file_path = get_connections_file();
            json connections_json_array = json::array();
            for (const auto& conn : all_connections) {
                json json_conn;
                json_conn["id"] = conn.id.raw();
                json_conn["name"] = conn.name.raw();
                json_conn["host"] = conn.host.raw();
                json_conn["port"] = conn.port;
                json_conn["username"] = conn.username.raw();
                json_conn["connection_type"] = conn.connection_type.raw();
                json_conn["folder_id"] = conn.folder_id.raw();

                if (conn.connection_type == "SSH") {
                    json_conn["auth_method"] = conn.auth_method.raw();
                    json_conn["password"] = conn.password.raw();
                    json_conn["ssh_key_path"] = conn.ssh_key_path.raw();
                    json_conn["ssh_key_passphrase"] = conn.ssh_key_passphrase.raw();
                    json_conn["additional_ssh_options"] = conn.additional_ssh_options.raw();
                }
                connections_json_array.push_back(json_conn);
            }
            std::ofstream connections_file(connections_file_path);
            if (!connections_file.is_open()) return false; // Failed to open connections file
            connections_file << connections_json_array.dump(4);
            connections_file.close();

            return true;
        }
        return false; // Folder not found or no change made
    } catch (const std::exception& e) {
        std::cerr << "Error deleting folder and its contents: " << e.what() << std::endl;
        return false;
    }
}