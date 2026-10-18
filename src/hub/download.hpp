// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <functional>
#include <string>

namespace hub
{
struct TransferProgress
{
    std::uint64_t received = 0, total = 0, bytes_per_second = 0;
    bool verifying = false;
};
struct DownloadRequest
{
    std::string url, path, sha256;
    std::uint64_t limit = 250ULL * 1024 * 1024 * 1024, expected_size = 0;
    bool retain_partial = true;
    std::function<bool()> cancelled;
    std::function<void(TransferProgress)> progress;
    std::function<std::string(const std::string &)> validate_file;
#ifndef HUB_PS5
    // Uniquement pour les certificats des serveurs de test locaux. Aucun
    // commutateur de désactivation TLS n'existe dans le binaire PS5.
    std::string test_ca;
#endif
};
struct DownloadResult
{
    std::string error, actual_sha256;
    std::uint64_t bytes = 0;
    long http_status = 0;
    bool resumed = false;
};
std::string sha256_file(const std::string &path, const std::function<bool()> &cancelled = {});
DownloadResult download_file(const DownloadRequest &request);
} // namespace hub
