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
namespace hub
{
// Only compiled by HUB_TEST: simulate a positive probe without opening sockets.
struct ServiceTestAccess
{
    static void identify_installer(Service &service)
    {
        std::lock_guard lock(service.mutex_);
        service.state_.installer_available = true;
    }
    static void complete_download(Service &service)
    {
        std::lock_guard lock(service.mutex_);
        service.state_.transfers.front().state = "complete";
    }
};
} // namespace hub
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
TEST_F(HubTransfers, RetryFromDetailsRequeuesFailedTransferWithoutDuplicatingHistory)
{
    hub::Service service(root, root + "/assets");
    ASSERT_TRUE(service.load());
    wait(service);
    ASSERT_TRUE(service.download(entry("one")));
    service.pump();
    wait(service);
    ASSERT_EQ(service.snapshot().transfers[0].state, "failed");
    auto refreshed = entry("one");
    refreshed.url = "https://example.org/refreshed.pkg";
    ASSERT_TRUE(service.download(refreshed));
    ASSERT_EQ(service.snapshot().transfers.size(), 1u);
    EXPECT_EQ(service.snapshot().transfers[0].state, "queued");
    EXPECT_EQ(service.snapshot().transfers[0].entry.url, refreshed.url);
}
TEST_F(HubTransfers, CombinedDownloadRequiresIdentifiedInstaller)
{
    hub::Service service(root, root + "/assets");
    ASSERT_TRUE(service.load());
    wait(service);
    EXPECT_FALSE(service.download(entry("one"), true));
    EXPECT_TRUE(service.snapshot().transfers.empty());
    EXPECT_TRUE(service.download(entry("one")));
}
TEST_F(HubTransfers, RestartNeverReplaysAutomaticInstallationConsent)
{
    ASSERT_TRUE(hui::save::ensure_directory(root + "/downloads"));
    ASSERT_TRUE(hui::save::write_atomic(root + "/downloads/one.pkg", "fixture").empty());
    ASSERT_TRUE(hui::save::write_atomic(root + "/transfers.tsv",
                                        "#TRANSFER\tone\tcomplete\n#AUTO_INSTALL\tone\t1\n" +
                                            hub::serialize({entry("one")}))
                    .empty());
    hub::Service service(root, root + "/assets");
    ASSERT_TRUE(service.load());
    wait(service);
    ASSERT_EQ(service.snapshot().transfers.size(), 1u);
    EXPECT_FALSE(service.snapshot().transfers[0].install_after);
    service.pump();
    EXPECT_FALSE(service.busy());
    EXPECT_EQ(service.snapshot().transfers[0].state, "complete");
}
TEST_F(HubTransfers, CombinedDownloadFailureDoesNotStartInstallation)
{
    hub::Service service(root, root + "/assets");
    ASSERT_TRUE(service.load());
    wait(service);
    hub::ServiceTestAccess::identify_installer(service);
    ASSERT_TRUE(service.download(entry("one"), true));
    EXPECT_TRUE(service.snapshot().transfers[0].install_after);
    service.pump();
    wait(service);
    EXPECT_EQ(service.snapshot().transfers[0].state, "failed");
    EXPECT_FALSE(service.snapshot().transfers[0].install_after);
    service.pump();
    EXPECT_FALSE(service.busy());
}
TEST_F(HubTransfers, CancellationRevokesCombinedModeConsent)
{
    hub::Service service(root, root + "/assets");
    ASSERT_TRUE(service.load());
    wait(service);
    hub::ServiceTestAccess::identify_installer(service);
    ASSERT_TRUE(service.download(entry("one"), true));
    ASSERT_TRUE(service.remove("one"));
    EXPECT_FALSE(service.snapshot().transfers[0].install_after);
    ASSERT_TRUE(service.resume("one"));
    EXPECT_FALSE(service.snapshot().transfers[0].install_after);
}
TEST_F(HubTransfers, SchedulerAttemptsCombinedInstallationOnceAfterCompletedDownload)
{
    hub::Service service(root, root + "/assets");
    ASSERT_TRUE(service.load());
    wait(service);
    hub::ServiceTestAccess::identify_installer(service);
    ASSERT_TRUE(service.download(entry("one"), true));
    hub::ServiceTestAccess::complete_download(service);
    service.pump();
    wait(service);
    // Missing local PKG fails inspection; no installer POST is sent in this unit test.
    EXPECT_EQ(service.snapshot().transfers[0].state, "install_error");
    EXPECT_FALSE(service.snapshot().transfers[0].install_after);
    service.pump();
    EXPECT_FALSE(service.busy());
}
TEST_F(HubTransfers, CleanupRequiresAcceptedRequestAndManualConfirmation)
{
    ASSERT_TRUE(hui::save::ensure_directory(root + "/downloads"));
    for (const auto *name : {"one.pkg", "one.pkg.part", "one.pkg.resume", "other.pkg"})
        ASSERT_TRUE(hui::save::write_atomic(root + "/downloads/" + name, "fixture").empty());
    ASSERT_TRUE(hui::save::write_atomic(root + "/transfers.tsv", "#TRANSFER\tone\taccepted\n" +
                                                                     hub::serialize({entry("one")}))
                    .empty());
    hub::Service service(root, root + "/assets");
    ASSERT_TRUE(service.load());
    wait(service);
    EXPECT_FALSE(service.cleanup(entry("one"), false));
    EXPECT_FALSE(service.cleanup(entry("other"), true));
    ASSERT_TRUE(service.cleanup(entry("one"), true));
    wait(service);
    EXPECT_FALSE(std::filesystem::exists(root + "/downloads/one.pkg"));
    EXPECT_FALSE(std::filesystem::exists(root + "/downloads/one.pkg.part"));
    EXPECT_FALSE(std::filesystem::exists(root + "/downloads/one.pkg.resume"));
    EXPECT_TRUE(std::filesystem::exists(root + "/downloads/other.pkg"));
    EXPECT_EQ(service.snapshot().transfers[0].state, "cleaned");
}
TEST_F(HubTransfers, CleanupRefusesUnknownInstallationState)
{
    ASSERT_TRUE(hui::save::write_atomic(root + "/transfers.tsv", "#TRANSFER\tone\tunknown\n" +
                                                                     hub::serialize({entry("one")}))
                    .empty());
    hub::Service service(root, root + "/assets");
    ASSERT_TRUE(service.load());
    wait(service);
    EXPECT_FALSE(service.cleanup(entry("one"), true));
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

TEST_F(HubTransfers, StorageDisappearingAfterQueueDoesNotLeaveTransferStuck)
{
    hub::Service service(root, root + "/assets");
    ASSERT_TRUE(service.load());
    wait(service);
    hub::ServiceTestAccess::identify_installer(service);
    ASSERT_TRUE(service.download(entry("one"), true));
    ASSERT_TRUE(std::filesystem::remove(root + "/downloads"));
    ASSERT_TRUE(hui::save::write_atomic(root + "/downloads", "not a directory").empty());
    service.pump();
    wait(service);
    EXPECT_EQ(service.snapshot().transfers[0].state, "failed");
    EXPECT_FALSE(service.snapshot().transfers[0].install_after);
    EXPECT_NE(service.snapshot().transfers[0].message.find("Stockage"), std::string::npos);
}
