// SPDX-License-Identifier: GPL-3.0-or-later
#include "hub/download.hpp"
#include "hub/catalog.hpp"
#include "core/save_file.hpp"
#include <curl/curl.h>
#include <openssl/evp.h>
#include <chrono>
#include <cstdio>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <algorithm>
#include <mutex>
#include <sys/stat.h>
#include <unistd.h>
#ifdef HUB_PS5
#include "platform/ps5/console_curl.h"
#endif

namespace hub
{
namespace
{
std::string hex(const unsigned char *bytes, unsigned length)
{
    std::string s;
    for (unsigned i = 0; i < length; ++i)
    {
        s += "0123456789abcdef"[bytes[i] >> 4];
        s += "0123456789abcdef"[bytes[i] & 15];
    }
    return s;
}
std::string hash_text(const std::string &text)
{
    unsigned char digest[32];
    unsigned length = 0;
    if (EVP_Digest(text.data(), text.size(), digest, &length, EVP_sha256(), nullptr) != 1 ||
        length != 32)
        return {};
    return hex(digest, length);
}
bool stopped(const DownloadRequest &r)
{
    return r.cancelled && r.cancelled();
}
struct Sink
{
    explicit Sink(const DownloadRequest &r) : request(r)
    {
    }
    const DownloadRequest &request;
    FILE *file = nullptr;
    EVP_MD_CTX *hash = nullptr;
    std::uint64_t offset = 0, count = 0, total = 0, range_start = 0;
    long status = 0;
    bool range_valid = false, restart = false, rejected = false;
    std::string etag, content_type, identity;
    std::chrono::steady_clock::time_point began = std::chrono::steady_clock::now(),
                                          reported = began;
};
std::size_t headers(char *data, std::size_t size, std::size_t count, void *user)
{
    if (size && count > SIZE_MAX / size)
        return 0;
    const auto length = size * count;
    if (length > 8192)
        return 0;
    auto &s = *static_cast<Sink *>(user);
    std::string line(data, length);
    if (line.starts_with("HTTP/"))
    {
        const auto space = line.find(' ');
        s.status = space == line.npos ? 0 : std::strtol(line.c_str() + space + 1, nullptr, 10);
        s.etag.clear();
        s.content_type.clear();
        s.range_valid = false;
        s.total = s.request.expected_size;
    }
    const auto colon = line.find(':');
    if (colon != line.npos)
    {
        auto key = line.substr(0, colon);
        for (auto &c : key)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        auto value = line.substr(colon + 1);
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
            value.erase(0, 1);
        while (!value.empty() && (value.back() == '\r' || value.back() == '\n'))
            value.pop_back();
        if (key == "etag" && value.size() <= 256 && value.find_first_of("\r\n") == value.npos)
            s.etag = value;
        else if (key == "content-type")
        {
            s.content_type = value;
            for (auto &c : s.content_type)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        else if (key == "content-length")
        {
            std::uint64_t bytes = 0;
            const auto result = std::from_chars(value.data(), value.data() + value.size(), bytes);
            if (!s.range_valid && result.ec == std::errc{} &&
                result.ptr == value.data() + value.size() && bytes <= s.request.limit)
                s.total = s.offset + bytes;
        }
        else if (key == "content-range")
        {
            unsigned long long first = 0, last = 0, total = 0;
            char tail = 0;
            if (std::sscanf(value.c_str(), "bytes %llu-%llu/%llu%c", &first, &last, &total,
                            &tail) == 3 &&
                first <= last && last < total && total <= s.request.limit &&
                (!s.request.expected_size || total == s.request.expected_size))
            {
                s.range_start = first;
                s.range_valid = true;
                s.total = total;
            }
        }
    }
    if (line == "\r\n" && (s.status == 200 || s.status == 206))
    {
        if (s.offset && s.status == 200)
            s.restart = true;
        if ((s.status == 206 && (!s.range_valid || s.range_start != s.offset)) ||
            s.total > s.request.limit || s.content_type.starts_with("text/html") ||
            (s.request.expected_size && s.total && s.total != s.request.expected_size))
            s.rejected = true;
        if (!s.restart && !s.rejected && s.request.retain_partial)
        {
            // Aucun jeton ou lien personnel lisible dans le fichier de reprise.
            const auto etag = s.etag.starts_with("W/") ? std::string{} : s.etag;
            hui::save::write_atomic(s.request.path + ".resume", s.identity + "\n" + etag + "\n");
        }
    }
    return length;
}
std::size_t write(char *data, std::size_t size, std::size_t n, void *user)
{
    auto &s = *static_cast<Sink *>(user);
    if (size && n > SIZE_MAX / size)
        return 0;
    const auto bytes = size * n;
    if (s.status >= 300 && s.status < 400)
        return bytes;
    if (stopped(s.request) || s.restart || s.rejected || (s.status != 200 && s.status != 206) ||
        s.offset + s.count > s.request.limit || bytes > s.request.limit - s.offset - s.count)
        return 0;
    if (std::fwrite(data, 1, bytes, s.file) != bytes || EVP_DigestUpdate(s.hash, data, bytes) != 1)
        return 0;
    s.count += bytes;
    const auto now = std::chrono::steady_clock::now();
    if (s.request.progress && now - s.reported >= std::chrono::milliseconds(200))
    {
        const auto seconds = std::chrono::duration<double>(now - s.began).count();
        s.request.progress({s.offset + s.count, s.total,
                            static_cast<std::uint64_t>(s.count / std::max(seconds, 0.001)), false});
        s.reported = now;
    }
    return bytes;
}
int progress(void *user, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
    return stopped(static_cast<Sink *>(user)->request) ? 1 : 0;
}
} // namespace

std::string sha256_file(const std::string &path, const std::function<bool()> &cancelled)
{
    auto *file = std::fopen(path.c_str(), "rb");
    auto *context = EVP_MD_CTX_new();
    bool ok = file && context && EVP_DigestInit_ex(context, EVP_sha256(), nullptr) == 1;
    std::vector<unsigned char> bytes(65536);
    while (ok)
    {
        if (cancelled && cancelled())
        {
            ok = false;
            break;
        }
        const auto count = std::fread(bytes.data(), 1, bytes.size(), file);
        if (!count)
        {
            ok = !std::ferror(file);
            break;
        }
        ok = EVP_DigestUpdate(context, bytes.data(), count) == 1;
    }
    unsigned char digest[32];
    unsigned length = 0;
    if (ok)
        ok = EVP_DigestFinal_ex(context, digest, &length) == 1 && length == 32;
    if (file)
        std::fclose(file);
    EVP_MD_CTX_free(context);
    return ok ? hex(digest, length) : std::string{};
}

bool initialize_network()
{
    static std::once_flag once;
    static CURLcode initialized = CURLE_FAILED_INIT;
    std::call_once(once, [] { initialized = curl_global_init(CURL_GLOBAL_DEFAULT); });
    return initialized == CURLE_OK;
}
DownloadResult download_file(const DownloadRequest &r)
{
    DownloadResult result;
    if (!valid_url(r.url) || (!r.sha256.empty() && !valid_hash(r.sha256)) || !r.limit ||
        r.expected_size > r.limit)
    {
        result.error = "Adresse, empreinte ou taille de téléchargement invalide";
        return result;
    }
    if (!initialize_network())
    {
        result.error = "Initialisation réseau impossible";
        return result;
    }
    const auto part = r.path + ".part", metadata = r.path + ".resume";
    const auto identity =
        hash_text(r.url + "\n" + r.sha256 + "\n" + std::to_string(r.expected_size));
    std::string previous, validator;
    struct stat facts{};
    std::uint64_t offset = 0;
    if (r.retain_partial && !r.sha256.empty() && hui::save::read_file(metadata, &previous, 1024) &&
        previous.starts_with(identity + "\n") && stat(part.c_str(), &facts) == 0 &&
        facts.st_size > 0 && static_cast<std::uint64_t>(facts.st_size) <= r.limit)
    {
        offset = static_cast<std::uint64_t>(facts.st_size);
        validator = previous.substr(identity.size() + 1);
        if (validator.ends_with('\n'))
            validator.pop_back();
        if (validator.find_first_of("\r\n") != validator.npos || validator.size() > 256)
            validator.clear();
    }
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        Sink sink(r);
        sink.identity = identity;
        sink.offset = offset;
        sink.total = r.expected_size;
        sink.hash = EVP_MD_CTX_new();
        bool ok = sink.hash && EVP_DigestInit_ex(sink.hash, EVP_sha256(), nullptr) == 1;
        if (offset && ok)
        {
            if (r.progress)
                r.progress({offset, r.expected_size, 0, true});
            auto *prefix = std::fopen(part.c_str(), "rb");
            std::vector<unsigned char> buffer(65536);
            std::uint64_t remaining = offset;
            while (prefix && remaining && ok && !stopped(r))
            {
                const auto got = std::fread(
                    buffer.data(), 1,
                    static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size(), remaining)),
                    prefix);
                if (!got)
                {
                    ok = false;
                    break;
                }
                remaining -= got;
                ok = EVP_DigestUpdate(sink.hash, buffer.data(), got) == 1;
            }
            if (prefix)
                std::fclose(prefix);
            else
                ok = false;
            if (remaining)
                ok = false;
        }
        if (offset && ok && !r.sha256.empty())
        {
            auto *copy = EVP_MD_CTX_new();
            unsigned char digest[32];
            unsigned length = 0;
            const bool complete = copy && EVP_MD_CTX_copy_ex(copy, sink.hash) == 1 &&
                                  EVP_DigestFinal_ex(copy, digest, &length) == 1 && length == 32 &&
                                  hex(digest, length) == r.sha256;
            EVP_MD_CTX_free(copy);
            if (complete)
            {
                result.bytes = offset;
                result.resumed = true;
                result.actual_sha256 = r.sha256;
                if (r.expected_size && offset != r.expected_size)
                    result.error = "Taille du fichier différente du catalogue";
                if (result.error.empty() && r.validate_file)
                    result.error = r.validate_file(part);
                if (result.error.empty() && std::rename(part.c_str(), r.path.c_str()) != 0)
                    result.error = "Impossible de publier le fichier vérifié";
                if (result.error.empty())
                    std::remove(metadata.c_str());
                EVP_MD_CTX_free(sink.hash);
                return result;
            }
        }
        sink.file = ok ? std::fopen(part.c_str(), offset ? "ab" : "wb") : nullptr;
        auto *curl = curl_easy_init();
        if (!ok || !sink.file || !curl || stopped(r))
        {
            if (sink.file)
                std::fclose(sink.file);
            if (curl)
                curl_easy_cleanup(curl);
            EVP_MD_CTX_free(sink.hash);
            result.error = stopped(r) ? "Téléchargement suspendu ; reprise conservée"
                                      : "Impossible de préparer le stockage";
            return result;
        }
#ifdef HUB_PS5
        console_curl_setup(curl);
#else
        if (!r.test_ca.empty())
            curl_easy_setopt(curl, CURLOPT_CAINFO, r.test_ca.c_str());
#endif
        curl_easy_setopt(curl, CURLOPT_URL, r.url.c_str());
        curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
        curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, r.retain_partial ? 86400L : 60L);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 128L);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 45L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "PS5-Hub/0.4");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headers);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &sink);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &sink);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        std::string range;
        curl_slist *request_headers = nullptr;
        if (offset)
        {
            range = std::to_string(offset) + "-";
            curl_easy_setopt(curl, CURLOPT_RANGE, range.c_str());
            if (!validator.empty())
                request_headers =
                    curl_slist_append(request_headers, ("If-Range: " + validator).c_str());
            if (request_headers)
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, request_headers);
        }
        sink.began = std::chrono::steady_clock::now();
        const auto rc = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.http_status);
        bool disk = std::fflush(sink.file) == 0;
        if (disk)
            disk = fsync(fileno(sink.file)) == 0;
        if (std::fclose(sink.file) != 0)
            disk = false;
        unsigned char digest[32];
        unsigned length = 0;
        ok = EVP_DigestFinal_ex(sink.hash, digest, &length) == 1 && length == 32;
        result.actual_sha256 = ok ? hex(digest, length) : std::string{};
        result.bytes = offset + sink.count;
        result.resumed = offset != 0;
        curl_slist_free_all(request_headers);
        curl_easy_cleanup(curl);
        EVP_MD_CTX_free(sink.hash);
        if (sink.restart && attempt == 0 && !stopped(r))
        {
            offset = 0;
            validator.clear();
            continue;
        }
        if (stopped(r))
            result.error = "Téléchargement suspendu ; reprise conservée";
        else if (result.http_status == 401)
            result.error = "Authentification requise par la source";
        else if (result.http_status == 403)
            result.error = "Accès refusé ou vérification de la source requise";
        else if (result.http_status == 429)
            result.error = "Limite de requêtes de la source ; réessayez plus tard";
        else if (sink.rejected)
            result.error = "Réponse HTML, plage ou taille inattendue : téléchargement refusé";
        else if (rc != CURLE_OK)
            result.error = "Téléchargement interrompu (code " + std::to_string(rc) +
                           ") ; réouvrez la fiche pour reprendre";
        else if (!disk || !ok)
            result.error = "Écriture ou vérification impossible ; vérifiez l’espace libre";
        else if ((r.expected_size && result.bytes != r.expected_size) ||
                 (sink.total && result.bytes != sink.total))
            result.error = "Fichier encore partiel ; reprise conservée";
        else if (!r.sha256.empty() && result.actual_sha256 != r.sha256)
        {
            result.error = "SHA-256 incorrect : fichier rejeté";
            std::remove(part.c_str());
            std::remove(metadata.c_str());
        }
        else
        {
            if (r.validate_file)
                result.error = r.validate_file(part);
            if (!result.error.empty())
            {
                std::remove(part.c_str());
                std::remove(metadata.c_str());
            }
            else if (std::rename(part.c_str(), r.path.c_str()) != 0)
                result.error = "Impossible de publier le fichier vérifié";
            else
                std::remove(metadata.c_str());
        }
        if (!r.retain_partial && !result.error.empty())
        {
            std::remove(part.c_str());
            std::remove(metadata.c_str());
        }
        if (r.progress)
            r.progress({result.bytes, sink.total, 0, result.error.empty()});
        return result;
    }
    result.error = "Le serveur ne permet pas cette reprise";
    return result;
}
} // namespace hub
