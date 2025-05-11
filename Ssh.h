#ifndef SSH_H
#define SSH_H

#include <string>
#include <vector>
#include "Connections.h" // For ConnectionInfo

namespace Ssh {

// Function to generate the SSH command and its arguments
std::vector<std::string> generate_ssh_command_args(const ConnectionInfo& conn_info);

} // namespace Ssh

#endif // SSH_H