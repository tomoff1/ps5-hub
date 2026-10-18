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
    if (!hui::save::read_file(path, &data, 8 * 1024 * 1024) || data.size() < 12)
        return {};
    if (data.substr(0, 4) != "HUBA")
        return decode_artwork(data);
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
std::string write_artwork(const std::string &path, const Artwork &art)
{
    if (art.width < 1 || art.height < 1 || art.width > 256 || art.height > 384 ||
        art.pixels.size() != static_cast<std::size_t>(art.width * art.height * 4))
        return "Jaquette invalide";
    std::string bytes = "HUBA";
    for (const auto n : {art.width, art.height})
        for (int i = 0; i < 4; ++i)
            bytes += static_cast<char>((n >> (i * 8)) & 255);
    bytes.append(reinterpret_cast<const char *>(art.pixels.data()), art.pixels.size());
    return hui::save::write_atomic(path, bytes);
}
} // namespace hub
