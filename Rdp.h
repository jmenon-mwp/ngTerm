#ifndef RDP_H
#define RDP_H

#include <gtkmm/socket.h>
#include <gtkmm/container.h>
#include <glibmm.h>
#include <string>
#include <vector>
#include <glib.h>
#include <unistd.h>
#include <sys/stat.h>

// Forward declare the Rdp class
class Rdp;

// Declare the callback function
extern "C" {
void child_watch_cb(GPid pid, gint status, gpointer user_data);
}

class Rdp {
    public:
        // Signal type for process exit
        using ProcessExitSignal = sigc::signal<void>;

        // Get the singleton instance
        static Rdp* instance() {
            static Rdp instance;
            return &instance;
        }

        // Get the signal
        ProcessExitSignal& signal_process_exit() {
            return process_exit_signal_;
        }

        static Gtk::Socket* create_rdp_session(
            Gtk::Container& parent,
            const std::string& server,
            const std::string& username,
            const std::string& password,
            int width,
            int height);

        static GPid get_pid() {
            return pid_;
        }

        static std::string get_rdp_command() {
            return m_rdp_command;
        }

        // Disable copy/move
        Rdp(const Rdp&) = delete;
        Rdp& operator=(const Rdp&) = delete;
        Rdp(Rdp&&) = delete;
        Rdp& operator=(Rdp&&) = delete;

        // Friend declaration for the callback
        friend void ::child_watch_cb(GPid pid, gint status, gpointer user_data);

    private:
        Rdp() = default;
        ~Rdp() {
            // Clean up credentials file if it exists
            if (!m_credentials_file.empty()) {
                unlink(m_credentials_file.c_str());
            }
        }

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

        static std::string m_rdp_command;
        static std::string m_credentials_file;
        static ProcessExitSignal process_exit_signal_;
};

#endif // RDP_H
