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
#include <glibmm.h>

using json = nlohmann::json;

// Struct to represent a folder for organizing connections
struct FolderInfo {
    Glib::ustring id;        // Unique identifier for the folder
    Glib::ustring name;      // Display name of the folder
    Glib::ustring parent_id; // ID of the parent folder, empty for root
};

// Struct to represent connection details
struct ConnectionInfo {
    Glib::ustring id;
    Glib::ustring name;
    Glib::ustring host;
    int port;
    Glib::ustring username;
    Glib::ustring connection_type; // "SSH", "Telnet", "Serial", etc.
    Glib::ustring folder_id; // ID of the parent folder, or empty if top-level
    Glib::ustring auth_method;          // "Password" or "SSHKey"
    Glib::ustring password;             // SSH password (NOTE: Storing plain text is insecure)
    Glib::ustring ssh_key_path;         // Path to SSH private key file
    Glib::ustring ssh_key_passphrase;   // Passphrase for the SSH private key (if encrypted)
    Glib::ustring additional_ssh_options; // e.g., "-o StrictHostKeyChecking=no"
    bool is_folder = false; // Helper to distinguish in combined lists, not directly saved if representing a pure folder.
    Glib::ustring parent_id_col; // Only used by TreeView model logic
    ConnectionInfo() : port(0), is_folder(false) {}
};

class ConnectionManager {
public:
    // Generate a unique connection ID
    static Glib::ustring generate_connection_id();

    // Generate a unique folder ID
    static Glib::ustring generate_folder_id();

    // Save connection to JSON file
    static bool save_connection(const ConnectionInfo& connection);

    // Save folder to JSON file
    static bool save_folder(const FolderInfo& folder);

    // Load all saved connections
    static std::vector<ConnectionInfo> load_connections();

    // Load all saved folders
    static std::vector<FolderInfo> load_folders();

    // Get a connection by its ID
    static ConnectionInfo get_connection_by_id(const Glib::ustring& connection_id);

    // Delete a connection by ID
    static bool delete_connection(const Glib::ustring& connection_id);

    // Delete a folder by ID
    static bool delete_folder(const Glib::ustring& folder_id);

    // Get folder names for populating dropdown
    static std::vector<Glib::ustring> get_folder_names() {
        std::vector<Glib::ustring> folder_names;
        std::vector<FolderInfo> folders = load_folders();
        for (const auto& folder : folders) {
            folder_names.push_back(folder.name);
        }
        return folder_names;
    }

    // Get folder name by folder ID
    static Glib::ustring get_folder_name(const Glib::ustring& folder_id) {
        if (folder_id.empty()) return "";

        std::vector<FolderInfo> folders = load_folders();
        auto it = std::find_if(folders.begin(), folders.end(),
            [&folder_id](const FolderInfo& f) { return f.id == folder_id; });
        return (it != folders.end()) ? it->name : "";
    }

    // Get connections for a specific folder
    static std::vector<ConnectionInfo> get_connections_by_folder(const Glib::ustring& folder_id) {
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
    static Glib::ustring get_folder_id(const Glib::ustring& folder_name) {
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
    // Helper function to ensure parent directory exists
    static void ensure_parent_directory_exists(const std::filesystem::path& file_path);
    // Helper for recursive folder deletion
    static void delete_folder_recursive(const Glib::ustring& folder_id_to_delete, std::vector<FolderInfo>& all_folders, std::vector<ConnectionInfo>& all_connections);
};
