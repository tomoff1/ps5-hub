// PS5 Hub - Test réseau explicite sur PC, jamais exécuté sur la console.
// Copyright (C) 2026 PS5 Hub contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "hub/service.hpp"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <thread>

static bool wait(hub::Service &service)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(3);
    while (service.snapshot().busy)
    {
        if (std::chrono::steady_clock::now() > deadline)
        {
            service.cancel();
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    std::puts(service.snapshot().message.c_str());
    return true;
}
int main()
{
    std::filesystem::create_directories("build/network-smoke");
    hub::Service service("build/network-smoke", "assets");
    if (!service.load() || !wait(service))
        return 1;
    const auto state = service.snapshot();
    if (state.entries.empty())
        return 2;
    auto entry = state.entries.front();
    if (entry.kind != "homebrew" || !hub::valid_hash(entry.sha256))
        return 3;
    if (!service.download(entry) || !wait(service))
        return 4;
    const auto file = "build/network-smoke/downloads/" + entry.id + ".zip";
    if (!std::filesystem::is_regular_file(file))
        return 5;
    entry.id = "checksum-refuse";
    entry.sha256 = std::string(64, '0');
    if (!service.download(entry) || !wait(service))
        return 6;
    if (service.snapshot().message.find("SHA-256 incorrecte") == std::string::npos)
        return 7;
    if (std::filesystem::exists("build/network-smoke/downloads/checksum-refuse.zip") ||
        std::filesystem::exists("build/network-smoke/downloads/checksum-refuse.zip.part"))
        return 8;
    std::puts("HTTPS et SHA-256 vérifiés sur PC ; aucune installation système effectuée.");
    return 0;
}
