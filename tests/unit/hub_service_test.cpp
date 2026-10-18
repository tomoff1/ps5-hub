// PS5 Hub - Persistance réelle et refus des opérations concurrentes.
// Copyright (C) 2026 PS5 Hub contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "hub/service.hpp"
#include "core/save_file.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <thread>
#include <cstdlib>

namespace
{
class HubService : public ::testing::Test
{
  protected:
    std::string root;
    void SetUp() override
    {
        char pattern[] = "/tmp/ps5-hub-test-XXXXXX";
        auto *name = mkdtemp(pattern);
        ASSERT_NE(name, nullptr);
        root = name;
        ASSERT_TRUE(hui::save::ensure_directory(root + "/assets"));
        ASSERT_TRUE(hui::save::write_atomic(root + "/assets/catalog.tsv", "# empty\n").empty());
    }
    void TearDown() override
    {
        std::filesystem::remove_all(root);
    }
    static void wait(hub::Service &service)
    {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (service.snapshot().busy && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        ASSERT_FALSE(service.snapshot().busy);
    }
    hub::Entry entry()
    {
        return {"mon-jeu", "Mon jeu libre", "01.000.000", "pkg", "https://example.org/mon-jeu.pkg",
                "",        "Personnel"};
    }
};
TEST_F(HubService, SavesAndReloadsPersonalCatalog)
{
    {
        hub::Service service(root + "/data", root + "/assets");
        ASSERT_TRUE(service.load());
        wait(service);
        ASSERT_TRUE(service.add(entry()));
        wait(service);
        ASSERT_EQ(service.snapshot().entries.size(), 1u);
    }
    hub::Service reopened(root + "/data", root + "/assets");
    ASSERT_TRUE(reopened.load());
    wait(reopened);
    ASSERT_EQ(reopened.snapshot().entries.size(), 1u);
    EXPECT_EQ(reopened.snapshot().entries[0].title, "Mon jeu libre");
}
TEST_F(HubService, RejectsDuplicateAndMissingChecksum)
{
    hub::Service service(root + "/data", root + "/assets");
    ASSERT_TRUE(service.load());
    wait(service);
    ASSERT_TRUE(service.add(entry()));
    wait(service);
    ASSERT_TRUE(service.add(entry()));
    wait(service);
    EXPECT_EQ(service.snapshot().entries.size(), 1u);
    EXPECT_NE(service.snapshot().message.find("existe"), std::string::npos);
    EXPECT_FALSE(service.download(entry()));
    wait(service);
    EXPECT_NE(service.snapshot().message.find("SHA-256"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(root + "/data/downloads/mon-jeu.pkg"));
}
TEST_F(HubService, CorruptImportPreservesLastLoadedCatalog)
{
    hub::Service service(root + "/data", root + "/assets");
    ASSERT_TRUE(service.load());
    wait(service);
    ASSERT_TRUE(service.add(entry()));
    wait(service);
    ASSERT_TRUE(hui::save::write_atomic(root + "/data/personal.tsv", "broken").empty());
    ASSERT_TRUE(service.load());
    wait(service);
    EXPECT_EQ(service.snapshot().entries.size(), 1u);
    EXPECT_NE(service.snapshot().message.find("illisible"), std::string::npos);
    std::string text;
    ASSERT_TRUE(hui::save::read_file(root + "/data/personal.tsv", &text));
    EXPECT_EQ(text, "broken");
}
TEST_F(HubService, LoadsBulkLibrary)
{
    auto game = entry();
    ASSERT_TRUE(
        hui::save::write_atomic(root + "/assets/library.tsv", hub::serialize({game})).empty());
    hub::Service service(root + "/data", root + "/assets");
    ASSERT_TRUE(service.load());
    wait(service);
    ASSERT_EQ(service.snapshot().entries.size(), 1u);
    EXPECT_EQ(service.snapshot().entries[0].id, game.id);
}
TEST_F(HubService, SharedCatalogKeepsPrivateOverrides)
{
    hub::Service service(root + "/data", root + "/assets");
    ASSERT_TRUE(service.load());
    wait(service);
    auto shared = entry();
    shared.title = "Version du catalogue partagé";
    ASSERT_TRUE(
        hui::save::write_atomic(root + "/data/shared.tsv", hub::serialize({shared})).empty());
    auto personal = entry();
    personal.title = "Ma version privée";
    ASSERT_TRUE(
        hui::save::write_atomic(root + "/data/personal.tsv", hub::serialize({personal})).empty());
    ASSERT_TRUE(service.load());
    wait(service);
    ASSERT_EQ(service.snapshot().entries.size(), 1u);
    EXPECT_EQ(service.snapshot().entries[0].title, "Ma version privée");
    ASSERT_TRUE(service.sync_catalog());
    wait(service);
    // Le réseau est coupé par HUB_TEST : la copie et les fiches restent intactes.
    std::string text;
    ASSERT_TRUE(hui::save::read_file(root + "/data/shared.tsv", &text));
    EXPECT_EQ(text, hub::serialize({shared}));
    EXPECT_EQ(service.snapshot().entries[0].title, "Ma version privée");
}
TEST_F(HubService, ArtworkRejectsTruncation)
{
    const auto path = root + "/bad.hubart";
    ASSERT_TRUE(hui::save::write_atomic(path, "HUBA").empty());
    EXPECT_FALSE(hub::read_artwork(path));
    std::string data = "HUBA";
    data.append(8, '\0');
    data[4] = 1;
    data[8] = 1;
    ASSERT_TRUE(hui::save::write_atomic(path, data).empty());
    EXPECT_FALSE(hub::read_artwork(path));
    data.append(4, '\xff');
    ASSERT_TRUE(hui::save::write_atomic(path, data).empty());
    auto art = hub::read_artwork(path);
    ASSERT_TRUE(art);
    EXPECT_EQ(art->pixels.size(), 4u);
}
} // namespace
