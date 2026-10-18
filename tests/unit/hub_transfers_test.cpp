// SPDX-License-Identifier: GPL-3.0-or-later
#include "hub/service.hpp"
#include "hub/browser.hpp"
#include "hub/installer.hpp"
#include "core/save_file.hpp"
#include <gtest/gtest.h>
#include <filesystem>
#include <thread>
#include <chrono>
#include <unistd.h>
namespace
{
hub::Entry entry(std::string id)
{
    return {std::move(id),
            "Création libre",
            "01.000.000",
            "pkg",
            "https://example.org/test.pkg",
            std::string(64, 'a'),
            "Fichier personnel"};
}
class HubTransfers : public ::testing::Test
{
  protected:
    std::string root;
    void SetUp() override
    {
        char pattern[] = "/tmp/hub-transfers-XXXXXX";
        auto *path = mkdtemp(pattern);
        ASSERT_NE(path, nullptr);
        root = path;
        ASSERT_TRUE(hui::save::ensure_directory(root + "/assets"));
        ASSERT_TRUE(
            hui::save::write_atomic(root + "/assets/catalog.tsv", hub::serialize({})).empty());
    }
    void TearDown() override
    {
        std::filesystem::remove_all(root);
    }
    void wait(hub::Service &service)
    {
        for (int n = 0; n < 1000 && service.snapshot().busy; ++n)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        ASSERT_FALSE(service.snapshot().busy);
    }
};
} // namespace
TEST_F(HubTransfers, QueuePauseResumeCancelAndFailure)
{
    hub::Service service(root, root + "/assets");
    ASSERT_TRUE(service.load());
    wait(service);
    ASSERT_TRUE(service.download(entry("one")));
    ASSERT_TRUE(service.download(entry("two")));
    EXPECT_FALSE(service.download(entry("one")));
    ASSERT_TRUE(service.pause("one"));
    EXPECT_EQ(service.snapshot().transfers[0].state, "paused");
    ASSERT_TRUE(service.remove("two"));
    EXPECT_EQ(service.snapshot().transfers[1].state, "cancelled");
    ASSERT_TRUE(service.resume("one"));
    service.pump();
    wait(service);
    EXPECT_EQ(service.snapshot().transfers[0].state, "failed"); // HUB_TEST has no network
    EXPECT_EQ(service.snapshot().transfers[1].state, "cancelled");
    EXPECT_FALSE(service.resume("missing"));
}
TEST_F(HubTransfers, RestartPreservesQueueWithoutStartingNetwork)
{
    {
        hub::Service service(root, root + "/assets");
        ASSERT_TRUE(service.load());
        wait(service);
        ASSERT_TRUE(service.download(entry("one")));
    }
    hub::Service restored(root, root + "/assets");
    ASSERT_TRUE(restored.load());
    wait(restored);
    ASSERT_EQ(restored.snapshot().transfers.size(), 1u);
    EXPECT_EQ(restored.snapshot().transfers[0].state, "paused");
    EXPECT_EQ(restored.snapshot().transfers[0].entry.sha256, std::string(64, 'a'));
}
TEST_F(HubTransfers, CancellationDiscardsOnlyOwnedPartialAndKeepsFinal)
{
    hub::Service service(root, root + "/assets");
    ASSERT_TRUE(service.load());
    wait(service);
    ASSERT_TRUE(service.download(entry("one")));
    ASSERT_TRUE(hui::save::write_atomic(root + "/downloads/one.pkg.part", "partial").empty());
    ASSERT_TRUE(hui::save::write_atomic(root + "/downloads/one.pkg.resume", "resume").empty());
    ASSERT_TRUE(hui::save::write_atomic(root + "/downloads/one.pkg", "final").empty());
    ASSERT_TRUE(hui::save::write_atomic(root + "/downloads/other.pkg.part", "other").empty());
    ASSERT_TRUE(service.remove("one"));
    EXPECT_FALSE(std::filesystem::exists(root + "/downloads/one.pkg.part"));
    EXPECT_FALSE(std::filesystem::exists(root + "/downloads/one.pkg.resume"));
    EXPECT_TRUE(std::filesystem::exists(root + "/downloads/one.pkg"));
    EXPECT_TRUE(std::filesystem::exists(root + "/downloads/other.pkg.part"));
}
TEST_F(HubTransfers, CorruptJournalIsNeverOverwrittenByQueueOrMetadataWork)
{
    ASSERT_TRUE(hui::save::write_atomic(root + "/transfers.tsv", "broken journal").empty());
    hub::Service service(root, root + "/assets");
    ASSERT_TRUE(service.load());
    wait(service);
    EXPECT_FALSE(service.download(entry("one")));
    ASSERT_TRUE(service.load());
    wait(service);
    std::string text;
    ASSERT_TRUE(hui::save::read_file(root + "/transfers.tsv", &text));
    EXPECT_EQ(text, "broken journal");
}
TEST_F(HubTransfers, PumpProcessesQueueInOrderAndNeverRetriesFailureAutomatically)
{
    hub::Service service(root, root + "/assets");
    ASSERT_TRUE(service.load());
    wait(service);
    ASSERT_TRUE(service.download(entry("one")));
    ASSERT_TRUE(service.download(entry("two")));
    service.pump();
    wait(service);
    EXPECT_EQ(service.snapshot().transfers[0].state, "failed");
    EXPECT_EQ(service.snapshot().transfers[1].state, "queued");
    service.pump();
    wait(service);
    EXPECT_EQ(service.snapshot().transfers[1].state, "failed");
    service.pump();
    EXPECT_FALSE(service.snapshot().busy);
}
TEST_F(HubTransfers, OversizedJournalIsKeptAndBlocksReplacement)
{
    const std::string text(2 * 1024 * 1024 + 1, '#');
    ASSERT_TRUE(hui::save::write_atomic(root + "/transfers.tsv", text).empty());
    hub::Service service(root, root + "/assets");
    ASSERT_TRUE(service.load());
    wait(service);
    EXPECT_FALSE(service.download(entry("one")));
    EXPECT_EQ(std::filesystem::file_size(root + "/transfers.tsv"), text.size());
}
TEST_F(HubTransfers, CancellationReportsAnUndeletablePartial)
{
    hub::Service service(root, root + "/assets");
    ASSERT_TRUE(service.load());
    wait(service);
    ASSERT_TRUE(service.download(entry("one")));
    ASSERT_TRUE(hui::save::ensure_directory(root + "/downloads/one.pkg.part"));
    ASSERT_TRUE(hui::save::write_atomic(root + "/downloads/one.pkg.part/keep", "test").empty());
    EXPECT_TRUE(service.remove("one"));
    EXPECT_NE(service.snapshot().transfers[0].message.find("non supprimée"), std::string::npos);
    EXPECT_TRUE(std::filesystem::exists(root + "/downloads/one.pkg.part/keep"));
}
TEST_F(HubTransfers, InterruptedInstallationRemainsUnknown)
{
    ASSERT_TRUE(hui::save::write_atomic(root + "/transfers.tsv", "#TRANSFER\tone\tinstalling\n" +
                                                                     hub::serialize({entry("one")}))
                    .empty());
    hub::Service service(root, root + "/assets");
    ASSERT_TRUE(service.load());
    wait(service);
    ASSERT_EQ(service.snapshot().transfers.size(), 1u);
    EXPECT_EQ(service.snapshot().transfers[0].state, "unknown");
    EXPECT_FALSE(service.resume("one"));
    EXPECT_FALSE(service.install(entry("one")));
}
TEST_F(HubTransfers, RejectsUnconfirmedContentAndInvalidStorage)
{
    hub::Service service(root, root + "/assets");
    auto e = entry("restricted");
    e.authorization = "unconfirmed";
    EXPECT_FALSE(service.download(e));
    EXPECT_FALSE(hub::storage_check(root + "/missing", 1).empty());
    EXPECT_FALSE(hub::storage_check(root, UINT64_MAX).empty());
    EXPECT_TRUE(hub::storage_check(root, 1).empty());
}
TEST(HubInstaller, IdentityAndAcceptanceAreStrict)
{
    EXPECT_FALSE(hub::dpiv2_identity("<title>pkg-server</title>"));
    EXPECT_FALSE(hub::dpiv2_identity("etaHEN DPIv2"));
    EXPECT_TRUE(hub::dpiv2_identity("etaHEN DPIv2 <form action='/upload'><input name='file'>"));
    EXPECT_TRUE(
        hub::dpiv2_accepted("SUCCESS: PKG installation started from file: hub-package.pkg"));
    EXPECT_FALSE(hub::dpiv2_accepted("SUCCESS: Direct install console Task started for URL: x"));
    EXPECT_FALSE(
        hub::dpiv2_accepted("<html>SUCCESS: PKG installation started from file: x</html>"));
    EXPECT_FALSE(
        hub::dpiv2_accepted("SUCCESS: PKG installation started from file: x FAILED: error"));
}
TEST(HubBrowser, RejectsNonHttpsAndDoesNotSimulateSuccessOnHost)
{
    EXPECT_FALSE(hub::open_system_browser("file:///etc/passwd").empty());
    EXPECT_FALSE(hub::open_system_browser("https://example.org/").empty());
}
