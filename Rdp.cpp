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
    // Create local copies of the parameters that will be captured by value
    auto* socket_ptr = socket;
    auto server_copy = server;
    auto username_copy = username;
    auto password_copy = password;
    auto* parent_ptr = &parent;

    // Connect to the realize signal to ensure the window is fully realized
    socket->signal_realize().connect([=]() {
        // Execute the RDP session creation in an idle callback
        Glib::signal_idle().connect_once([=]() mutable {  // Add mutable to allow modifying captured variables
            // Create local variables for dimensions
            int actual_width = width;
            int actual_height = height;
            auto* self = Rdp::instance();  // Get the singleton instance
            try {
                // Get the X11 window ID
                Window xid = socket_ptr->get_id();
                std::cout << "RDP: Got XID: " << std::hex << xid << std::dec << std::endl;

                if (xid == 0) {
                    std::cerr << "RDP: Invalid X Window ID" << std::endl;
                    return; // Exit the idle callback
                }

                    // Get size from parent if available
                    if (Gtk::Widget* parent_widget = dynamic_cast<Gtk::Widget*>(parent_ptr)) {
                        // Get dimensions directly
                        actual_width = parent_widget->get_allocated_width();
                        actual_height = parent_widget->get_allocated_height();

                        // Ensure minimum dimensions
                        if (actual_width <= 0) actual_width = width;
                        if (actual_height <= 0) actual_height = height;
                    }

                    // Build the xfreerdp command with all options using actual dimensions
                    std::vector<std::string> args = {
                        "/usr/bin/xfreerdp",
                        "/v:" + server_copy,
                        "/u:" + username_copy,
                        "/p:" + password_copy,  // Password is passed directly in the command line
                        "/cert-ignore",   // Ignore certificate warnings
                        "/sec:rdp",       // Use TLS security
                        "/gdi:hw",        // Hardware graphics
                        "/smart-sizing",
                        "/size:" + std::to_string(actual_width) + "x" + std::to_string(actual_height),
                        "/window-drag",
                        "/sound:sys:alsa",
                        "/microphone:sys:alsa",
                        "/network:auto",
                        "/compression",
                        "/gfx:RFX",
                        "/rfx",
                        "/jpeg"
                    };

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
        // Create a temporary file for credentials
    std::string temp_filename = "/tmp/ngterm_rdp_XXXXXX";
    std::vector<char> temp_filename_vec(temp_filename.begin(), temp_filename.end());
    temp_filename_vec.push_back('\0');  // Null-terminate for mkstemp

    int fd = mkstemp(temp_filename_vec.data());
    if (fd == -1) {
        std::cerr << "RDP: Failed to create temporary credentials file" << std::endl;
        return {};
    }

    // Get the actual filename
    temp_filename = temp_filename_vec.data();

    try {
        // Write credentials to the temporary file
        std::string credentials = "username:" + username + "\npassword:" + password + "\n";
        if (write(fd, credentials.c_str(), credentials.length()) == -1) {
            throw std::runtime_error("Failed to write credentials");
        }

        // Set restrictive permissions on the credentials file
        if (fchmod(fd, S_IRUSR | S_IWUSR) == -1) {
            throw std::runtime_error("Failed to set file permissions");
        }

        close(fd);

        // Build the command with credentials file
        std::vector<std::string> argv = {
            "/usr/bin/xfreerdp",
            "/v:" + server,
            "/u:" + username,
            "/p:",  // Empty password as we're using credentials file
            "/w:" + std::to_string(width),
            "/h:" + std::to_string(height),
            "/parent-window:" + std::to_string(xid),
            "/sec:rdp",
            "+clipboard",
            "+auto-reconnect",
            "/smart-sizing",
            "/cert-ignore",
            "/from-stdin"  // Read credentials from stdin
        };

        // Store the credentials file path for cleanup
        m_credentials_file = temp_filename;

        return argv;

    } catch (const std::exception& e) {
        close(fd);
        unlink(temp_filename.c_str());
        std::cerr << "RDP Error: " << e.what() << std::endl;
        return {};
    }
}