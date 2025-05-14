#include "Ssh.h"
#include <iostream>
#include <sstream>
#include <iterator>
#include <vector>
#include <string>
#include <cstdlib>

namespace Ssh {

bool is_sshpass_available() {
    return system("which sshpass > /dev/null 2>&1") == 0;
}

std::vector<std::string> generate_ssh_command_args(const ConnectionInfo& conn_info) {
    std::vector<std::string> args;
    static bool sshpass_checked = false;
    static bool sshpass_available = false;

    // Check sshpass availability only once
    if (!sshpass_checked) {
        sshpass_available = is_sshpass_available();
        sshpass_checked = true;
    }

    // Handle password authentication first
    if (conn_info.auth_method == "Password" && !conn_info.password.empty()) {
        if (sshpass_available) {
            std::cerr << "WARNING: Using password authentication with sshpass" << std::endl;
            args.push_back("sshpass");
            args.push_back("-p");
            args.push_back("'" + conn_info.password + "'");
            args.push_back("ssh");
            args.push_back("-tt"); // Force pseudo-terminal allocation

            // Add port if needed
            if (conn_info.port > 0 && conn_info.port != 22) {
                args.push_back("-p");
                args.push_back(std::to_string(conn_info.port));
            }

            // Add user@host
            std::string user_host_arg;
            if (!conn_info.username.empty()) {
                user_host_arg = conn_info.username + "@" + conn_info.host;
            } else {
                user_host_arg = conn_info.host;
            }
            args.push_back(user_host_arg);
            return args;
        } else {
            std::cerr << "Warning: sshpass is not installed. Password authentication will be interactive." << std::endl;
            std::cerr << "To install sshpass, run: sudo apt install sshpass" << std::endl;
        }
    }

    // If we get here, either it's not password auth or sshpass is not available
    args.push_back("ssh");
    args.push_back("-tt"); // Force pseudo-terminal allocation

    // Add port if it's non-standard (not 0 and not 22)
    if (conn_info.port > 0 && conn_info.port != 22) {
        args.push_back("-p");
        args.push_back(std::to_string(conn_info.port));
    }

    // Handle SSH key authentication
    if (conn_info.auth_method == "SSHKey" && !conn_info.ssh_key_path.empty()) {
        args.push_back("-i");
        args.push_back(conn_info.ssh_key_path);
    }

    // Add additional SSH options (split by space)
    if (!conn_info.additional_ssh_options.empty()) {
        std::istringstream iss(conn_info.additional_ssh_options);
        std::vector<std::string> options((std::istream_iterator<std::string>(iss)),
                                         std::istream_iterator<std::string>());
        args.insert(args.end(), options.begin(), options.end());
    }

    // Construct user@host part
    std::string user_host_arg;
    if (!conn_info.username.empty()) {
        user_host_arg = conn_info.username + "@" + conn_info.host;
    } else {
        user_host_arg = conn_info.host;
    }
    args.push_back(user_host_arg);

    return args;
}

} // namespace Ssh