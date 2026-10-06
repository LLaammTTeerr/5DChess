#!/usr/bin/env bash
# Print the body of one version's section from CHANGELOG.md: the lines between "## [<version>]" and the next "## [" heading
# (or the link references at the end of the file, "[x.y.z]: https://...").
# Used by the release workflow for the GitHub release notes. Exits 1 if the section is missing or empty.
#
# Usage: scripts/changelog_section.sh <version> [CHANGELOG.md]
set -euo pipefail

version="${1:?usage: $0 <version> [CHANGELOG.md]}"
file="${2:-CHANGELOG.md}"

body=$(awk -v ver="$version" '
  index($0, "## [" ver "]") == 1 { found = 1; next }
  found && (/^## \[/ || /^\[[^]]+\]: /) { exit }
  found { print }
' "$file" | sed -e '/./,$!d')   # drop leading blank lines

# trailing blank lines go with the command substitution
if [ -z "${body//[[:space:]]/}" ]; then
  echo "error: no '## [$version]' section (or it is empty) in $file" >&2
  exit 1
fi
printf '%s\n' "$body"
