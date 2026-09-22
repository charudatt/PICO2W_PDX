#!/usr/bin/env bash
# Bump semantic version and rebuild the user manual.
# Usage: ./bump_version.sh [major|minor|patch] ["changelog message"]
set -euo pipefail
cd "$(dirname "$0")"

PART="${1:-patch}"
MSG="${2:-}"

VER=$(tr -d '[:space:]' < VERSION)
IFS=. read -r MA MI PA <<< "$VER"
MA=${MA:-0}; MI=${MI:-0}; PA=${PA:-0}

case "$PART" in
  major) MA=$((MA+1)); MI=0; PA=0 ;;
  minor) MI=$((MI+1)); PA=0 ;;
  patch) PA=$((PA+1)) ;;
  *) echo "Usage: $0 major|minor|patch [message]"; exit 1 ;;
esac

NEW="${MA}.${MI}.${PA}"
echo "$NEW" > VERSION
TODAY=$(date +%F)

# Prepend changelog entry if message given
if [[ -n "$MSG" ]]; then
  TMP=$(mktemp)
  {
    echo "# PICO2W PDX Step-1 User Manual — Changelog"
    echo
    echo "## [${NEW}] — ${TODAY}"
    echo "### Changed"
    echo "- ${MSG}"
    echo
    # keep previous entries (skip first heading line of old file)
    tail -n +2 CHANGELOG.md
  } > "$TMP"
  mv "$TMP" CHANGELOG.md
else
  TMP=$(mktemp)
  {
    echo "# PICO2W PDX Step-1 User Manual — Changelog"
    echo
    echo "## [${NEW}] — ${TODAY}"
    echo "### Changed"
    echo "- Version bump (${PART})"
    echo
    tail -n +2 CHANGELOG.md
  } > "$TMP"
  mv "$TMP" CHANGELOG.md
fi

echo "VERSION: $VER → $NEW"
python3 build_manual.py
echo "Done."
