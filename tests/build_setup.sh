#!/bin/bash
set -eu
# Host-only package, installed in an ephemeral container for ZIP setup tests.
pacman -Sy --noconfirm minizip > test-artifacts/host-setup-build.log 2>&1
gcc -std=c11 -Wall -Wextra -D_DEFAULT_SOURCE -Ithird_party/lzma -Ilauncher/source \
    tests/setup_main.c launcher/source/game_setup.c third_party/lzma/LzmaDec.c \
    -lminizip -lz -o test-artifacts/test_setup
args=(--apk /inputs/boz.apk --etc /inputs/blackops_etc.dz)
if [ -f /inputs/boz_files.idx ]; then args+=(--index /inputs/boz_files.idx); fi
python3 tests/test_setup.py "${args[@]}"
