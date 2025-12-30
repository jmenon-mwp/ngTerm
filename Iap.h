#ifndef IAP_H
#define IAP_H

#include <string>

/**
 * @namespace Iap
 * @brief Provides functionality for Google Identity-Aware Proxy (IAP) authentication and token management.
 */
namespace Iap {

/**
 * @brief Checks if the application is currently authenticated to Google Cloud Platform.
 *
 * This function verifies if valid Google Cloud credentials exist on the system
 * that can be used for IAP connections.
 *
 * @return true if authenticated, false otherwise.
 */
bool is_authenticated();

/**
 * @brief Invokes the default web browser to allow the user to authenticate with Google.
 *
 * This function initiates the Google OAuth2 flow, opening the browser to the
 * sign-in page. It handles the redirect/callback logic to obtain credentials.
 */
void authenticate();

/**
 * @brief Generates an Identity-Aware Proxy (IAP) token for a specific target resource.
 *
 * Once authenticated, this function creates an ID token which is required
 * to pass through the IAP tunnel to SSH or RDP services.
 *
 * @param target_resource The GCP resource or audience for which the token is generated.
 * @return A string containing the encoded IAP token, or an empty string on failure.
 */
std::string generate_identity_token(const std::string& target_resource = "");

} // namespace Iap

#endif // IAP_H
