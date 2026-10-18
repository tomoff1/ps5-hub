// PS5 Hub - Un seul travailleur pour les opérations disque et réseau.
// Copyright (C) 2026 PS5 Hub contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "hub/catalog.hpp"
#include "hub/artwork.hpp"
#include <map>
#include <set>
#include <atomic>
#include <mutex>
#include <pthread.h>

namespace hub
{
inline constexpr char version[] = "00.006.000";
inline constexpr char catalog_url[] =
    "https://ps5-hub-catalog.ps5-hub-catalog-service.workers.dev/api/native/catalog.tsv";
struct Transfer
{
    Entry entry;
    std::string state = "queued", message;
    bool install_after = false;
};
struct Snapshot
{
    std::vector<Entry> entries;
    std::vector<SourceStatus> sources;
    std::vector<std::string> files;
    std::vector<Transfer> transfers;
    std::string active_id;
    bool installer_available = false;
    std::string installer_message = "Service etaHEN non détecté";
    std::string message = "Chargement du catalogue…";
    bool busy = false;
    unsigned revision = 0;
    unsigned artwork_revision = 0;
    std::map<std::string, std::shared_ptr<const Artwork>> covers;
};
class Service
{
  public:
    Service(std::string root, std::string assets);
    ~Service();
    Service(const Service &) = delete;
    Service &operator=(const Service &) = delete;
    Snapshot snapshot();
    bool busy();
    bool poll(Snapshot &previous);
    bool load();
    bool add(Entry entry);
    bool download(Entry entry, bool install_after = false);
    bool cleanup(Entry entry, bool installation_confirmed);
    bool check_update();
    bool probe_installation();
    bool install(Entry entry);
    void pump();
    bool pause(const std::string &id);
    bool resume(const std::string &id);
    bool remove(const std::string &id);
    bool downloaded(const Entry &entry) const;
    bool sync_catalog();
    bool request_covers(std::vector<std::string> ids);
    void cancel();
    std::uint64_t bytes() const
    {
        return received_.load();
    }
    std::uint64_t total() const
    {
        return total_.load();
    }
    std::uint64_t speed() const
    {
        return speed_.load();
    }
    bool cancelled() const
    {
        return cancel_.load();
    }
    void count(std::uint64_t bytes)
    {
        received_.store(bytes);
    }

  private:
#ifdef HUB_TEST
    friend struct ServiceTestAccess;
#endif
    enum class Job
    {
        load,
        add,
        download,
        update,
        sync,
        covers,
        probe,
        install,
        cleanup
    };
    bool start(Job job, Entry entry = {});
    static void *run(void *self);
    void execute();
    void finish(std::string message);
    std::string persist_transfers();
    void log_network(const std::string &diagnostic);
    void restore_transfers();
    std::string download_path(const Entry &entry) const;
    std::string transfer(const std::string &url, const std::string &path, const std::string &hash,
                         std::uint64_t limit);
    std::string root_, assets_;
    std::mutex mutex_;
    Snapshot state_;
    pthread_t thread_{};
    bool joinable_ = false;
    Job job_ = Job::load;
    Entry entry_;
    std::vector<std::string> cover_ids_;
    std::set<std::string> public_ids_;
    bool restored_ = false, journal_valid_ = true;
    std::atomic<int> interrupt_{0}; // 1 pause, 2 cancel and discard partial
    std::atomic<bool> cancel_{false};
    std::atomic<std::uint64_t> received_{0};
    std::atomic<std::uint64_t> total_{0}, speed_{0};
};
} // namespace hub
