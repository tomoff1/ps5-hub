// SPDX-License-Identifier: GPL-3.0-or-later
// Inspection bornée de CNT et FIH. Aucun déchiffrement ni modification du PKG.
#include "hub/package.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <vector>
#include <climits>
namespace hub
{
namespace
{
std::uint32_t be32(const unsigned char *p)
{
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) | (std::uint32_t(p[2]) << 8) |
           p[3];
}
std::uint64_t le64(const unsigned char *p)
{
    std::uint64_t n = 0;
    for (int i = 0; i < 8; ++i)
        n |= std::uint64_t(p[i]) << (8 * i);
    return n;
}
bool read(FILE *file, std::uint64_t offset, void *buffer, std::size_t length)
{
    return offset <= LONG_MAX && std::fseek(file, static_cast<long>(offset), SEEK_SET) == 0 &&
           std::fread(buffer, 1, length, file) == length;
}
bool title(const std::string &s)
{
    if (s.size() != 9 || (!s.starts_with("CUSA") && !s.starts_with("PPSA")))
        return false;
    for (std::size_t i = 4; i < s.size(); ++i)
        if (s[i] < '0' || s[i] > '9')
            return false;
    return true;
}
} // namespace
PackageInfo inspect_package(const std::string &path)
{
    PackageInfo info;
    struct stat facts{};
    if (stat(path.c_str(), &facts) != 0 || !S_ISREG(facts.st_mode) || facts.st_size < 0x5a0)
    {
        info.error = "Paquet absent, trop court ou non régulier";
        return info;
    }
    info.size = static_cast<std::uint64_t>(facts.st_size);
    auto *file = std::fopen(path.c_str(), "rb");
    if (!file)
    {
        info.error = "Paquet illisible";
        return info;
    }
    std::array<unsigned char, 0x5a0> header{};
    std::uint64_t base = 0;
    if (!read(file, 0, header.data(), header.size()))
        info.error = "En-tête incomplet";
    else if (std::memcmp(header.data(),
                         "\x7f"
                         "FIH",
                         4) == 0)
    {
        info.platform = "PS5";
        base = le64(header.data() + 0x58);
        if (!base || base > info.size - header.size() ||
            !read(file, base, header.data(), header.size()))
            info.error = "Conteneur PS5 FIH invalide";
    }
    else
        info.platform = "PS4";
    if (info.error.empty() && std::memcmp(header.data(),
                                          "\x7f"
                                          "CNT",
                                          4) != 0)
        info.error = "Ce fichier n’est pas un PKG CNT/FIH pris en charge (ZIP/PFS/LZ4 refusé)";
    if (info.error.empty())
    {
        const auto count = be32(header.data() + 0x10), offset = be32(header.data() + 0x18);
        const auto bytes = std::uint64_t(count) * 0x20;
        if (!count || count > 8192 || offset < 0x20 || base + offset > info.size ||
            bytes > info.size - base - offset)
            info.error = "Table du PKG hors limites";
        else
        {
            std::vector<unsigned char> table(static_cast<std::size_t>(bytes));
            if (!read(file, base + offset, table.data(), table.size()))
                info.error = "Table du PKG tronquée";
            for (std::size_t i = 0; info.error.empty() && i < count; ++i)
            {
                const auto *entry = table.data() + i * 0x20;
                const auto position = std::uint64_t(be32(entry + 0x10)) + base;
                const auto size = be32(entry + 0x14);
                if (position > info.size || size > info.size - position)
                    info.error = "Entrée du PKG hors limites";
            }
        }
        for (std::size_t i = 0x40; i < 0x70 && header[i]; ++i)
        {
            if (header[i] < 32 || header[i] > 126)
            {
                info.content_id.clear();
                break;
            }
            info.content_id += static_cast<char>(header[i]);
        }
        if (info.content_id.size() >= 16 && title(info.content_id.substr(7, 9)))
            info.title_id = info.content_id.substr(7, 9);
    }
    std::fclose(file);
    return info;
}
std::string validate_package(const std::string &path, const std::string &platform,
                             const std::string &title_id)
{
    const auto info = inspect_package(path);
    if (!info.error.empty())
        return info.error;
    if (platform != "UNKNOWN" && !platform.empty() && platform != info.platform)
        return "La plateforme du fichier diffère de la fiche";
    if (!title_id.empty() && (!title(title_id) || title_id != info.title_id))
        return "L’identifiant du PKG diffère de la fiche";
    return {};
}
} // namespace hub
