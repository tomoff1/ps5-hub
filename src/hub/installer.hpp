// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "hub/catalog.hpp"
#include "hub/download.hpp"
namespace hub
{
struct InstallerResult
{
    bool available = false, accepted = false;
    std::string message;
    bool uncertain = false;
};
bool dpiv2_identity(const std::string &html);
bool dpiv2_accepted(const std::string &reply);
// Loopback only, fixed port, no redirects or proxy; never probes with a POST.
InstallerResult probe_installer();
InstallerResult install_package(const Entry &entry, const std::string &path,
                                const std::function<bool()> &cancelled,
                                const std::function<void(TransferProgress)> &progress);
std::string storage_check(const std::string &path, std::uint64_t required);
} // namespace hub
