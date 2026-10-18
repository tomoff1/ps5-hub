// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <string>
namespace hub
{
struct PackageInfo
{
    std::string error, platform, content_id, title_id;
    std::uint64_t size = 0;
};
PackageInfo inspect_package(const std::string &path);
std::string validate_package(const std::string &path, const std::string &platform = "UNKNOWN",
                             const std::string &title_id = {});
} // namespace hub
