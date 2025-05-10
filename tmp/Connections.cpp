#include "Connections.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
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
            // Add new folder
            folders.push_back(folder);
        }

        // Write back to file
        json j_folders = json::array();
        for (const auto& f : folders) {
            j_folders.push_back({
                {"id", f.id},
                {"name", f.name}
            });
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
    try {
        if (!std::filesystem::exists(get_connections_file())) {
            return connections;
        }

        std::ifstream file(get_connections_file());
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
    try {
        if (!std::filesystem::exists(get_folders_file())) {
            return folders;
        }

        std::ifstream file(get_folders_file());
        json j_folders;
        file >> j_folders;

        for (const auto& j_folder : j_folders) {
            FolderInfo folder;
            folder.id = j_folder["id"];
            folder.name = j_folder["name"];
            folders.push_back(folder);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error loading folders: " << e.what() << std::endl;
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

bool ConnectionManager::delete_folder(const std::string& folder_id) {
    try {
        std::vector<FolderInfo> folders = load_folders();

        auto it = std::remove_if(folders.begin(), folders.end(),
            [&folder_id](const FolderInfo& f) { return f.id == folder_id; });

        if (it != folders.end()) {
            folders.erase(it, folders.end());

            // Write back to file
            json j_folders = json::array();
            for (const auto& folder : folders) {
                j_folders.push_back({
                    {"id", folder.id},
                    {"name", folder.name}
                });
            }

            std::ofstream file(get_folders_file());
            file << j_folders.dump(4);
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Error deleting folder: " << e.what() << std::endl;
        return false;
    }
}
