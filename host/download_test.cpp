// SPDX-License-Identifier: GPL-3.0-or-later
#include "hub/download.hpp"
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
int main(int argc, char **argv)
{
    if (argc != 6 && argc != 7)
        return 2;
    hub::DownloadRequest request;
    request.url = argv[1];
    request.path = argv[2];
    request.sha256 = argv[3];
    request.test_ca = argv[4];
    request.expected_size = std::strtoull(argv[5], nullptr, 10);
    request.limit = 4 * 1024 * 1024;
    if (argc == 7)
    {
        const auto threshold = std::strtoull(argv[6], nullptr, 10);
        request.cancelled = [&request, threshold]
        {
            struct stat st{};
            return stat((request.path + ".part").c_str(), &st) == 0 && st.st_size > 0 &&
                   static_cast<unsigned long long>(st.st_size) >= threshold;
        };
    }
    const auto result = hub::download_file(request);
    std::printf("%s\n", result.error.empty() ? "OK" : result.error.c_str());
    return result.error.empty() ? 0 : 1;
}
