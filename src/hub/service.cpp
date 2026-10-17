// PS5 Hub - HTTPS vérifié, empreinte SHA-256 et publication atomique.
// Copyright (C) 2026 PS5 Hub contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "hub/service.hpp"
#include "core/save_file.hpp"
#include <curl/curl.h>
#include <openssl/evp.h>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>
#ifdef HUB_PS5
#include "platform/ps5/console_curl.h"
#endif

namespace hub
{
namespace
{
struct Sink
{
    Service *service;
    std::FILE *file;
    EVP_MD_CTX *hash;
    std::uint64_t count = 0, limit = 0;
};
std::size_t write_chunk(char *data, std::size_t size, std::size_t n, void *user)
{
    auto &s = *static_cast<Sink *>(user);
    if (size && n > SIZE_MAX / size)
        return 0;
    const auto bytes = size * n;
    if (s.service->cancelled() || bytes > s.limit - s.count)
        return 0;
    if (std::fwrite(data, 1, bytes, s.file) != bytes || EVP_DigestUpdate(s.hash, data, bytes) != 1)
        return 0;
    s.count += bytes;
    s.service->count(s.count);
    return bytes;
}
int progress(void *user, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
    return static_cast<Service *>(user)->cancelled() ? 1 : 0;
}
} // namespace
Service::Service(std::string root, std::string assets)
    : root_(std::move(root)), assets_(std::move(assets))
{
}
Service::~Service()
{
    cancel();
    if (joinable_)
        pthread_join(thread_, nullptr);
}
Snapshot Service::snapshot()
{
    std::lock_guard lock(mutex_);
    return state_;
}
bool Service::poll(Snapshot &previous)
{
    std::lock_guard lock(mutex_);
    if (previous.revision == state_.revision)
        return false;
    previous = state_;
    return true;
}
void Service::cancel()
{
    cancel_ = true;
}
bool Service::load()
{
    return start(Job::load);
}
bool Service::add(Entry e)
{
    return start(Job::add, std::move(e));
}
bool Service::download(Entry e)
{
    return start(Job::download, std::move(e));
}
bool Service::check_update()
{
    return start(Job::update);
}
bool Service::sync_catalog()
{
    return start(Job::sync);
}
bool Service::request_covers(std::vector<std::string> ids)
{
    if (ids.size() > 12 || snapshot().busy)
        return false;
    for (const auto &id : ids)
    {
        if (id.empty() || id.size() > 64)
            return false;
        for (char c : id)
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
                return false;
    }
    cover_ids_ = std::move(ids);
    return start(Job::covers);
}
bool Service::start(Job job, Entry entry)
{
    {
        std::lock_guard lock(mutex_);
        if (state_.busy)
            return false;
    }
    if (joinable_)
    {
        pthread_join(thread_, nullptr);
        joinable_ = false;
    }
    job_ = job;
    entry_ = std::move(entry);
    cancel_ = false;
    received_ = 0;
    {
        std::lock_guard lock(mutex_);
        state_.busy = true;
        if (job != Job::covers)
            state_.message = "Opération en cours…";
        ++state_.revision;
    }
    if (pthread_create(&thread_, nullptr, run, this) != 0)
    {
        finish("Impossible de créer le travailleur");
        return false;
    }
    joinable_ = true;
    return true;
}
void *Service::run(void *self)
{
    static_cast<Service *>(self)->execute();
    return nullptr;
}
void Service::finish(std::string message)
{
    // Les liens personnels et leurs éventuels jetons ne sont jamais journalisés.
    const auto path = root_ + "/hub.log";
    struct stat st{};
    if (stat(path.c_str(), &st) == 0 && st.st_size > 256 * 1024)
        std::rename(path.c_str(), (path + ".1").c_str());
    if (auto *log = std::fopen(path.c_str(), "a"))
    {
        std::fprintf(log, "%s\n", message.c_str());
        std::fclose(log);
    }
    std::lock_guard lock(mutex_);
    state_.message = std::move(message);
    state_.busy = false;
    ++state_.revision;
}
std::string Service::transfer(const std::string &url, const std::string &path,
                              const std::string &hash, std::uint64_t limit)
{
#ifdef HUB_TEST
    // Les tests de navigation aléatoire ne doivent jamais ouvrir de connexion.
    (void)url;
    (void)path;
    (void)hash;
    (void)limit;
    return "Réseau désactivé dans les tests unitaires";
#endif
    if (!valid_url(url))
        return "Adresse HTTPS invalide";
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
        return "Initialisation réseau impossible";
    auto *curl = curl_easy_init();
    auto *digest = EVP_MD_CTX_new();
    const auto temp = path + ".part";
    auto *file = std::fopen(temp.c_str(), "wb");
    if (!curl || !digest || !file)
    {
        if (file)
            std::fclose(file);
        if (curl)
            curl_easy_cleanup(curl);
        EVP_MD_CTX_free(digest);
        curl_global_cleanup();
        std::remove(temp.c_str());
        return "Impossible de préparer le téléchargement";
    }
    Sink sink{this, file, digest, 0, limit};
    bool hash_ok = EVP_DigestInit_ex(digest, EVP_sha256(), nullptr) == 1;
#ifdef HUB_PS5
    console_curl_setup(curl);
#endif
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3600L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "PS5-Hub/0.3");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_chunk);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    const auto rc = hash_ok ? curl_easy_perform(curl) : CURLE_FAILED_INIT;
    unsigned char result[32];
    unsigned length = 0;
    hash_ok = hash_ok && EVP_DigestFinal_ex(digest, result, &length) == 1 && length == 32;
    bool disk = std::fflush(file) == 0;
    if (disk)
        disk = fsync(fileno(file)) == 0;
    if (std::fclose(file) != 0)
        disk = false;
    curl_easy_cleanup(curl);
    EVP_MD_CTX_free(digest);
    curl_global_cleanup();
    std::string actual;
    if (hash_ok)
        for (unsigned char byte : result)
        {
            actual += "0123456789abcdef"[byte >> 4];
            actual += "0123456789abcdef"[byte & 15];
        }
    std::string error;
    if (cancelled())
        error = "Téléchargement annulé";
    else if (rc != CURLE_OK)
        error = "Échec réseau, certificat, espace disque ou limite de taille (code " +
                std::to_string(rc) + ")";
    else if (!disk || !hash_ok)
        error = "Échec de vérification ou d’écriture";
    else if (!hash.empty() && actual != hash)
        error = "Empreinte SHA-256 incorrecte : fichier rejeté";
    else if (std::rename(temp.c_str(), path.c_str()) != 0)
        error = "Impossible de publier le fichier téléchargé";
    if (!error.empty())
        std::remove(temp.c_str());
    return error;
}
void Service::execute()
{
    using namespace hui;
    if (!save::ensure_directory(root_) || !save::ensure_directory(root_ + "/downloads") ||
        !save::ensure_directory(root_ + "/covers"))
    {
        finish("Stockage personnel inaccessible");
        return;
    }
    if (job_ == Job::covers)
    {
        std::map<std::string, std::shared_ptr<const Artwork>> covers;
        cover_ids_.push_back("hub-feature");
        cover_ids_.push_back("hub-homebrew");
        cover_ids_.push_back("hub-files");
        for (const auto &id : cover_ids_)
        {
            if (cancelled())
                break;
            auto art = read_artwork(assets_ + "/covers/" + id + ".hubart");
            const auto cached = root_ + "/covers/" + id + ".hubart";
            if (!art)
                art = read_artwork(cached);
            // Les identifiants privés ne sont jamais envoyés au serveur public.
            if (!art && public_ids_.count(id))
            {
                const auto candidate = cached + ".incoming";
                const auto error = transfer(
                    "https://raw.githubusercontent.com/tomoff1/ps5-hub/main/assets/covers/" + id + ".hubart",
                    candidate, "", 12 + 256 * 384 * 4);
                if (error.empty())
                {
                    art = read_artwork(candidate);
                    if (art && std::rename(candidate.c_str(), cached.c_str()) != 0)
                        art.reset();
                }
                std::remove(candidate.c_str());
            }
            if (art)
                covers.emplace(id, std::move(art));
        }
        std::lock_guard lock(mutex_);
        state_.covers = std::move(covers);
        state_.busy = false;
        ++state_.artwork_revision;
        ++state_.revision;
        return;
    }
    if (job_ == Job::sync)
    {
        const auto candidate = root_ + "/shared.incoming";
        auto error = transfer(
            "https://raw.githubusercontent.com/tomoff1/ps5-hub/main/shared/catalog.tsv",
            candidate, "", 4 * 1024 * 1024);
        std::string text;
        std::vector<Entry> entries;
        if (error.empty() && !save::read_file(candidate, &text, 4 * 1024 * 1024))
            error = "Réponse du catalogue illisible";
        if (error.empty() && !parse_catalog(text, entries, error))
            error = "Catalogue distant invalide : " + error;
        if (error.empty() && entries.empty())
            error = "Catalogue distant vide, copie précédente conservée";
        if (error.empty())
            error = save::write_atomic(root_ + "/shared.tsv", text);
        std::remove(candidate.c_str());
        if (!error.empty())
        {
            finish("Actualisation impossible. Catalogue conservé. " + error);
            return;
        }
        job_ = Job::load;
        execute();
        return;
    }
    if (job_ == Job::load)
    {
        std::string text, error;
        std::vector<Entry> all;
        if (!save::read_file(assets_ + "/catalog.tsv", &text, 1024 * 1024) ||
            !parse_catalog(text, all, error))
        {
            finish("Catalogue fourni illisible : " + error);
            return;
        }
        // Le catalogue partagé remplace uniquement les fiches fournies ; les
        // imports privés sont fusionnés ensuite et restent prioritaires.
        struct stat shared_facts{};
        if (stat((root_ + "/shared.tsv").c_str(), &shared_facts) == 0)
        {
            std::vector<Entry> shared;
            if (save::read_file(root_ + "/shared.tsv", &text, 4 * 1024 * 1024) &&
                parse_catalog(text, shared, error) && !shared.empty())
                all = std::move(shared);
        }
        struct stat library_facts{};
        public_ids_.clear();
        for (const auto &entry : all)
            public_ids_.insert(entry.id);
        if (stat((assets_ + "/library.tsv").c_str(), &library_facts) == 0)
        {
            std::vector<Entry> library;
            if (!save::read_file(assets_ + "/library.tsv", &text, 4 * 1024 * 1024) ||
                !parse_catalog(text, library, error))
            {
                finish("Bibliothèque PC illisible : " + error);
                return;
            }
            for (auto &entry : library)
            {
                bool replaced = false;
                for (auto &existing : all)
                    if (existing.id == entry.id)
                    {
                        existing = entry;
                        replaced = true;
                        break;
                    }
                if (!replaced)
                    all.push_back(std::move(entry));
            }
        }
        struct stat facts{};
        if (stat((root_ + "/personal.tsv").c_str(), &facts) == 0)
        {
            std::vector<Entry> personal;
            if (!save::read_file(root_ + "/personal.tsv", &text, 4 * 1024 * 1024) ||
                !parse_catalog(text, personal, error))
            {
                finish("Catalogue personnel illisible : " + error);
                return;
            }
            for (auto &e : personal)
            {
                bool replaced = false;
                for (auto &existing : all)
                    if (existing.id == e.id)
                    {
                        existing = e;
                        replaced = true;
                        break;
                    }
                if (!replaced)
                    all.push_back(std::move(e));
            }
        }
        auto files = save::list_files(root_ + "/downloads");
        {
            std::lock_guard lock(mutex_);
            state_.entries = std::move(all);
            state_.files = std::move(files);
        }
        finish("Bibliothèque chargée. Croix : ouvrir · Triangle : ajouter · Carré : actualiser.");
    }
    else if (job_ == Job::add)
    {
        auto error = validate(entry_);
        std::vector<Entry> personal;
        std::string text;
        struct stat facts{};
        if (error.empty() && stat((root_ + "/personal.tsv").c_str(), &facts) == 0)
        {
            if (!save::read_file(root_ + "/personal.tsv", &text, 1024 * 1024) ||
                !parse_catalog(text, personal, error))
            {
                finish("Catalogue personnel illisible, ajout refusé");
                return;
            }
        }
        auto current = snapshot();
        for (const auto &e : current.entries)
            if (e.id == entry_.id)
                error = "Cet identifiant existe déjà";
        if (personal.size() >= 500)
            error = "Limite du catalogue atteinte";
        if (!error.empty())
        {
            finish(error);
            return;
        }
        personal.push_back(entry_);
        error = save::write_atomic(root_ + "/personal.tsv", serialize(personal));
        if (error.empty())
        {
            std::lock_guard lock(mutex_);
            state_.entries.push_back(entry_);
        }
        finish(error.empty() ? "Lien personnel enregistré" : "Échec de sauvegarde : " + error);
    }
    else if (job_ == Job::download)
    {
        auto error = validate(entry_);
        if (error.empty() && !valid_hash(entry_.sha256))
            error = "Ajoute le SHA-256 fourni par l’auteur avant de télécharger";
        if (error.empty() && is_reference(entry_))
            error = "Cette fiche est un lien de référence, pas un fichier";
        if (error.empty())
            error = transfer(entry_.url,
                             root_ + "/downloads/" + entry_.id +
                                 (entry_.kind == "pkg" ? ".pkg" : ".zip"),
                             entry_.sha256, 2ULL * 1024 * 1024 * 1024);
        auto files = save::list_files(root_ + "/downloads");
        {
            std::lock_guard lock(mutex_);
            state_.files = std::move(files);
        }
        finish(error.empty() ? "Fichier téléchargé et vérifié. Installation système non disponible."
                             : error);
    }
    else
    {
        auto error =
            transfer("https://raw.githubusercontent.com/tomoff1/ps5-hub/main/release-version.txt",
                     root_ + "/release-version.txt", "", 128);
        std::string remote;
        if (error.empty() && !save::read_file(root_ + "/release-version.txt", &remote, 128))
            error = "Version distante illisible";
        while (!remote.empty() && (remote.back() == '\n' || remote.back() == '\r'))
            remote.pop_back();
        if (error.empty())
        {
            const auto result = compare_version(remote, version);
            error = result == -2 ? "Format de mise à jour invalide"
                    : result > 0 ? "Mise à jour disponible : " + remote
                                 : "PS5 Hub est à jour";
        }
        finish(error);
    }
}
} // namespace hub
