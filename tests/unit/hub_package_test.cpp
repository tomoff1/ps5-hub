// SPDX-License-Identifier: GPL-3.0-or-later
#include "hub/package.hpp"
#include "core/save_file.hpp"
#include <gtest/gtest.h>
#include <cstdio>

namespace
{
void be32(std::string &s, std::size_t at, std::uint32_t n)
{
    for (int i = 0; i < 4; ++i)
        s[at + i] = static_cast<char>(n >> (24 - i * 8));
}
std::string cnt()
{
    std::string bytes(2048, '\0');
    bytes.replace(0, 4,
                  "\x7f"
                  "CNT",
                  4);
    be32(bytes, 0x10, 1);
    be32(bytes, 0x18, 0x5a0);
    be32(bytes, 0x5b0, 0x600);
    be32(bytes, 0x5b4, 16);
    bytes.replace(0x40, 19, "UP0000-CUSA12345_00-");
    return bytes;
}
struct PackageFixture : testing::Test
{
    const std::string path = "build/package-inspection-fixture.bin";
    void put(const std::string &bytes)
    {
        ASSERT_TRUE(hui::save::write_atomic(path, bytes).empty());
    }
    void TearDown() override
    {
        std::remove(path.c_str());
    }
};
} // namespace
TEST_F(PackageFixture, RecognizesBoundedCntAndChecksPlatformAndTitle)
{
    put(cnt());
    auto info = hub::inspect_package(path);
    EXPECT_TRUE(info.error.empty());
    EXPECT_EQ(info.platform, "PS4");
    EXPECT_EQ(info.title_id, "CUSA12345");
    EXPECT_TRUE(hub::validate_package(path, "PS4", "CUSA12345").empty());
    EXPECT_FALSE(hub::validate_package(path, "PS5").empty());
    EXPECT_FALSE(hub::validate_package(path, "PS4", "CUSA00000").empty());
}
TEST_F(PackageFixture, RejectsTruncatedTablesPayloadsAndOtherFormats)
{
    for (auto bytes : {std::string("PK\x03\x04", 4), std::string(2048, 'x')})
    {
        put(bytes);
        EXPECT_FALSE(hub::inspect_package(path).error.empty());
    }
    auto bytes = cnt();
    be32(bytes, 0x10, 8193);
    put(bytes);
    EXPECT_FALSE(hub::inspect_package(path).error.empty());
    bytes = cnt();
    be32(bytes, 0x5b0, 0xffffffff);
    put(bytes);
    EXPECT_FALSE(hub::inspect_package(path).error.empty());
}
TEST_F(PackageFixture, RecognizesFihWithoutDecryptingOrInstalling)
{
    auto inner = cnt();
    inner.replace(0x47, 4, "PPSA");
    std::string bytes(4096, '\0');
    bytes.replace(0, 4,
                  "\x7f"
                  "FIH",
                  4);
    bytes[0x59] = 0x10;
    bytes += inner;
    put(bytes);
    auto info = hub::inspect_package(path);
    EXPECT_TRUE(info.error.empty());
    EXPECT_EQ(info.platform, "PS5");
    EXPECT_EQ(info.title_id, "PPSA12345");
    bytes[0x5a] = 0x7f;
    put(bytes);
    EXPECT_FALSE(hub::inspect_package(path).error.empty());
}
