#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
mkdir -p build
"${HOST_CXX:-clang++}" -std=c++20 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -pthread \
    -DHUB_INSTALLER_TEST=1 -Isrc host/installer_test.cpp src/hub/installer.cpp src/hub/package.cpp \
    src/hub/download.cpp src/hub/catalog.cpp src/core/save_file.cpp -lcurl -lcrypto -o build/installer_test
NATIVE_INSTALLER_RUNNER="$PWD/build/installer_test" python3 -m unittest discover -s tests -p test_native_installer.py -v
