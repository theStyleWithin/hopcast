#!/usr/bin/env bash
# Build and run the secure store tests (with sanitizers).
# Usage: hopcast/scripts/test-securestore.sh            # key-file path, plus keyring if one is running
#        HOPCAST_TEST_KEYRING=1 hopcast/scripts/...     # require the real keyring
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="${OUT:-$ROOT/build/securestore-test}"
mkdir -p "$(dirname "$OUT")"
flags=() libs=(-lcrypto)
if [[ "${HOPCAST_USE_KEYRING:-0}" == 1 ]]; then # off by default, like the app
  flags+=(-DHOPCAST_HAVE_LIBSECRET $(pkg-config --cflags libsecret-1)); libs+=($(pkg-config --libs libsecret-1))
fi
g++ -std=c++17 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined "${flags[@]}" -o "$OUT" \
  "$ROOT/hopcast/tests/secure_store_test.cpp" "$ROOT/hopcast/securestore/secure_store.cpp" \
  "$ROOT/hopcast/securestore/key_provider.cpp" "${libs[@]}"
exec "$OUT"
