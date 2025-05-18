#ifndef RDP_H
#define RDP_H

#include <gtkmm/socket.h>
#include <gtkmm/container.h>
#include <glibmm.h>
#include <string>
#include <vector>
#include <glib.h>

// Forward declare the Rdp class
class Rdp;

// Declare the callback function
extern "C" {
void child_watch_cb(GPid pid, gint status, gpointer user_data);
}

class Rdp {
    public:
        static Gtk::Socket* create_rdp_session(
            Gtk::Container& parent,
            const std::string& server,
            const std::string& username,
            const std::string& password,
            int width = 1024,
            int height = 768);
        // Add this method
        static GPid get_pid() {
            return pid_;
        }

        // Disable copy/move
        Rdp(const Rdp&) = delete;
        Rdp& operator=(const Rdp&) = delete;

        // Friend declaration for the callback
        friend void ::child_watch_cb(GPid pid, gint status, gpointer user_data);

    private:
        Rdp() = default;
        ~Rdp() = default;

        // Static member to track the child process ID
        static GPid pid_;

        // Static callback for child process exit
        static void on_child_exit(GPid pid, gint status, gpointer user_data);

        // Helper function to build the xfreerdp command
        static std::vector<std::string> build_rdp_command(
            const std::string& server,
            const std::string& username,
            const std::string& password,
            int width,
            int height,
            unsigned long xid);
};

#endif // RDP_H
