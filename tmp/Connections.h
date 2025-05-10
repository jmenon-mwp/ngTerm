#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <random>
#include <iomanip>
#include <sstream>
#include <chrono>

using json = nlohmann::json;

// Struct to represent a folder for organizing connections
struct FolderInfo {
    std::string id;        // Unique identifier for the folder
    std::string name;      // Display name of the folder
};

// Struct to represent connection details
struct ConnectionInfo {
    std::string id;        // Unique identifier for the connection
    std::string name;      // Connection display name
    std::string host;      // Optional host
    int port;              // Optional port
    std::string username;  // Optional username
    std::string folder_id; // Optional folder reference
    std::string connection_type; // Type of connection (e.g., SSH, Telnet, VNC, RDP)
};

class ConnectionManager {
public:
    // Generate a unique connection ID
    static std::string generate_connection_id();

    // Generate a unique folder ID
    static std::string generate_folder_id();

    // Save connection to JSON file
    static bool save_connection(const ConnectionInfo& connection);

    // Save folder to JSON file
    static bool save_folder(const FolderInfo& folder);

    // Load all saved connections
    static std::vector<ConnectionInfo> load_connections();

    // Load all saved folders
    static std::vector<FolderInfo> load_folders();

    // Delete a connection by ID
    static bool delete_connection(const std::string& connection_id);

    // Delete a folder by ID
    static bool delete_folder(const std::string& folder_id);

    // Get folder names for populating dropdown
    static std::vector<std::string> get_folder_names() {
        std::vector<std::string> folder_names;
        std::vector<FolderInfo> folders = load_folders();
        for (const auto& folder : folders) {
            folder_names.push_back(folder.name);
        }
        return folder_names;
    }

    // Get folder name by folder ID
    static std::string get_folder_name(const std::string& folder_id) {
        if (folder_id.empty()) return "";

        std::vector<FolderInfo> folders = load_folders();
        auto it = std::find_if(folders.begin(), folders.end(),
            [&folder_id](const FolderInfo& f) { return f.id == folder_id; });

        return (it != folders.end()) ? it->name : "";
    }

    // Get connections for a specific folder
    static std::vector<ConnectionInfo> get_connections_by_folder(const std::string& folder_id) {
        std::vector<ConnectionInfo> folder_connections;
        std::vector<ConnectionInfo> all_connections = load_connections();

        for (const auto& connection : all_connections) {
            if (connection.folder_id == folder_id) {
                folder_connections.push_back(connection);
            }
        }

        return folder_connections;
    }

    // Get folder ID by folder name
    static std::string get_folder_id(const std::string& folder_name) {
        std::vector<FolderInfo> folders = load_folders();
        auto it = std::find_if(folders.begin(), folders.end(),
            [&folder_name](const FolderInfo& f) { return f.name == folder_name; });

        return (it != folders.end()) ? it->id : "";
    }

private:
    // Get the connections directory path
    static std::filesystem::path get_connections_directory();

    // Helper methods for file operations
    static std::filesystem::path get_connections_dir();
    static std::filesystem::path get_connections_file();
    static std::filesystem::path get_folders_file();
};
