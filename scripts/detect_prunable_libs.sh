set -euo pipefail

APPDIR=AppDir
LIBDIR="$APPDIR/usr/lib"
PLUGINDIR="$APPDIR/usr/plugins"

WORKDIR="$(mktemp -d)"
echo "Workdir: $WORKDIR"

# -----------------------------
# 1. All bundled libs
# -----------------------------
find "$LIBDIR" -maxdepth 1 -type f -name "*.so*" -exec basename {} \; \
  | sort -u > "$WORKDIR/all_libs.txt"

# -----------------------------
# 2. Required libs by ldd (binary + plugins)
# -----------------------------
{
  find "$APPDIR/usr/bin" -type f -executable -exec ldd {} \;
  find "$PLUGINDIR" -type f -name "*.so" -exec ldd {} \;
} \
| awk '/=> \// {print $3}' \
| xargs -n1 basename \
| sort -u > "$WORKDIR/required_by_ldd.txt"

# -----------------------------
# 3. Qt runtime plugin load (REAL runtime deps)
# -----------------------------
export QT_DEBUG_PLUGINS=1
export QT_PLUGIN_PATH="$(realpath "$PLUGINDIR")"
export LD_LIBRARY_PATH="$(realpath "$LIBDIR")"

QT_QPA_PLATFORM=offscreen \
"$APPDIR/usr/bin/Notesman" \
  > /dev/null \
  2> "$WORKDIR/qt_plugins.log" || true

grep -oE 'lib[^/ ]+\.so[^ ]*' "$WORKDIR/qt_plugins.log" \
  | sort -u > "$WORKDIR/required_by_qt.txt"

# -----------------------------
# 4. Hard whitelist (NEVER prune)
# -----------------------------
cat > "$WORKDIR/whitelist.txt" <<'EOF'
^libQt6
^libicu
^libfreetype
^libfontconfig
^libharfbuzz
^libxcb
^libX11
^libXau
^libXdmcp
^libGL
^libEGL
^libOpenGL
^libstdc\+\+
^libgcc_s
^libc\.so
^ld-linux
^libz\.so
^libzstd
^libbz2
^libpng
^libssl
^libcrypto
^libsystemd
EOF

# -----------------------------
# 5. Merge all required libs
# -----------------------------
cat \
  "$WORKDIR/required_by_ldd.txt" \
  "$WORKDIR/required_by_qt.txt" \
| sort -u > "$WORKDIR/required_all.txt"

# -----------------------------
# 6. Compute prune candidates
# -----------------------------
comm -23 "$WORKDIR/all_libs.txt" "$WORKDIR/required_all.txt" \
| grep -Evf "$WORKDIR/whitelist.txt" \
> prune_libs.txt

# -----------------------------
# 7. Report
# -----------------------------
echo "===== SUMMARY ====="
echo "All libs:        $(wc -l < "$WORKDIR/all_libs.txt")"
echo "Required libs:   $(wc -l < "$WORKDIR/required_all.txt")"
echo "Prune candidates: $(wc -l < prune_libs.txt)"

echo "Output: prune_libs.txt"
