#!/bin/bash
# Check 2: pluginval (strictness 10) on all four plugin binaries + auval on both AUs.
# Usage: scripts/validate.sh <artefact-dir-root e.g. build/src/plugins> [config]
#        scripts/validate.sh --installed   (validate the copies in ~/Library/Audio/Plug-Ins)
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PV="$("$ROOT/scripts/get_pluginval.sh")"
STRICT="${PLUGINVAL_STRICTNESS:-10}"
FAIL=0
if [ "${1:-}" = "--installed" ]; then
  VST3S=("$HOME/Library/Audio/Plug-Ins/VST3/GNARL.vst3" "$HOME/Library/Audio/Plug-Ins/VST3/GNARL FX.vst3")
  AUS=("$HOME/Library/Audio/Plug-Ins/Components/GNARL.component" "$HOME/Library/Audio/Plug-Ins/Components/GNARL FX.component")
else
  A="${1:-$ROOT/build/src/plugins}"; CFG="${2:-Release}"
  VST3S=("$A/GNARL_artefacts/$CFG/VST3/GNARL.vst3" "$A/GNARL_FX_artefacts/$CFG/VST3/GNARL FX.vst3")
  AUS=("$A/GNARL_artefacts/$CFG/AU/GNARL.component" "$A/GNARL_FX_artefacts/$CFG/AU/GNARL FX.component")
fi
mkdir -p "$ROOT/build/validation"
# AUs are only visible to pluginval/auval once installed in a Components folder.
if [ "${1:-}" != "--installed" ]; then
  mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"
  NEWAUS=()
  for p in "${AUS[@]}"; do d="$HOME/Library/Audio/Plug-Ins/Components/$(basename "$p")"; rm -rf "$d"; cp -R "$p" "$d"; NEWAUS+=("$d"); done
  AUS=("${NEWAUS[@]}")
  killall -9 AudioComponentRegistrar 2>/dev/null || true
  sleep 2
fi
for p in "${VST3S[@]}" "${AUS[@]}"; do
  name="$(basename "$p")"
  echo "== pluginval --strictness-level $STRICT $name"
  if "$PV" --strictness-level "$STRICT" --validate-in-process --timeout-ms 600000 --output-dir "$ROOT/build/validation" --validate "$p" > "$ROOT/build/validation/$name.log" 2>&1; then
    echo "   PASS"
  else
    echo "   FAIL (see build/validation/$name.log)"; tail -30 "$ROOT/build/validation/$name.log"; FAIL=1
  fi
done
for spec in "aumu Gsyn Gnrl" "aumf Gfxp Gnrl"; do
  echo "== auval -v $spec"
  if auval -strict -v $spec > "$ROOT/build/validation/auval-${spec// /_}.log" 2>&1 && grep -q "AU VALIDATION SUCCEEDED" "$ROOT/build/validation/auval-${spec// /_}.log"; then
    echo "   PASS"
  else
    echo "   FAIL"; tail -30 "$ROOT/build/validation/auval-${spec// /_}.log"; FAIL=1
  fi
done
exit $FAIL
