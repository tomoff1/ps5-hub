// PS5 Hub - Validation du catalogue, sans exécution de son contenu.
// Copyright (C) 2026 PS5 Hub contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "hub/catalog.hpp"
#include <array>
#include <set>
#include <cctype>

namespace hub
{
namespace
{
bool clean(std::string_view s, std::size_t max)
{
    if (s.size() > max)
        return false;
    for (unsigned char c : s)
        if (c < 32 || c == 127)
            return false;
    return true;
}
bool version(std::string_view s, std::array<int, 3> &out)
{
    int part = 0, digits = 0;
    for (char c : s)
    {
        if (c == '.')
        {
            if (!digits || part == 2)
                return false;
            ++part;
            digits = 0;
        }
        else
        {
            if (c < '0' || c > '9' || ++digits > 3)
                return false;
            out[part] = out[part] * 10 + c - '0';
        }
    }
    return part == 2 && digits > 0;
}
} // namespace
bool valid_url(std::string_view s)
{
    if (!s.starts_with("https://") || !clean(s, 2048))
        return false;
    auto end = s.find('/', 8);
    auto host = s.substr(8, end == s.npos ? s.size() - 8 : end - 8);
    if (host.empty() || host.find_first_of("@?#\\ ") != host.npos)
        return false;
    return s.find_first_of("\\ \"<>") == s.npos;
}
bool valid_hash(std::string_view h)
{
    if (h.size() != 64)
        return false;
    for (char c : h)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            return false;
    return true;
}
std::string validate(const Entry &e)
{
    if (e.id.empty() || e.id.size() > 64)
        return "Identifiant invalide";
    for (char c : e.id)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
            return "Identifiant invalide";
    if (e.title.empty() || !clean(e.title, 120) || !clean(e.description, 400))
        return "Texte invalide ou trop long";
    std::array<int, 3> v{};
    if (!version(e.version, v))
        return "Version attendue : 01.000.000";
    if (e.kind != "homebrew" && e.kind != "pkg" && e.kind != "link" && e.kind != "game")
        return "Type inconnu";
    if (!valid_url(e.url))
        return "Une adresse HTTPS sans identifiants est nécessaire";
    if (!e.sha256.empty() && !valid_hash(e.sha256))
        return "SHA-256 attendu : 64 caractères hexadécimaux minuscules";
    return {};
}
bool is_game(const Entry &e)
{
    return e.kind == "pkg" || e.kind == "game";
}
bool is_reference(const Entry &e)
{
    return e.kind == "link" || (e.kind == "game" && e.sha256.empty());
}
bool matches(const Entry &e, std::string_view query)
{
    const auto fold = [](std::string s)
    {
        std::string result;
        for (std::size_t i = 0; i < s.size(); ++i)
        {
            const auto c = static_cast<unsigned char>(s[i]);
            if (c == 0xc3 && i + 1 < s.size())
            {
                const auto n = static_cast<unsigned char>(s[i + 1]);
                char latin = 0;
                if ((n >= 0x80 && n <= 0x85) || (n >= 0xa0 && n <= 0xa5)) latin = 'a';
                else if (n == 0x87 || n == 0xa7) latin = 'c';
                else if ((n >= 0x88 && n <= 0x8b) || (n >= 0xa8 && n <= 0xab)) latin = 'e';
                else if ((n >= 0x8c && n <= 0x8f) || (n >= 0xac && n <= 0xaf)) latin = 'i';
                else if (n == 0x91 || n == 0xb1) latin = 'n';
                else if ((n >= 0x92 && n <= 0x96) || (n >= 0xb2 && n <= 0xb6)) latin = 'o';
                else if ((n >= 0x99 && n <= 0x9c) || (n >= 0xb9 && n <= 0xbc)) latin = 'u';
                else if (n == 0x9d || n == 0xbd || n == 0xbf) latin = 'y';
                if (latin)
                {
                    result += latin;
                    ++i;
                    continue;
                }
            }
            result += static_cast<char>(std::tolower(c));
        }
        return result;
    };
    const auto text = fold(e.title + " " + e.description + " " + e.id);
    auto words = fold(std::string(query));
    std::size_t start = 0;
    while (start < words.size())
    {
        const auto end = words.find(' ', start);
        const auto word = words.substr(start, end == words.npos ? words.size() - start : end - start);
        if (!word.empty() && text.find(word) == text.npos)
            return false;
        if (end == words.npos)
            break;
        start = end + 1;
    }
    return true;
}
bool parse_catalog(std::string_view text, std::vector<Entry> &out, std::string &error)
{
    std::vector<Entry> parsed;
    std::set<std::string> ids;
    if (text.size() > 4 * 1024 * 1024)
    {
        error = "Catalogue trop volumineux";
        return false;
    }
    while (!text.empty())
    {
        auto pos = text.find('\n');
        auto line = text.substr(0, pos);
        text = pos == text.npos ? std::string_view{} : text.substr(pos + 1);
        if (line.ends_with('\r'))
            line.remove_suffix(1);
        if (line.empty() || line.front() == '#')
            continue;
        std::array<std::string, 7> f;
        for (int i = 0; i < 7; ++i)
        {
            auto tab = line.find('\t');
            if ((i < 6 && tab == line.npos) || (i == 6 && tab != line.npos))
            {
                error = "Chaque ligne doit contenir 7 colonnes TSV";
                return false;
            }
            f[i] = line.substr(0, tab);
            line = tab == line.npos ? std::string_view{} : line.substr(tab + 1);
        }
        Entry e{f[0], f[1], f[2], f[3], f[4], f[5], f[6]};
        error = validate(e);
        if (!error.empty())
            return false;
        if (!ids.insert(e.id).second || parsed.size() >= 5000)
        {
            error = "Doublon ou limite de 5000 entrées";
            return false;
        }
        parsed.push_back(std::move(e));
    }
    out = std::move(parsed);
    error.clear();
    return true;
}
std::string serialize(const std::vector<Entry> &entries)
{
    std::string s = "# PS5 Hub catalogue v1\n";
    for (const auto &e : entries)
        s += e.id + '\t' + e.title + '\t' + e.version + '\t' + e.kind + '\t' + e.url + '\t' +
             e.sha256 + '\t' + e.description + '\n';
    return s;
}
int compare_version(std::string_view a, std::string_view b)
{
    std::array<int, 3> x{}, y{};
    if (!version(a, x) || !version(b, y))
        return -2;
    return x == y ? 0 : (x < y ? -1 : 1);
}
} // namespace hub
