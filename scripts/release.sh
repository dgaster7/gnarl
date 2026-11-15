#!/bin/bash
# One-command release. Usage:
#   scripts/release.sh <version> "<release notes>"          plugin (+ content) release, e.g. 1.0.1
#   scripts/release.sh --content-only "<release notes>"     presets/wavetables only (bumps CONTENT_VERSION)
#   add --content to a plugin release to also bump CONTENT_VERSION
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; cd "$ROOT"
[ -z "$(git status --porcelain)" ] || { echo "Working tree is not clean; commit first."; exit 1; }
[ "$(git rev-parse --abbrev-ref HEAD)" = "main" ] || { echo "Release from main."; exit 1; }
MODE="plugin"; BUMP_CONTENT=0
if [ "${1:-}" = "--content-only" ]; then MODE="content"; shift; NOTES="${1:-Content update}"; else VER="${1:?version}"; shift; NOTES="${1:-GNARL $VER}"; shift || true; [ "${1:-}" = "--content" ] && BUMP_CONTENT=1; fi
if [ "$MODE" = "content" ] || [ $BUMP_CONTENT = 1 ]; then echo $(( $(tr -d '[:space:]' < CONTENT_VERSION) + 1 )) > CONTENT_VERSION; fi
if [ "$MODE" = "plugin" ]; then
  echo "$VER" > VERSION; TAG="v$VER"
else
  VER="$(tr -d '[:space:]' < VERSION)"; TAG="v$VER-c$(tr -d '[:space:]' < CONTENT_VERSION)"
fi
git rev-parse "$TAG" >/dev/null 2>&1 && { echo "Tag $TAG already exists."; exit 1; }
# saved-state fixtures for this release (compatibility contract)
if [ "$MODE" = "plugin" ]; then
  cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null
  cmake --build build-release --target gnarl_app_tests >/dev/null
  GNARL_WRITE_FIXTURES="tests/fixtures/$VER" "build-release/tests/gnarl_app_tests_artefacts/Release/gnarl_app_tests" "[.make-fixtures]" >/dev/null
fi
git add -A
git commit -q --allow-empty -m "Release $TAG" -m "$NOTES"
git tag -a "$TAG" -m "$NOTES"
git push -q origin main "$TAG"
echo "Pushed $TAG. Waiting for the release workflow..."
sleep 15
RUN_ID="$(gh run list --workflow Release --limit 1 --json databaseId -q '.[0].databaseId')"
gh run watch "$RUN_ID" --exit-status
gh release view "$TAG" --json assets -q '.assets[].name'
echo "Released $TAG: https://github.com/dgaster7/gnarl/releases/tag/$TAG"
