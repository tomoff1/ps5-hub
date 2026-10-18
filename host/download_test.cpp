// SPDX-License-Identifier: GPL-3.0-or-later
#include "hub/download.hpp"
#include <cstdio>
#include <cstdlib>
int main(int argc, char **argv)
{
    if (argc != 6)
        return 2;
    hub::DownloadRequest request;
    request.url = argv[1];
    request.path = argv[2];
    request.sha256 = argv[3];
    request.test_ca = argv[4];
    request.expected_size = std::strtoull(argv[5], nullptr, 10);
    request.limit = 4 * 1024 * 1024;
    const auto result = hub::download_file(request);
    std::printf("%s\n", result.error.empty() ? "OK" : result.error.c_str());
    return result.error.empty() ? 0 : 1;
}
