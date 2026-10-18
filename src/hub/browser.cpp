// SPDX-License-Identifier: GPL-3.0-or-later
#include "hub/browser.hpp"
#include "hub/catalog.hpp"
#include <cstdio>
#ifdef HUB_PS5
extern "C" int sceSystemServiceLaunchWebBrowser(const char *uri, void *options);
#endif
namespace hub
{
std::string open_system_browser(const std::string &url)
{
    if (!valid_url(url))
        return "Adresse HTTPS de la fiche invalide";
#ifdef HUB_PS5
    // Same two-argument ABI as ps5-payload-dev/sdk samples/browser/main.c.
    // UserService is already initialized by the DualSense platform layer.
    const int code = sceSystemServiceLaunchWebBrowser(url.c_str(), nullptr);
    if (!code)
        return {};
    char message[160];
    std::snprintf(message, sizeof(message),
                  "Le système a refusé le navigateur (0x%08x). Disponibilité sur 13.60 à vérifier.",
                  static_cast<unsigned>(code));
    return message;
#else
    return "Le navigateur système PS5 est indisponible dans la version de test PC";
#endif
}
} // namespace hub
