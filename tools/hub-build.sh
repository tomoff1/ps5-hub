#!/usr/bin/env bash
# PS5 Hub - Compilation reproductible sous Linux / WSL.
# Copyright (C) 2026 PS5 Hub contributors
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
# Certaines dépendances C amont ne savent pas installer dans un chemin avec espaces.
# Le clone Windows reste la source ; un cache Linux reçoit seulement les entrées de build.
if [[ $PWD == *[[:space:]]* ]]; then
    original=$PWD
    key=$(printf '%s' "$original" | sha256sum | cut -c1-16)
    cache="${HUB_BUILD_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/ps5-hub/$key}"
    if [[ $cache == *[[:space:]]* || $cache != /* ]]; then
        echo "HUB_BUILD_CACHE doit être un chemin Linux absolu sans espaces." >&2
        exit 1
    fi
    mkdir -p "$cache"
    tar --exclude=.git --exclude=.deps --exclude=build --exclude=dist \
        --exclude=.local --exclude=node_modules --exclude=.wrangler -cf - . | tar -xf - -C "$cache"
    bash "$cache/tools/hub-build.sh"
    mkdir -p "$original/dist"
    cp -a "$cache/dist/." "$original/dist/"
    exit 0
fi
export PS5_CLANG="${PS5_CLANG:-clang-18}"
export BUILD_JOBS="${BUILD_JOBS:-4}"
export USE_CCACHE="${USE_CCACHE:-0}"
make deps
make
python3 tools/hub-package.py
