#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
mkdir -p build
"${HOST_CXX:-clang++}" -std=c++20 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -pthread \
    -Isrc host/download_test.cpp src/hub/download.cpp src/hub/catalog.cpp src/core/save_file.cpp \
    -lcurl -lcrypto -o build/download_test
NATIVE_DOWNLOAD_RUNNER="$PWD/build/download_test" python3 -m unittest discover -s tests -p test_native_download.py -v
