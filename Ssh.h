#ifndef SSH_H
#define SSH_H

#include "Connections.h"
#include <vector>
#include <string>

namespace Ssh {

// Check if sshpass is available on the system
bool is_sshpass_available();

// Function to generate the SSH command and its arguments
std::vector<std::string> generate_ssh_command_args(const ConnectionInfo& conn_info);

} // namespace Ssh

#endif // SSH_H