#include "Connections.h"
#include <iostream>
#include <algorithm>

std::string ConnectionManager::generate_connection_id() {
    // Use current timestamp and random number for uniqueness
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();

    // Generate a random number
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    int random_num = dis(gen);

    // Combine timestamp and random number
    std::stringstream ss;
    ss << std::hex << timestamp << "-" << std::setw(4) << std::setfill('0') << random_num;
    return ss.str();
}

std::filesystem::path ConnectionManager::get_connections_directory() {
    // Get the path to the user's home directory
    const char* home_dir = std::getenv("HOME");
    if (!home_dir) {
        throw std::runtime_error("Could not find home directory");
    }

    // Construct the path to the connections directory
    return std::filesystem::path(home_dir) / ".config" / "ngTerm" / "connections";
}

bool ConnectionManager::save_connection(const ConnectionInfo& connection) {
    try {
        // Ensure connections directory exists
        std::filesystem::path connections_dir = get_connections_directory();
        std::filesystem::create_directories(connections_dir);

        // Construct the filename using the connection ID
        std::filesystem::path connection_file = connections_dir / (connection.id + ".json");

        // Create JSON object for the connection
        json connection_json = {
            {"id", connection.id},
            {"name", connection.name},
            {"host", connection.host},
            {"port", connection.port},
            {"username", connection.username}
        };

        // Write to file
        std::ofstream file(connection_file);
        file << std::setw(4) << connection_json << std::endl;

        std::cout << "Saved connection: " << connection.name
                  << " (ID: " << connection.id << ")" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving connection: " << e.what() << std::endl;
        return false;
    }
}

std::vector<ConnectionInfo> ConnectionManager::load_connections() {
    std::vector<ConnectionInfo> connections;

    try {
        std::filesystem::path connections_dir = get_connections_directory();

        // Check if directory exists
        if (!std::filesystem::exists(connections_dir)) {
            return connections;
        }

        // Iterate through JSON files in the directory
        for (const auto& entry : std::filesystem::directory_iterator(connections_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                try {
                    std::ifstream file(entry.path());
                    json connection_json;
                    file >> connection_json;

                    ConnectionInfo connection{
                        connection_json["id"].get<std::string>(),
                        connection_json["name"].get<std::string>(),
                        connection_json["host"].get<std::string>(),
                        connection_json["port"].get<int>(),
                        connection_json["username"].get<std::string>()
                    };

                    connections.push_back(connection);
                } catch (const std::exception& e) {
                    std::cerr << "Error reading connection file "
                              << entry.path() << ": " << e.what() << std::endl;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error loading connections: " << e.what() << std::endl;
    }

    return connections;
}

bool ConnectionManager::delete_connection(const std::string& connection_id) {
    try {
        std::filesystem::path connections_dir = get_connections_directory();
        std::filesystem::path connection_file = connections_dir / (connection_id + ".json");

        if (std::filesystem::exists(connection_file)) {
            std::filesystem::remove(connection_file);
            std::cout << "Deleted connection: " << connection_id << std::endl;
            return true;
        }

        std::cerr << "Connection file not found: " << connection_id << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Error deleting connection: " << e.what() << std::endl;
        return false;
    }
}
