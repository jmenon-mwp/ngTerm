#include "Connections.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
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

std::string ConnectionManager::generate_connection_id() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    return "conn_" + std::to_string(dis(gen));
}

std::string ConnectionManager::generate_folder_id() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    return "folder_" + std::to_string(dis(gen));
}

bool ConnectionManager::save_connection(const ConnectionInfo& connection) {
    try {
        std::vector<ConnectionInfo> connections = load_connections();

        // Check if connection with same ID exists
        auto it = std::find_if(connections.begin(), connections.end(),
            [&connection](const ConnectionInfo& c) { return c.id == connection.id; });

        if (it != connections.end()) {
            // Replace existing connection
            *it = connection;
        } else {
            // Add new connection
            connections.push_back(connection);
        }

        // Write back to file
        json j_connections = json::array();
        for (const auto& conn : connections) {
            j_connections.push_back({
                {"id", conn.id},
                {"name", conn.name},
                {"host", conn.host},
                {"port", conn.port},
                {"username", conn.username},
                {"folder_id", conn.folder_id},
                {"connection_type", conn.connection_type} // Save connection type
            });
        }

        std::ofstream file(get_connections_file());
        file << j_connections.dump(4);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving connection: " << e.what() << std::endl;
        return false;
    }
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
        json j_folders = json::array();
        for (const auto& f : folders) {
            json j_folder;
            j_folder["id"] = f.id;
            j_folder["name"] = f.name;
            j_folder["parent_id"] = f.parent_id; // Save parent_id
            j_folders.push_back(j_folder);
        }

        std::ofstream file(get_folders_file());
        file << j_folders.dump(4);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving folder: " << e.what() << std::endl;
        return false;
    }
}

std::vector<ConnectionInfo> ConnectionManager::load_connections() {
    std::vector<ConnectionInfo> connections;
    std::filesystem::path connections_file = get_connections_file(); // Use the helper function

    if (!std::filesystem::exists(connections_file)) {
        return connections;
    }

    std::ifstream file(connections_file);
    try {
        json j_connections;
        file >> j_connections;

        for (const auto& j_conn : j_connections) {
            ConnectionInfo conn;
            conn.id = j_conn["id"];
            conn.name = j_conn["name"];
            conn.host = j_conn.value("host", "");
            conn.port = j_conn.value("port", 22);
            conn.username = j_conn.value("username", "");
            conn.folder_id = j_conn.value("folder_id", "");
            conn.connection_type = j_conn.value("connection_type", "SSH"); // Load connection type, default to SSH
            connections.push_back(conn);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error loading connections: " << e.what() << std::endl;
    }
    return connections;
}

std::vector<FolderInfo> ConnectionManager::load_folders() {
    std::vector<FolderInfo> folders;
    std::filesystem::path folders_file = get_folders_file(); // Use the helper function

    if (!std::filesystem::exists(folders_file)) {
        return folders;
    }

    std::ifstream file(folders_file);
    try {
        json j_folders;
        file >> j_folders;

        for (const auto& item : j_folders) {
            FolderInfo folder;
            folder.id = item.value("id", "");
            folder.name = item.value("name", "");
            folder.parent_id = item.value("parent_id", ""); // Load parent_id, default to empty string if not found
            folders.push_back(folder);
        }
    } catch (const json::exception& e) {
        std::cerr << "ConnectionManager: Error parsing folders file: " << e.what() << std::endl;
        if (file.is_open()) file.close();
        // Optionally, attempt to backup corrupted file and/or return empty list
        return {}; // Return empty on error
    }

    return folders;
}

bool ConnectionManager::delete_connection(const std::string& connection_id) {
    try {
        std::vector<ConnectionInfo> connections = load_connections();

        auto it = std::remove_if(connections.begin(), connections.end(),
            [&connection_id](const ConnectionInfo& c) { return c.id == connection_id; });

        if (it != connections.end()) {
            connections.erase(it, connections.end());

            // Write back to file
            json j_connections = json::array();
            for (const auto& conn : connections) {
                j_connections.push_back({
                    {"id", conn.id},
                    {"name", conn.name},
                    {"host", conn.host},
                    {"port", conn.port},
                    {"username", conn.username},
                    {"folder_id", conn.folder_id},
                    {"connection_type", conn.connection_type} // Save connection type
                });
            }

            std::ofstream file(get_connections_file());
            file << j_connections.dump(4);
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Error deleting connection: " << e.what() << std::endl;
        return false;
    }
}

// Private helper function for recursive deletion
void ConnectionManager::delete_folder_recursive(const std::string& folder_id_to_delete, std::vector<FolderInfo>& all_folders, std::vector<ConnectionInfo>& all_connections) {
    // First, delete all connections directly inside this folder
    all_connections.erase(
        std::remove_if(all_connections.begin(), all_connections.end(),
                       [&](const ConnectionInfo& conn) {
                           return conn.folder_id == folder_id_to_delete;
                       }),
        all_connections.end()
    );

    // Find sub-folders
    std::vector<std::string> sub_folder_ids;
    for (const auto& folder : all_folders) {
        if (folder.parent_id == folder_id_to_delete) {
            sub_folder_ids.push_back(folder.id);
        }
    }

    // Recursively delete each sub-folder and its contents
    for (const auto& sub_folder_id : sub_folder_ids) {
        delete_folder_recursive(sub_folder_id, all_folders, all_connections); // Recursive call
    }

    // Finally, remove the current folder itself from the list of all folders
    all_folders.erase(
        std::remove_if(all_folders.begin(), all_folders.end(),
                       [&](const FolderInfo& f) {
                           return f.id == folder_id_to_delete;
                       }),
        all_folders.end()
    );
}

bool ConnectionManager::delete_folder(const std::string& folder_id) {
    try {
        std::vector<FolderInfo> folders = load_folders();
        std::vector<ConnectionInfo> connections = load_connections();

        // Check if the target folder actually exists before starting recursive deletion
        auto it_target_folder = std::find_if(folders.begin(), folders.end(),
                                         [&folder_id](const FolderInfo& f) { return f.id == folder_id; });

        if (it_target_folder == folders.end()) {
            // Folder not found, nothing to delete
            return false;
        }

        // Call the recursive helper to delete the folder and its contents
        delete_folder_recursive(folder_id, folders, connections);

        // Save the modified folders list
        json j_folders = json::array();
        for (const auto& f : folders) {
            j_folders.push_back({
                {"id", f.id},
                {"name", f.name},
                {"parent_id", f.parent_id}
            });
        }
        std::ofstream folder_file(get_folders_file());
        folder_file << j_folders.dump(4);

        // Save the modified connections list
        json j_connections = json::array();
        for (const auto& conn : connections) {
            j_connections.push_back({
                {"id", conn.id},
                {"name", conn.name},
                {"host", conn.host},
                {"port", conn.port},
                {"username", conn.username},
                {"folder_id", conn.folder_id},
                {"connection_type", conn.connection_type}
            });
        }
        std::ofstream conn_file(get_connections_file());
        conn_file << j_connections.dump(4);

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error deleting folder and its contents: " << e.what() << std::endl;
        return false;
    }
}
