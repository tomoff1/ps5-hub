// PS5 Hub - Catalogue personnel borné et portable.
// Copyright (C) 2026 PS5 Hub contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace hub
{
struct Entry
{
    std::string id, title, version, kind, url, sha256, description;
};
bool valid_url(std::string_view url);
bool valid_hash(std::string_view hash);
bool is_game(const Entry &entry);
bool is_reference(const Entry &entry);
bool matches(const Entry &entry, std::string_view query);
std::string validate(const Entry &entry);
bool parse_catalog(std::string_view text, std::vector<Entry> &entries, std::string &error);
std::string serialize(const std::vector<Entry> &entries);
int compare_version(std::string_view a, std::string_view b);
} // namespace hub
