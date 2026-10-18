#!/usr/bin/env bash
# PS5 Hub - Vérification réseau optionnelle sur PC (environ 64 Mo).
# Copyright (C) 2026 PS5 Hub contributors
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
mkdir -p build
"${HOST_CXX:-clang++}" -std=c++20 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -pthread \
    -Isrc host/network_smoke.cpp src/hub/service.cpp src/hub/catalog.cpp src/hub/artwork.cpp src/hub/artwork_codec.cpp src/hub/download.cpp src/hub/package.cpp src/hub/installer.cpp src/core/save_file.cpp \
    -lcurl -lcrypto -lwebp -lpng -ljpeg -o build/network_smoke
build/network_smoke
