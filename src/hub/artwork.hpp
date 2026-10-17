// PS5 Hub - Jaquettes RGBA bornées, préparées par le bot PC.
// Copyright (C) 2026 PS5 Hub contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace hub
{
struct Artwork
{
    int width = 0, height = 0;
    std::vector<std::uint8_t> pixels;
};
std::shared_ptr<const Artwork> read_artwork(const std::string &path);
} // namespace hub
