#include "Rdp.h"
#include <gtkmm/socket.h>
#include <gdk/gdkx.h>
#include <iostream>

// Initialize static member
GPid Rdp::pid_ = 0;

Gtk::Socket* Rdp::create_rdp_session(
    Gtk::Container& parent,
    const std::string& server,
    const std::string& username,
    const std::string& password,
    int width,
    int height)
{
    // Create a new socket
    Gtk::Socket* socket = Gtk::manage(new Gtk::Socket());
    parent.add(*socket);

    // Show the socket
    socket->show();

    // Get the X11 window ID
    Window xid = socket->get_id();

    // Build and execute the xfreerdp command
    try {
        std::vector<std::string> argv = build_rdp_command(server, username, password, width, height, xid);

        // Convert to char** for g_spawn_async
        std::vector<const char*> argv_ptrs;
        for (const auto& arg : argv) {
            argv_ptrs.push_back(arg.c_str());
        }
        argv_ptrs.push_back(nullptr);

        // Spawn the process
        g_spawn_async(
            nullptr,  // working directory
            const_cast<gchar**>(argv_ptrs.data()),
            nullptr,  // environment
            G_SPAWN_DO_NOT_REAP_CHILD,
            nullptr,  // child setup function
            nullptr,  // user data for setup function
            &pid_,
            nullptr   // GError
        );

        // Set up signal handler to clean up the process when it exits
        g_child_watch_add(pid_, [](GPid pid, gint status, gpointer) {
            g_spawn_close_pid(pid);
            pid_ = 0;
            return G_SOURCE_REMOVE;
        }, nullptr);

    } catch (const std::exception& e) {
        std::cerr << "Failed to start RDP client: " << e.what() << std::endl;
        delete socket;
        return nullptr;
    }

    return socket;
}

std::vector<std::string> Rdp::build_rdp_command(
    const std::string& server,
    const std::string& username,
    const std::string& password,
    int width,
    int height,
    Window xid)
{
    std::vector<std::string> args = {
        "xfreerdp",
        "/v:" + server,
        "/u:" + username,
        "/p:" + password,
        "/w:" + std::to_string(width),
        "/h:" + std::to_string(height),
        "/parent-window:" + std::to_string(xid),
        "/gdi:sw",           // Software rendering (more reliable for embedding)
        "/cert-ignore",       // Ignore certificate warnings
        "/f",                // Fullscreen within the parent window
        "/bpp:16",           // 16-bit color (better performance)
        "/compression",       // Enable compression
        "/sound:sys:alsa",    // Sound support
        "/microphone:sys:alsa",
        "/clipboard",         // Enable clipboard sharing
        "/gfx:RFX"            // Enable graphics pipeline
    };

    return args;
}
