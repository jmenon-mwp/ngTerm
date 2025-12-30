#include "Iap.h"
#include <iostream>
#include <cstdlib>
#include <vector>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <array>
#include <glibmm.h>
#include <giomm/appinfo.h>

/**
 * Implementation Note:
 * This implementation aims to use Google API patterns. While the full
 * google-cloud-cpp SDK setup may require specific platform integration,
 * this module provides the required interface for ngTerm.
 */

namespace Iap {

/**
 * @brief Helper to execute a command and capture its output.
 */
static std::string exec_command(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        return "";
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    // Trim newline
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    return result;
}

bool is_authenticated() {
    // Check if gcloud or the library has valid credentials.
    // In a production environment with google-cloud-cpp, we would check:
    // auto credentials = google::cloud::MakeGoogleDefaultCredentials();
    // return credentials.ok();

    // Fallback/Bridge: Check if we can print an identity token without triggering a login prompt.
    int status = std::system("gcloud auth print-identity-token --quiet > /dev/null 2>&1");
    return (status == 0);
}

void authenticate() {
    // Invoke the default browser asking the user to authenticate.
    // This uses Giomm to reliably open the default browser on the system.

    std::cout << "GCP Authentication: Redirecting to browser..." << std::endl;

    // The OAuth2 flow usually starts with a specific URL.
    // Here we use gcloud's login flow as a robust way to handle the browser-based auth.
    // This is often more reliable than manual URL construction for desktop apps
    // without a fixed client secret.

    // If using a custom browser call:
    // Glib::RefPtr<Gio::AppInfo> app_info = Gio::AppInfo::create_from_commandline("xdg-open");
    // app_info->launch_uris({"https://accounts.google.com/o/oauth2/auth..."});

    // For now, invoking gcloud's standard flow which handles everything including the local server.
    std::system("gcloud auth login");
}

std::string generate_identity_token(const std::string& target_resource) {
    if (!is_authenticated()) {
        std::cerr << "IAP: Not authenticated to GCP. Call authenticate() first." << std::endl;
        return "";
    }

    // Generate the token. For IAP connections, we typically need an ID token.
    // If a target_resource (like a Client ID) is provided, it is used as the audience.

    std::string cmd = "gcloud auth print-identity-token --quiet";
    if (!target_resource.empty()) {
        cmd += " --audiences=\"" + target_resource + "\"";
    }

    std::string token = exec_command(cmd.c_str());

    if (token.empty()) {
        std::cerr << "IAP: Failed to generate identity token." << std::endl;
    }

    return token;
}

} // namespace Iap
