#!/bin/bash
# Downloads pluginval (pinned) into .cache/pluginval and prints the binary path.
set -euo pipefail
VER=v1.0.4
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DIR="$ROOT/.cache/pluginval-$VER"
BIN="$DIR/pluginval.app/Contents/MacOS/pluginval"
if [ ! -x "$BIN" ]; then
  mkdir -p "$DIR"
  curl -fsSL -o "$DIR/pv.zip" "https://github.com/Tracktion/pluginval/releases/download/$VER/pluginval_macOS.zip"
  (cd "$DIR" && unzip -q -o pv.zip && rm pv.zip)
  xattr -dr com.apple.quarantine "$DIR" 2>/dev/null || true
fi
echo "$BIN"
