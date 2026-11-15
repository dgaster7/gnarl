#!/bin/bash
# End-to-end proof of the in-plugin updater, run against the copies installed in ~/Library/Audio/Plug-Ins.
# The headless host loads the INSTALLED plugin with GNARL_UPDATER_SELFTEST set: that plugin instance's own
# Updater checks GitHub, verifies the signed manifest and hashes, and installs the update (or rolls back).
#   scripts/update_e2e.sh update      # installed version -> latest release
#   scripts/update_e2e.sh rollback    # swap installed <-> backup (no network)
#   scripts/update_e2e.sh status      # print installed versions, recall the 1.0.0 fixtures
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HOST="${GNARL_HOST:-$ROOT/build/tools/gnarl_host_artefacts/Release/gnarl_host}"
VST3="$HOME/Library/Audio/Plug-Ins/VST3"
RES="$(mktemp)"
case "${1:-status}" in
  update)
    HOST_GNARL_UPDATER_SELFTEST="$RES" "$HOST" info "$VST3/GNARL.vst3"; echo "--- updater (inside the plugin):"; cat "$RES" ;;
  rollback)
    HOST_GNARL_UPDATE_MANIFEST_URL="https://invalid.invalid/manifest.json" HOST_GNARL_UPDATER_SELFTEST="$RES" HOST_GNARL_UPDATER_SELFTEST_ROLLBACK=1 "$HOST" info "$VST3/GNARL.vst3"
    echo "--- updater (inside the plugin):"; cat "$RES" ;;
esac
echo "--- fresh host process (what a DAW sees after restarting):"
"$HOST" info "$VST3/GNARL.vst3"; "$HOST" info "$VST3/GNARL FX.vst3"
for f in synth-2 fx-2; do
  p="$VST3/GNARL.vst3"; [[ $f == fx* ]] && p="$VST3/GNARL FX.vst3"
  "$HOST" recall "$p" "$ROOT/tests/fixtures/1.0.0/$f.state" "$ROOT/tests/fixtures/1.0.0/$f.expect.json"
done
"$HOST" render "$VST3/GNARL.vst3" /dev/null
