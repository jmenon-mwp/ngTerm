#ifndef CONNECTIONS_H
#define CONNECTIONS_H

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

// Struct to represent connection details
struct ConnectionInfo {
    std::string id;
    std::string name;
    std::string host;
    int port;
    std::string username;
    // Add more fields as needed
};

class ConnectionManager {
public:
    // Generate a unique connection ID
    static std::string generate_connection_id();

    // Save connection to JSON file
    static bool save_connection(const ConnectionInfo& connection);

    // Load all saved connections
    static std::vector<ConnectionInfo> load_connections();

    // Delete a connection by ID
    static bool delete_connection(const std::string& connection_id);

private:
    // Get the connections directory path
    static std::filesystem::path get_connections_directory();
};

#endif // CONNECTIONS_H
