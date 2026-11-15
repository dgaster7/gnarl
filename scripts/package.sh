#!/bin/bash
# Packages a release: dist/GNARL-macOS.zip, dist/GNARL-content.zip, dist/manifest.json(+.sig).
# Usage: scripts/package.sh <release-version e.g. 1.0.1> <build-dir> [notes-file]
# Signing key: GNARL_SIGNING_KEY env (CI secret) or ~/.gnarl/signing-key.
set -euo pipefail
REL="${1:?release version}"; BUILD="${2:-build}"; NOTES_FILE="${3:-}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PLUGIN_VERSION="$(tr -d '[:space:]' < "$ROOT/VERSION")"
CONTENT_VERSION="$(tr -d '[:space:]' < "$ROOT/CONTENT_VERSION")"
DIST="$ROOT/dist"; rm -rf "$DIST"; mkdir -p "$DIST/stage/VST3" "$DIST/stage/Components" "$DIST/content/Content"
A="$BUILD/src/plugins"
cp -R "$A/GNARL_artefacts/Release/VST3/GNARL.vst3" "$A/GNARL_FX_artefacts/Release/VST3/GNARL FX.vst3" "$DIST/stage/VST3/"
cp -R "$A/GNARL_artefacts/Release/AU/GNARL.component" "$A/GNARL_FX_artefacts/Release/AU/GNARL FX.component" "$DIST/stage/Components/"
for b in "$DIST"/stage/VST3/* "$DIST"/stage/Components/*; do
  codesign --force --deep --sign - --timestamp=none "$b"
  codesign --verify --deep --strict "$b"
  lipo -archs "$b/Contents/MacOS/"* | grep -q arm64
  lipo -archs "$b/Contents/MacOS/"* | grep -q x86_64
done
(cd "$DIST/stage" && zip -qry "$DIST/GNARL-macOS.zip" VST3 Components)
# content: presets + wavetables
mkdir -p "$DIST/content/Content/Presets" "$DIST/content/Content/Wavetables"
cp -R "$ROOT/content/presets/Synth" "$ROOT/content/presets/FX" "$DIST/content/Content/Presets/"
"$BUILD/tools/gnarl_presetgen" --export-wavetables "$DIST/content/Content/Wavetables" >/dev/null
echo "$CONTENT_VERSION" > "$DIST/content/Content/version.txt"
(cd "$DIST/content" && zip -qry "$DIST/GNARL-content.zip" Content)
SIGN="$BUILD/tools/gnarl_sign"
SHA_P="$("$SIGN" sha256 "$DIST/GNARL-macOS.zip")"; SIZE_P=$(stat -f%z "$DIST/GNARL-macOS.zip")
SHA_C="$("$SIGN" sha256 "$DIST/GNARL-content.zip")"; SIZE_C=$(stat -f%z "$DIST/GNARL-content.zip")
NOTES="GNARL $REL"
if [ -n "$NOTES_FILE" ] && [ -f "$NOTES_FILE" ]; then NOTES="$(cat "$NOTES_FILE")"; fi
BASE="https://github.com/dgaster7/gnarl/releases/download/v$REL"
NOTES_JSON=$(printf '%s' "$NOTES" | /usr/bin/osascript -l JavaScript -e 'function run(argv){return JSON.stringify(argv[0])}' "$NOTES")
cat > "$DIST/manifest.json" <<JSON
{
  "version": "$REL",
  "pluginVersion": "$PLUGIN_VERSION",
  "contentVersion": $CONTENT_VERSION,
  "minimumOS": "11.0",
  "releaseNotes": $NOTES_JSON,
  "assets": [
    {"name": "GNARL-macOS.zip", "url": "$BASE/GNARL-macOS.zip", "sha256": "$SHA_P", "size": $SIZE_P},
    {"name": "GNARL-content.zip", "url": "$BASE/GNARL-content.zip", "sha256": "$SHA_C", "size": $SIZE_C}
  ]
}
JSON
if [ -n "${GNARL_SIGNING_KEY:-}" ]; then "$SIGN" sign -env "$DIST/manifest.json"; else "$SIGN" sign "$HOME/.gnarl/signing-key" "$DIST/manifest.json"; fi
"$SIGN" verify "$(grep -o '"[0-9a-f]\{64\}"' "$ROOT/src/app/Updater.h" | head -1 | tr -d '"')" "$DIST/manifest.json"
rm -rf "$DIST/stage" "$DIST/content"
ls -l "$DIST"
