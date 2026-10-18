// PS5 Hub - HTTPS vérifié, empreinte SHA-256 et publication atomique.
// Copyright (C) 2026 PS5 Hub contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "hub/service.hpp"
#include "core/save_file.hpp"
#include "hub/download.hpp"
#include "hub/package.hpp"
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>

namespace hub
{
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
    total_ = 0;
    speed_ = 0;
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
    DownloadRequest request;
    request.url = url;
    request.path = path;
    request.sha256 = hash;
    request.limit = limit;
    request.retain_partial = false;
    request.cancelled = [this] { return cancelled(); };
    request.progress = [this](TransferProgress p)
    {
        received_ = p.received;
        total_ = p.total;
        speed_ = p.bytes_per_second;
    };
    return download_file(request).error;
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
                std::string url;
                for (const auto &entry : snapshot().entries)
                    if (entry.id == id)
                    {
                        url = entry.cover_url;
                        break;
                    }
                if (url.empty())
                    url = "https://raw.githubusercontent.com/tomoff1/ps5-hub/main/assets/covers/" +
                          id + ".hubart";
                const auto error = transfer(url, candidate, "", 8 * 1024 * 1024);
                if (error.empty())
                {
                    art = read_artwork(candidate);
                    if (art && !write_artwork(cached, *art).empty())
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
        auto error = transfer(catalog_url, candidate, "", 16 * 1024 * 1024);
        std::string text;
        std::vector<Entry> entries;
        if (error.empty() && !save::read_file(candidate, &text, 16 * 1024 * 1024))
            error = "Réponse du catalogue illisible";
        if (error.empty() && !parse_catalog(text, entries, error))
            error = "Catalogue distant invalide : " + error;
        std::vector<SourceStatus> sources;
        if (error.empty() && !parse_sources(text, sources, error))
            error = "États de sources invalides : " + error;
        if (error.empty() && entries.empty() && sources.empty())
            error = "Catalogue distant vide sans état de source, copie précédente conservée";
        if (error.empty())
            error = save::write_atomic(root_ + "/shared-v2.tsv", text);
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
        std::vector<SourceStatus> sources;
        if (!save::read_file(assets_ + "/catalog.tsv", &text, 16 * 1024 * 1024) ||
            !parse_catalog(text, all, error) || !parse_sources(text, sources, error))
        {
            finish("Catalogue fourni illisible : " + error);
            return;
        }
        // Le catalogue partagé remplace uniquement les fiches fournies ; les
        // imports privés sont fusionnés ensuite et restent prioritaires.
        struct stat shared_facts{};
        if (stat((root_ + "/shared-v2.tsv").c_str(), &shared_facts) == 0)
        {
            std::vector<Entry> shared;
            if (save::read_file(root_ + "/shared-v2.tsv", &text, 16 * 1024 * 1024) &&
                parse_catalog(text, shared, error))
            {
                std::vector<SourceStatus> remote_sources;
                if (parse_sources(text, remote_sources, error) &&
                    (!shared.empty() || !remote_sources.empty()))
                {
                    all = std::move(shared);
                    sources = std::move(remote_sources);
                }
            }
        }
        struct stat library_facts{};
        public_ids_.clear();
        for (const auto &entry : all)
            public_ids_.insert(entry.id);
        if (stat((assets_ + "/library.tsv").c_str(), &library_facts) == 0)
        {
            std::vector<Entry> library;
            if (!save::read_file(assets_ + "/library.tsv", &text, 16 * 1024 * 1024) ||
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
            if (!save::read_file(root_ + "/personal.tsv", &text, 16 * 1024 * 1024) ||
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
            state_.sources = std::move(sources);
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
        auto error = download_block_reason(entry_);
        if (error.empty())
        {
#ifdef HUB_TEST
            error = "Réseau désactivé dans les tests unitaires";
#else
            const bool pkg = entry_.kind == "pkg" || entry_.format == "PKG";
            DownloadRequest request;
            request.url = entry_.url;
            request.path = root_ + "/downloads/" + entry_.id + (pkg ? ".pkg" : ".zip");
            request.sha256 = entry_.sha256;
            request.expected_size = entry_.size;
            request.cancelled = [this] { return cancelled(); };
            request.progress = [this](TransferProgress p)
            {
                received_ = p.received;
                total_ = p.total;
                speed_ = p.bytes_per_second;
            };
            if (pkg)
                request.validate_file = [this](const std::string &path)
                { return validate_package(path, entry_.platform, entry_.title_id); };
            error = download_file(request).error;
#endif
        }
        auto files = save::list_files(root_ + "/downloads");
        {
            std::lock_guard lock(mutex_);
            state_.files = std::move(files);
        }
        finish(error.empty()
                   ? "Fichier téléchargé et vérifié. Installation système manuelle requise."
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
