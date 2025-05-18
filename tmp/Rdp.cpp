#include "Rdp.h"
#include <gtkmm/socket.h>
#include <gdk/gdkx.h>
#include <iostream>
#include <glibmm.h>

// Initialize static member
GPid Rdp::pid_ = 0;

// Static callback function for child process exit
void Rdp::on_child_exit(GPid pid, gint /* status */, gpointer /* user_data */) {
    if (pid > 0) {
        g_spawn_close_pid(pid);
        pid_ = 0;
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
socket->signal_realize().connect([socket, server, username, password, &width, &height, &parent]() {
    // Use a timeout to ensure the window is fully realized
    Glib::signal_timeout().connect_once([socket, server, username, password, &width, &height, &parent]() {
        try {
            // Get the X11 window ID
            Window xid = socket->get_id();
            std::cout << "RDP: Got XID: " << std::hex << xid << std::dec << std::endl;

            if (xid == 0) {
                std::cerr << "RDP: Invalid X Window ID" << std::endl;
                return;
            }

            // Get initial size from parent
            if (Gtk::Widget* parent_widget = dynamic_cast<Gtk::Widget*>(&parent)) {
                int parent_width = parent_widget->get_allocated_width();
                int parent_height = parent_widget->get_allocated_height();
                width = parent_width;
                height = parent_height;
            }

            // Build and execute the xfreerdp command
            std::vector<std::string> argv = build_rdp_command(server, username, password, width, height, xid);

            // Debug: Print the command
            std::cout << "RDP: Executing command:";
            for (const auto& arg : argv) {
                std::cout << " " << arg;
            }
            std::cout << std::endl;

            // Convert to char** for g_spawn_async
            std::vector<const char*> argv_ptrs;
            for (const auto& arg : argv) {
                argv_ptrs.push_back(arg.c_str());
            }
            argv_ptrs.push_back(nullptr);

            // Spawn the process
            GPid pid = 0;
            GError* error = nullptr;
            g_spawn_async(
                nullptr,  // working directory
                const_cast<gchar**>(argv_ptrs.data()),
                nullptr,  // environment
                G_SPAWN_DO_NOT_REAP_CHILD,
                nullptr,  // child setup function
                nullptr,  // user data for setup function
                &pid,
                &error
            );

            if (error) {
                std::cerr << "RDP: Failed to spawn process: " << error->message << std::endl;
                g_error_free(error);
            } else if (pid > 0) {
                pid_ = pid;
                g_child_watch_add(pid, child_watch_cb, nullptr);
                std::cout << "RDP: Started process with PID: " << pid << std::endl;
            } else {
                std::cerr << "RDP: Failed to spawn RDP process" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "RDP: Error in RDP session creation: " << e.what() << std::endl;
        }
    }, 100); // 100ms delay to ensure window is ready
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
    return {
        "/usr/bin/xfreerdp",
        "/v:" + server,
        "/u:" + username,
        "/p:" + password,  // No quotes needed
        "/w:" + std::to_string(width),
        "/h:" + std::to_string(height),
        "/parent-window:" + std::to_string(xid),
        // "/gdi:sw",
        // "/cert-ignore",
        // "/f",
        // "/bpp:16",
        // "/compression",
        "/sec:rdp",
        "+clipboard",
        "+auto-reconnect"
        // "/gfx:rfx",
        // "/rfx"
    };
}