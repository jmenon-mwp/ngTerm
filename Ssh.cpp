#include "Ssh.h"
#include <iostream> // For potential debugging, can be removed later

namespace Ssh {

std::vector<std::string> generate_ssh_command_args(const ConnectionInfo& conn_info) {
    std::vector<std::string> args;

    // Basic command
    args.push_back("ssh");

    // Add port if it's non-standard (not 0 and not 22)
    if (conn_info.port > 0 && conn_info.port != 22) {
        args.push_back("-p");
        args.push_back(std::to_string(conn_info.port));
    }

    // Construct user@host part
    std::string user_host_arg;
    if (!conn_info.username.empty()) {
        user_host_arg = conn_info.username + "@" + conn_info.host;
    } else {
        user_host_arg = conn_info.host;
    }
    args.push_back(user_host_arg);

    // For debugging purposes, print the command and arguments
    // std::cout << "Generated SSH command: ";
    // for (const auto& arg : args) {
    //     std::cout << arg << " ";
    // }
    // std::cout << std::endl;

    return args;
}

} // namespace Ssh