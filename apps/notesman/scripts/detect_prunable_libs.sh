#!/usr/bin/env bash
set -euo pipefail

APPDIR=AppDir
LIBDIR="$APPDIR/usr/lib"
PLUGINDIR="$APPDIR/usr/plugins"
WORKDIR="$(mktemp -d)"

find "$LIBDIR" -name "*.so*" -exec realpath {} + | sort -u > "$WORKDIR/all_libs_real.txt"

find "$APPDIR/usr/bin" -type f -executable > "$WORKDIR/entries.txt"
find "$PLUGINDIR" -type f -name "*.so" >> "$WORKDIR/entries.txt"

declare -A REQUIRED_LIBS

resolve_deps() {
    local target="$1"
    local deps=$(readelf -d "$target" 2>/dev/null | awk '/NEEDED/ {print $5}' | tr -d '[]')
    
    for dep in $deps; do
        local fullpath="$LIBDIR/$dep"
        if [[ -f "$fullpath" ]]; then
            local real_fullpath=$(realpath "$fullpath")
            if [[ -z "${REQUIRED_LIBS[$real_fullpath]:-}" ]]; then
                REQUIRED_LIBS["$real_fullpath"]=1
                resolve_deps "$real_fullpath"
            fi
        fi
    done
}

while read -r entry; do
    resolve_deps "$entry"
done < "$WORKDIR/entries.txt"

for lib in "${!REQUIRED_LIBS[@]}"; do
    echo "$lib"
done | sort -u > "$WORKDIR/required_libs_real.txt"

comm -23 "$WORKDIR/all_libs_real.txt" "$WORKDIR/required_libs_real.txt" > "$WORKDIR/prune_candidates_real.txt"

> prune_libs.txt
while read -r path; do
    basename "$path" >> prune_libs.txt
done < "$WORKDIR/prune_candidates_real.txt"

echo "===== SUMMARY ====="
echo "Prune candidates saved to prune_libs.txt"