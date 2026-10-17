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
inline constexpr char version[] = "00.003.000";
struct Snapshot
{
    std::vector<Entry> entries;
    std::vector<std::string> files;
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
    bool poll(Snapshot &previous);
    bool load();
    bool add(Entry entry);
    bool download(Entry entry);
    bool check_update();
    bool sync_catalog();
    bool request_covers(std::vector<std::string> ids);
    void cancel();
    std::uint64_t bytes() const
    {
        return received_.load();
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
    enum class Job
    {
        load,
        add,
        download,
        update,
        sync,
        covers
    };
    bool start(Job job, Entry entry = {});
    static void *run(void *self);
    void execute();
    void finish(std::string message);
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
    std::atomic<bool> cancel_{false};
    std::atomic<std::uint64_t> received_{0};
};
} // namespace hub
