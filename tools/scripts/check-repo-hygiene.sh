#!/usr/bin/env bash
#
# Repo hygiene checks for an Unreal Engine project.
#
# Catches the two mistakes that are painful to undo:
#   1. A binary committed outside Git LFS (permanently bloats every clone).
#   2. Engine-generated output committed (churns the diff, breaks other machines).
#
# Runs in CI and locally. Needs git only -- no engine, no LFS install.
#
#   bash tools/scripts/check-repo-hygiene.sh

set -uo pipefail

RED=$'\033[0;31m'; GREEN=$'\033[0;32m'; YELLOW=$'\033[0;33m'; DIM=$'\033[2m'; OFF=$'\033[0m'
if [[ ! -t 1 && -z "${CI:-}" ]]; then RED=; GREEN=; YELLOW=; DIM=; OFF=; fi

FAILURES=0
LFS_POINTER_PREFIX='version https://git-lfs.github.com/spec/v1'
MAX_BLOB_BYTES=$((5 * 1024 * 1024))   # anything bigger should be in LFS

# Extensions that must always be LFS-backed. Keep in sync with .gitattributes.
BINARY_GLOBS=(
  '*.uasset' '*.umap' '*.ubulk' '*.uexp'
  '*.fbx' '*.blend' '*.obj' '*.glb' '*.abc' '*.psd'
  '*.png' '*.jpg' '*.jpeg' '*.tga' '*.tif' '*.tiff' '*.exr' '*.hdr' '*.dds' '*.bmp' '*.gif'
  '*.wav' '*.mp3' '*.ogg' '*.flac' '*.mp4' '*.mov' '*.webm'
  '*.ttf' '*.otf'
)

pass() { printf '%s  ok  %s %s\n' "$GREEN" "$OFF" "$1"; }
fail() { printf '%s FAIL %s %s\n' "$RED" "$OFF" "$1"; FAILURES=$((FAILURES + 1)); }
warn() { printf '%s warn %s %s\n' "$YELLOW" "$OFF" "$1"; }
note() { printf '%s       %s%s\n' "$DIM" "$1" "$OFF"; }

git rev-parse --git-dir >/dev/null 2>&1 || { echo "not a git repository"; exit 1; }

echo "Repo hygiene"
echo "------------"

# --- 1. .gitattributes must exist and route uasset/umap to LFS -----------------
if [[ ! -f .gitattributes ]]; then
  fail ".gitattributes is missing"
else
  missing_rules=()
  for ext in uasset umap; do
    git check-attr filter -- "probe.$ext" | grep -q 'filter: lfs' || missing_rules+=(".$ext")
  done
  if ((${#missing_rules[@]})); then
    fail ".gitattributes does not route ${missing_rules[*]} through LFS"
  else
    pass ".gitattributes routes Unreal assets through LFS"
  fi
fi

# --- 2. Every tracked binary must be stored as an LFS pointer ------------------
non_lfs=()
while IFS= read -r -d '' file; do
  header=$(git cat-file blob ":$file" 2>/dev/null | head -c ${#LFS_POINTER_PREFIX})
  [[ "$header" == "$LFS_POINTER_PREFIX" ]] || non_lfs+=("$file")
done < <(git ls-files -z -- "${BINARY_GLOBS[@]}" 2>/dev/null)

if ((${#non_lfs[@]})); then
  fail "${#non_lfs[@]} binary file(s) committed outside Git LFS"
  printf '%s\n' "${non_lfs[@]}" | head -20 | sed 's/^/         /'
  ((${#non_lfs[@]} > 20)) && note "... and $(( ${#non_lfs[@]} - 20 )) more"
  note "fix: git lfs track \"*.ext\" && git add .gitattributes, then re-add the files"
  note "already pushed? git lfs migrate import --include=\"*.ext\" --everything (rewrites history)"
else
  pass "all tracked binaries are LFS pointers"
fi

# --- 3. No oversized non-LFS blobs --------------------------------------------
big=()
while IFS= read -r line; do
  size=${line%% *}; path=${line#* }
  (( size > MAX_BLOB_BYTES )) && big+=("$(( size / 1024 / 1024 ))MB  $path")
done < <(
  git ls-files -s |
    awk -F'	' '{ split($1, meta, " "); print meta[2], $2 }' |
    while read -r sha path; do
      printf '%s %s\n' "$(git cat-file -s "$sha" 2>/dev/null || echo 0)" "$path"
    done
)

if ((${#big[@]})); then
  fail "${#big[@]} file(s) over 5MB stored outside LFS"
  printf '%s\n' "${big[@]}" | sed 's/^/         /'
else
  pass "no oversized non-LFS blobs"
fi

# --- 4. No engine-generated directories tracked -------------------------------
generated=$(git ls-files | grep -E '(^|/)(Binaries|Intermediate|Saved|DerivedDataCache)/' || true)
if [[ -n "$generated" ]]; then
  fail "engine-generated files are tracked"
  printf '%s\n' "$generated" | head -20 | sed 's/^/         /'
  note "fix: git rm -r --cached <dir> (they are already in .gitignore)"
else
  pass "no engine-generated directories tracked"
fi

# --- 5. Unreal JSON files must parse ------------------------------------------
json_files=$(git ls-files -- '*.uproject' '*.uplugin' '*.json' | grep -v node_modules || true)
if [[ -z "$json_files" ]]; then
  note "no .uproject/.uplugin/.json files yet -- skipping JSON validation"
elif command -v python3 >/dev/null 2>&1; then
  bad_json=()
  while IFS= read -r f; do
    python3 -c 'import json,sys; json.load(open(sys.argv[1], encoding="utf-8-sig"))' "$f" 2>/dev/null || bad_json+=("$f")
  done <<< "$json_files"
  if ((${#bad_json[@]})); then
    fail "invalid JSON: ${bad_json[*]}"
  else
    pass "all JSON files parse"
  fi
else
  warn "python3 not found -- skipping JSON validation"
fi

# --- 6. Case-collision check (Windows/macOS clone breaker) --------------------
collisions=$(git ls-files | tr 'A-Z' 'a-z' | sort | uniq -d || true)
if [[ -n "$collisions" ]]; then
  fail "paths that differ only by case -- these break clones on Windows/macOS"
  printf '%s\n' "$collisions" | sed 's/^/         /'
else
  pass "no case-only path collisions"
fi

echo "------------"
if (( FAILURES )); then
  printf '%s%d check(s) failed%s\n' "$RED" "$FAILURES" "$OFF"
  exit 1
fi
printf '%sall checks passed%s\n' "$GREEN" "$OFF"
