// SPDX-License-Identifier: GPL-3.0-or-later
#include "hub/installer.hpp"
#include "hub/package.hpp"
#include <curl/curl.h>
#include <sys/statvfs.h>
#include <chrono>
#ifdef HUB_PS5
#include "platform/ps5/console_curl.h"
#endif
namespace hub
{
bool dpiv2_identity(const std::string &html)
{
    return html.find("etaHEN DPIv2") != html.npos &&
           (html.find("action='/upload'") != html.npos ||
            html.find("action=\"/upload\"") != html.npos) &&
           (html.find("name='file'") != html.npos || html.find("name=\"file\"") != html.npos);
}
bool dpiv2_accepted(const std::string &reply)
{
    return reply.starts_with("SUCCESS: PKG installation started from file:") &&
           reply.size() <= 4096 && reply.find("FAILED:") == reply.npos;
}
std::string storage_check(const std::string &path, std::uint64_t required)
{
    struct statvfs st{};
    if (statvfs(path.c_str(), &st) || !st.f_frsize)
        return "Impossible de vérifier l’espace disponible dans le stockage demandé";
    const auto blocks = static_cast<std::uint64_t>(st.f_bavail);
    const auto unit = static_cast<std::uint64_t>(st.f_frsize);
    const auto available = blocks > UINT64_MAX / unit ? UINT64_MAX : blocks * unit;
    return required > available ? "Espace libre insuffisant dans le stockage demandé" : "";
}
namespace
{
size_t receive(char *p, size_t s, size_t n, void *data)
{
    auto &body = *static_cast<std::string *>(data);
    if (s && n > SIZE_MAX / s)
        return 0;
    if (s * n > 65536 - body.size())
        return 0;
    body.append(p, s * n);
    return s * n;
}
struct Progress
{
    const std::function<bool()> &cancel;
    const std::function<void(TransferProgress)> &report;
    std::chrono::steady_clock::time_point began = std::chrono::steady_clock::now();
};
int upload_progress(void *data, curl_off_t, curl_off_t, curl_off_t total, curl_off_t now)
{
    auto &p = *static_cast<Progress *>(data);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - p.began)
                        .count();
    if (p.report)
        p.report(
            {static_cast<std::uint64_t>(now), static_cast<std::uint64_t>(total),
             ms > 0 ? static_cast<std::uint64_t>(now) * 1000 / static_cast<std::uint64_t>(ms) : 0,
             false});
    return p.cancel && p.cancel() ? 1 : 0;
}
CURL *handle(std::string &body)
{
    if (!initialize_network())
        return nullptr;
    auto *c = curl_easy_init();
    if (!c)
        return nullptr;
#ifdef HUB_PS5
    console_curl_setup(c);
#endif
    curl_easy_setopt(c, CURLOPT_PROXY, "");
    curl_easy_setopt(c, CURLOPT_NOPROXY, "*");
    curl_easy_setopt(c, CURLOPT_PROTOCOLS_STR, "http");
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 3L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, receive);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "PS5-Hub/0.5");
    return c;
}
} // namespace
InstallerResult probe_installer()
{
#ifdef HUB_TEST
    return {false, false, "Détection etaHEN désactivée dans les tests de navigation"};
#else
    std::string body;
    auto *c = handle(body);
    if (!c)
        return {false, false, "Initialisation réseau impossible"};
    curl_easy_setopt(c, CURLOPT_URL, "http://127.0.0.1:12800/");
    const auto code = curl_easy_perform(c);
    long status = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(c);
    const bool ok = code == CURLE_OK && status == 200 && dpiv2_identity(body);
    if (ok)
        return {true, false, "etaHEN DPIv2 identifié sur le port 12800"};
    // Legacy 9090 accepts raw JSON, not HTTP. Open/close TCP without sending
    // commands. A listening port alone does not identify an installer.
    body.clear();
    c = handle(body);
    bool legacy_open = false;
    if (c)
    {
        curl_easy_setopt(c, CURLOPT_URL, "http://127.0.0.1:9090/");
        curl_easy_setopt(c, CURLOPT_CONNECT_ONLY, 1L);
        curl_easy_setopt(c, CURLOPT_TIMEOUT, 1L);
        legacy_open = curl_easy_perform(c) == CURLE_OK;
        curl_easy_cleanup(c);
    }
    return {false, false,
            legacy_open ? "Port 9090 accessible, service non identifié. Envoi local pris en charge "
                          "uniquement par etaHEN DPIv2 sur 12800 : activez DPI v2."
                        : "etaHEN DPIv2 absent ou non reconnu sur 12800 ; port 9090 inaccessible. "
                          "Activez DPI v2 dans etaHEN."};
#endif
}
InstallerResult install_package(const Entry &e, const std::string &path,
                                const std::function<bool()> &cancelled,
                                const std::function<void(TransferProgress)> &progress)
{
    auto error = download_block_reason(e);
    if (error.empty())
        error = validate_package(path, e.platform, e.title_id);
    if (error.empty() && sha256_file(path, cancelled) != e.sha256)
        error = "Le SHA-256 local ne correspond pas au catalogue";
    const auto info = inspect_package(path);
    // DPIv2 stages an additional copy in /user/data/tmp. This is a minimum
    // staging check, not a claim to know the expanded application's size/quota.
    if (error.empty())
    {
#ifdef HUB_INSTALLER_TEST
        // Local simulator only; absent from the installed PS5 binary.
        error = storage_check(path.substr(0, path.find_last_of('/')), info.size);
#else
        error = storage_check("/user/data/tmp", info.size);
#endif
    }
    if (!error.empty())
        return {false, false, error};
    auto result = probe_installer();
    if (!result.available || (cancelled && cancelled()))
        return result;
    std::string body;
    auto *c = handle(body);
    if (!c)
        return {true, false, "Initialisation réseau impossible"};
    auto *mime = curl_mime_init(c);
    auto *part = mime ? curl_mime_addpart(mime) : nullptr;
    if (!part || curl_mime_name(part, "file") != CURLE_OK ||
        curl_mime_filedata(part, path.c_str()) != CURLE_OK ||
        curl_mime_filename(part, "hub-package.pkg") != CURLE_OK)
    {
        curl_mime_free(mime);
        curl_easy_cleanup(c);
        return {true, false, "Préparation de l’envoi impossible"};
    }
    Progress p{cancelled, progress};
    curl_easy_setopt(c, CURLOPT_URL, "http://127.0.0.1:12800/upload");
    curl_easy_setopt(c, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 86400L);
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_LIMIT, 128L);
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_TIME, 45L);
    curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, upload_progress);
    curl_easy_setopt(c, CURLOPT_XFERINFODATA, &p);
    const auto code = curl_easy_perform(c);
    long status = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
    curl_mime_free(mime);
    curl_easy_cleanup(c);
    result.accepted = code == CURLE_OK && status == 200 && dpiv2_accepted(body);
    result.uncertain =
        code != CURLE_OK || (!result.accepted && !(status == 200 && body.starts_with("FAILED:")));
    result.message =
        result.accepted ? "Demande acceptée par etaHEN. Vérifiez les notifications système : "
                          "installation terminée et jeu jouable non confirmés."
        : code != CURLE_OK ? "Envoi interrompu ou réponse perdue. État d’installation inconnu : "
                             "vérifiez etaHEN avant de réessayer."
        : result.uncertain ? "Réponse etaHEN inattendue. État d’installation inconnu : vérifiez "
                             "les notifications système avant de réessayer."
                           : "etaHEN a refusé la demande. Consultez ses notifications système.";
    if (!result.accepted && code == CURLE_OK)
    {
        result.message += " HTTP " + std::to_string(status) + ".";
        const auto start = body.find(", code ");
        if (start != body.npos)
        {
            auto end = start + 7;
            while (end < body.size() && end < start + 24 &&
                   ((body[end] >= '0' && body[end] <= '9') || body[end] == '-'))
                ++end;
            if (end > start + 7)
                result.message += " Code etaHEN : " + body.substr(start + 7, end - start - 7) + ".";
        }
    }
    return result;
}
} // namespace hub
