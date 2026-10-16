// PS5 Hub - Tests des frontières de confiance du catalogue.
// Copyright (C) 2026 PS5 Hub contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "hub/catalog.hpp"
#include <gtest/gtest.h>

TEST(HubCatalog, RoundTripFrenchAndQueryString)
{
    std::vector<hub::Entry> input{{"jeu-libre", "Été à Paris", "01.002.003", "pkg",
                                   "https://example.org/game.pkg?a=b&c=d", std::string(64, 'a'),
                                   "Création personnelle"}},
        output;
    std::string error;
    ASSERT_TRUE(hub::parse_catalog(hub::serialize(input), output, error));
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output[0].title, input[0].title);
    EXPECT_EQ(output[0].url, input[0].url);
}
TEST(HubCatalog, RejectsTraversalAndEmbeddedControls)
{
    hub::Entry e{"../eboot", "Nom",        "01.000.000", "pkg", "https://example.org/a",
                 "",         "Description"};
    EXPECT_FALSE(hub::validate(e).empty());
    e.id = "safe";
    e.title = "a\nb";
    EXPECT_FALSE(hub::validate(e).empty());
}
TEST(HubCatalog, RejectsUnsafeUrls)
{
    for (const char *s : {"http://example.org/a", "file:///etc/passwd", "https://u:p@example.org/a",
                          "https:///a", "https://example.org/a\n"})
        EXPECT_FALSE(hub::valid_url(s)) << s;
    EXPECT_TRUE(hub::valid_url("https://github.com/user/project/releases/download/v1/app.zip"));
}
TEST(HubCatalog, BadImportDoesNotReplaceExistingData)
{
    std::vector<hub::Entry> entries{
        {"safe", "Nom", "01.000.000", "link", "https://example.org", "", ""}};
    std::string error;
    EXPECT_FALSE(hub::parse_catalog("bad\tdata", entries, error));
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].id, "safe");
    auto text = hub::serialize(entries);
    text += text;
    EXPECT_FALSE(hub::parse_catalog(text, entries, error));
}
TEST(HubCatalog, BoundsAndHash)
{
    std::vector<hub::Entry> entries;
    std::string error;
    EXPECT_FALSE(hub::parse_catalog(std::string(1024 * 1024 + 1, 'x'), entries, error));
    EXPECT_TRUE(hub::valid_hash(std::string(64, 'a')));
    EXPECT_FALSE(hub::valid_hash(std::string(63, 'a')));
    EXPECT_FALSE(hub::valid_hash(std::string(64, 'g')));
}
TEST(HubVersion, NumericRatherThanLexicographic)
{
    EXPECT_EQ(hub::compare_version("1.10.0", "1.9.0"), 1);
    EXPECT_EQ(hub::compare_version("01.002.003", "1.2.3"), 0);
    EXPECT_EQ(hub::compare_version("1.2", "1.2.3"), -2);
    EXPECT_EQ(hub::compare_version("1.2.3x", "1.2.3"), -2);
}
