#ifndef RDP_H
#define RDP_H

#include <gtkmm/socket.h>
#include <glibmm/refptr.h>
#include <glibmm/spawn.h>
#include <string>

class Rdp {
public:
    // Create a new RDP session embedded in a Gtk::Socket
    // Returns a pointer to the created socket (owned by the parent container)
    // The parent container is responsible for managing the socket's lifetime
    static Gtk::Socket* create_rdp_session(
        Gtk::Container& parent,
        const std::string& server,
        const std::string& username,
        const std::string& password,
        int width = 1024,
        int height = 768);

    // Disable copy/move
    Rdp(const Rdp&) = delete;
    Rdp& operator=(const Rdp&) = delete;

private:
    Rdp() = default;
    ~Rdp() = default;

    // Helper function to build the xfreerdp command
    static std::vector<std::string> build_rdp_command(
        const std::string& server,
        const std::string& username,
        const std::string& password,
        int width,
        int height,
        Window xid);

    // PID of the xfreerdp process
    static GPid pid_;
};

#endif // RDP_EMBED_H
