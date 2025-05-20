#include "Rdp.h"
#include <gtkmm/socket.h>
#include <gdk/gdkx.h>
#include <iostream>
#include <glibmm.h>
#include <vector>
#include <string>
#include <regex>
#include <fcntl.h>  // For O_RDONLY
#include <unistd.h> // For close, write, unlink

// Initialize static members
GPid Rdp::pid_ = 0;
std::string Rdp::m_rdp_command;
std::string Rdp::m_credentials_file;
Rdp::ProcessExitSignal Rdp::process_exit_signal_;



// Static callback function for child process exit
void Rdp::on_child_exit(GPid pid, gint /* status */, gpointer /* user_data */) {
    Rdp* self = Rdp::instance();
    if (pid == self->pid_) {
        g_spawn_close_pid(self->pid_);
        self->pid_ = 0;
        // Emit signal that the process has exited
        self->process_exit_signal_.emit();
    }
}

// Define the C-style callback function
extern "C" {
void child_watch_cb(GPid pid, gint status, gpointer user_data) {
    // Call the private static method
    Rdp::on_child_exit(pid, status, user_data);
}
}

Gtk::Socket* Rdp::create_rdp_session(
    Gtk::Container& parent,
    const std::string& server,
    const std::string& username,
    const std::string& password,
    int width,
    int height)
{
    // Create a new socket
    Gtk::Socket* socket = new Gtk::Socket();

    // Set up the socket
    socket->set_events(Gdk::ALL_EVENTS_MASK);
    socket->set_visible(true);

    // Connect to the realize signal
    auto* socket_ptr = socket;
    auto* parent_ptr = &parent;

    // Connect to the realize signal to ensure the window is fully realized
    socket->signal_realize().connect([=]() {
        // Execute the RDP session creation in an idle callback
        Glib::signal_idle().connect_once([=]() mutable {
            // Create local variables for dimensions
            int actual_width = width;
            int actual_height = height;
            auto* self = Rdp::instance();
            try {
                // Get the X11 window ID
                Window xid = socket_ptr->get_id();
                std::cout << "RDP: Got XID: " << std::hex << xid << std::dec << std::endl;

                if (xid == 0) {
                    std::cerr << "RDP: Invalid X Window ID" << std::endl;
                    return;
                }

                // Log initial dimensions
                std::cout << "RDP: Initial dimensions - width: " << width << ", height: " << height << std::endl;

                // Ensure minimum dimensions (at least 800x600)
                if (actual_width < 800) {
                    std::cout << "RDP: Width too small (" << actual_width << "), using 1024" << std::endl;
                    actual_width = 1024;
                }
                if (actual_height < 600) {
                    std::cout << "RDP: Height too small (" << actual_height << "), using 768" << std::endl;
                    actual_height = 768;
                }

                std::cout << "RDP: Final dimensions - width: " << actual_width
                        << ", height: " << actual_height << std::endl;
                std::cout << "RDP: Connecting to " << server << " as " << username << std::endl;
                if (password.empty()) {
                    std::cerr << "RDP WARNING: Empty password provided!" << std::endl;
                }

                // Build the xfreerdp command using the build_rdp_command function
                std::vector<std::string> args = build_rdp_command(
                    server,
                    username,
                    password,
                    actual_width,
                    actual_height,
                    xid
                );

                // Convert to char** for g_spawn_async
                std::vector<const char*> argv_ptrs;
                for (const auto& arg : args) {
                    argv_ptrs.push_back(arg.c_str());
                }
                argv_ptrs.push_back(nullptr);

                // Spawn the process
                GPid pid;
                GError* error = nullptr;
                gboolean success = g_spawn_async(
                    nullptr, // working directory
                    const_cast<gchar**>(argv_ptrs.data()),
                    nullptr, // environment
                    static_cast<GSpawnFlags>(G_SPAWN_DO_NOT_REAP_CHILD),
                    nullptr, // child setup function
                    nullptr, // user data for setup
                    &pid,
                    &error
                );

                if (!success) {
                    std::cerr << "RDP: Failed to spawn process: " << error->message << std::endl;
                    g_error_free(error);
                    return;
                }

                self->pid_ = pid;
                g_child_watch_add(pid, child_watch_cb, nullptr);
                std::cout << "RDP: Started process with PID: " << pid << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "RDP: Error in RDP session creation: " << e.what() << std::endl;
            }
        });
    });

    return socket;
}

std::vector<std::string> Rdp::build_rdp_command(
    const std::string& server,
    const std::string& username,
    const std::string& password,
    int width,
    int height,
    unsigned long xid)
{
    // Build the command with basic settings
    std::vector<std::string> argv = {
        "/usr/bin/xfreerdp",
        "/v:" + server,
        "/u:" + username,
        "/w:" + std::to_string(width),
        "/h:" + std::to_string(height),
        "/parent-window:" + std::to_string(xid),
        "/smart-sizing",
        "/sec:rdp",
        "+clipboard",
        "+auto-reconnect",
        "/cert-ignore",
        "/bpp:16",   // Use 16-bit color depth
        "/rfx",      // Enable RDP 8.0 RemoteFX codec
        "/gdi:sw",   // Use software rendering for stability
        "/network:auto"  // Auto-detect network conditions
    };

    // Add password if provided (use /p: instead of /p: to avoid showing in logs)
    if (!password.empty()) {
        argv.push_back("/p:" + password);
    } else {
        // If no password, allow it to be entered interactively
        argv.push_back("/p:");
    }

    // Log the command (without password for security)
    std::cout << "RDP Command: ";
    for (const auto& arg : argv) {
        if (arg.find("/p:") == 0) {
            std::cout << "[PASSWORD_PROVIDED] ";
        } else {
            std::cout << arg << " ";
        }
    }
    std::cout << std::endl;

    return argv;
}