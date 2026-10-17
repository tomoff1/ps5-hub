// PS5 Hub - Vérifie dimensions et longueur avant tout envoi au GPU.
// Copyright (C) 2026 PS5 Hub contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "hub/artwork.hpp"
#include "core/save_file.hpp"
namespace hub
{
std::shared_ptr<const Artwork> read_artwork(const std::string &path)
{
    std::string data;
    if (!hui::save::read_file(path, &data, 12 + 256 * 384 * 4) || data.size() < 12 ||
        data.substr(0, 4) != "HUBA")
        return {};
    const auto number = [&](int at)
    {
        std::uint32_t n = 0;
        for (int i = 0; i < 4; ++i)
            n |= static_cast<std::uint32_t>(static_cast<unsigned char>(data[at + i])) << (i * 8);
        return n;
    };
    const auto width = number(4), height = number(8);
    if (width == 0 || height == 0 || width > 256 || height > 384 ||
        data.size() != 12 + static_cast<std::size_t>(width) * height * 4)
        return {};
    auto art = std::make_shared<Artwork>();
    art->width = static_cast<int>(width);
    art->height = static_cast<int>(height);
    art->pixels.assign(data.begin() + 12, data.end());
    return art;
}
} // namespace hub
