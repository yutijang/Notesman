#!/usr/bin/env bash
set -euo pipefail

APPDIR=AppDir
LIBDIR="$APPDIR/usr/lib"
PLUGINDIR="$APPDIR/usr/plugins"
WORKDIR="$(mktemp -d)"

# -----------------------------
# 1. Collect all bundled libs
# -----------------------------
find "$LIBDIR" -type f -name '*.so*' -printf '%f\n' | sort -u > "$WORKDIR/all_libs.txt"

# -----------------------------
# 2. Collect entry ELFs
# -----------------------------
find "$APPDIR/usr/bin" -type f -executable > "$WORKDIR/entry_bins.txt"
find "$PLUGINDIR" -type f -name '*.so' >> "$WORKDIR/entry_bins.txt"

# -----------------------------
# 3. Resolve DT_NEEDED recursively
# -----------------------------
declare -A SEEN
QUEUE=()

while read -r f; do
  QUEUE+=("$f")
done < "$WORKDIR/entry_bins.txt"

> "$WORKDIR/required_fullpath.txt"

while ((${#QUEUE[@]})); do
  cur="${QUEUE[0]}"
  QUEUE=("${QUEUE[@]:1}")

  readelf -d "$cur" 2>/dev/null \
    | awk '/NEEDED/ {print $5}' \
    | tr -d '[]' \
    | while read -r dep; do
        path="$LIBDIR/$dep"
        [[ -f "$path" ]] || continue
        [[ -n "${SEEN[$path]:-}" ]] && continue
        SEEN["$path"]=1
        echo "$path" >> "$WORKDIR/required_fullpath.txt"
        QUEUE+=("$path")
      done
done

# -----------------------------
# 4. Normalize required names
# -----------------------------
awk -F/ '{print $NF}' "$WORKDIR/required_fullpath.txt" \
  | sort -u > "$WORKDIR/required_libs.txt"

# -----------------------------
# 5. Prune list = all - required
# -----------------------------
comm -23 "$WORKDIR/all_libs.txt" "$WORKDIR/required_libs.txt" \
  > prune_libs.txt

# -----------------------------
# 6. Report
# -----------------------------
echo "===== SUMMARY ====="
echo "All libs:        $(wc -l < "$WORKDIR/all_libs.txt")"
echo "Required libs:   $(wc -l < "$WORKDIR/required_libs.txt")"
echo "Prune candidates: $(wc -l < prune_libs.txt)"
echo "Output: prune_libs.txt"
