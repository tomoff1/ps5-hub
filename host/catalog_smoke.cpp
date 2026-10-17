// PS5 Hub - Test HTTPS explicite du flux publié, sans télécharger de jeu.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "hub/service.hpp"
#include "core/save_file.hpp"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <thread>
#include <unistd.h>

int main()
{
    char pattern[] = "/tmp/ps5-hub-catalog-XXXXXX";
    const auto *dir = mkdtemp(pattern);
    if (!dir)
        return 1;
    const std::string root = dir;
    int status = 0;
    {
        hub::Service service(root, "assets");
        service.sync_catalog();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(55);
        while (service.snapshot().busy && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        const auto result = service.snapshot();
        std::string remote, local;
        if (result.busy || !hui::save::read_file(root + "/shared.tsv", &remote) ||
            !hui::save::read_file("shared/catalog.tsv", &local) || remote != local)
        {
            std::fprintf(stderr, "Échec du flux : %s\n", result.message.c_str());
            status = 1;
        }
        else
        {
            std::size_t games = 0;
            for (const auto &entry : result.entries)
                games += hub::is_game(entry);
            std::printf("HTTPS et cache : %zu fiches, %zu jeux/portages, copie identique.\n", result.entries.size(), games);
            if (games < 31)
                status = 1;
        }
    }
    std::filesystem::remove_all(root);
    return status;
}
