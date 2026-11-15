#!/bin/bash
# GNARL installer: downloads the latest release, verifies it, installs both plugins (VST3 + AU)
# into your user plug-in folders and the presets/wavetables into ~/Library/Application Support/GNARL.
# Safe to run as often as you like. No admin password needed.
#   curl -fsSL https://raw.githubusercontent.com/dgaster7/gnarl/main/install.sh | bash
set -euo pipefail
REPO="dgaster7/gnarl"
say() { printf '\033[1;32m==>\033[0m %s\n' "$*"; }
die() { printf '\033[1;31mError:\033[0m %s\n' "$*" >&2; exit 1; }
[ "$(uname)" = "Darwin" ] || die "GNARL is for macOS."
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
json() { /usr/bin/osascript -l JavaScript -e "function run(a){var o=JSON.parse(a[0]);var v=eval('o'+a[1]);return v===undefined?'':String(v)}" "$(cat "$TMP/manifest.json")" "$1"; }

say "Downloading the latest GNARL release information"
curl -fsSL -o "$TMP/manifest.json" "https://github.com/$REPO/releases/latest/download/manifest.json" || die "Could not reach GitHub. Check your internet connection."
VERSION="$(json '.version')"
say "Latest version: $VERSION"

fetch() { # name -> verified file
  local name="$1" url sha
  for i in 0 1 2 3; do
    [ "$(json ".assets[$i] && o.assets[$i].name")" = "$name" ] || continue
    url="$(json ".assets[$i].url")"; sha="$(json ".assets[$i].sha256")"
  done
  [ -n "${url:-}" ] || die "Release has no $name"
  say "Downloading $name"
  curl -fsSL -o "$TMP/$name" "$url" || die "Download failed: $name"
  [ "$(shasum -a 256 "$TMP/$name" | cut -d' ' -f1)" = "$sha" ] || die "$name failed its SHA-256 check. Nothing was installed."
  say "$name verified (SHA-256 matches the release manifest)"
}
fetch "GNARL-macOS.zip"
fetch "GNARL-content.zip"

VST3="$HOME/Library/Audio/Plug-Ins/VST3"; COMP="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3" "$COMP" "$TMP/p" "$TMP/c"
ditto -x -k "$TMP/GNARL-macOS.zip" "$TMP/p"
ditto -x -k "$TMP/GNARL-content.zip" "$TMP/c"
for b in "GNARL.vst3" "GNARL FX.vst3"; do rm -rf "$VST3/$b"; mv "$TMP/p/VST3/$b" "$VST3/"; done
for b in "GNARL.component" "GNARL FX.component"; do rm -rf "$COMP/$b"; mv "$TMP/p/Components/$b" "$COMP/"; done
APP="$HOME/Library/Application Support/GNARL"
mkdir -p "$APP"
rm -rf "$APP/Content.old"; [ -d "$APP/Content" ] && mv "$APP/Content" "$APP/Content.old"
mv "$TMP/c/Content" "$APP/Content" && rm -rf "$APP/Content.old"
say "Installed presets and wavetables (your User presets are untouched)"

for b in "$VST3/GNARL.vst3" "$VST3/GNARL FX.vst3" "$COMP/GNARL.component" "$COMP/GNARL FX.component"; do
  xattr -dr com.apple.quarantine "$b" 2>/dev/null || true
  codesign --verify --deep --strict "$b" || die "Signature check failed for $b"
done
say "Code signatures verified, quarantine flags cleared"

killall -9 AudioComponentRegistrar 2>/dev/null || true
sleep 2
for spec in "aumu Gsyn Gnrl" "aumf Gfxp Gnrl"; do
  if auval -v $spec > "$TMP/auval.log" 2>&1 && grep -q "AU VALIDATION SUCCEEDED" "$TMP/auval.log"; then say "auval passed: $spec"
  else tail -20 "$TMP/auval.log"; die "auval failed for $spec"; fi
done

cat <<MSG

GNARL $VERSION is installed:
  $VST3/GNARL.vst3
  $VST3/GNARL FX.vst3
  $COMP/GNARL.component
  $COMP/GNARL FX.component

Next: open your DAW and rescan plug-ins (see the README for Ableton Live, Bitwig and Logic).
Updates: GNARL checks once a day; you can also click Check now in its SETTINGS tab.
MSG
