// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
namespace hub
{
// Empty means the system accepted the launch request, not that the page loaded.
std::string open_system_browser(const std::string &url);
} // namespace hub
