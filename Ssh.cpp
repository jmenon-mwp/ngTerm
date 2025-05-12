#include "Ssh.h"
#include <iostream> // For potential debugging, can be removed later
#include <sstream>  // For splitting additional options
#include <iterator> // For splitting additional options
#include <vector>
#include <string>

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

    // Add identity file if auth method is SSH Key and path is specified
    if (conn_info.auth_method == "SSHKey" && !conn_info.ssh_key_path.empty()) {
        args.push_back("-i");
        args.push_back(conn_info.ssh_key_path);
        // Note: Handling the ssh_key_passphrase programmatically is complex
        // and often involves external tools like ssh-agent or sshpass.
        // For now, we rely on ssh to prompt for the passphrase if needed,
        // or for the user to have their key managed by ssh-agent.
    }
    // Note: Password auth is handled interactively by ssh itself.
    // We don't pass the password on the command line for security.

    // Add additional SSH options (split by space)
    if (!conn_info.additional_ssh_options.empty()) {
        std::istringstream iss(conn_info.additional_ssh_options);
        std::vector<std::string> options((std::istream_iterator<std::string>(iss)),
                                         std::istream_iterator<std::string>());
        // Insert the options into the main args vector
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