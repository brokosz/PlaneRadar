#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# export_firmware.sh
#
# After you do  Sketch → Export Compiled Binary  in Arduino IDE 2, run this
# script to copy the merged binary into docs/firmware/ so the web installer
# can serve it.
#
# Usage:
#   chmod +x scripts/export_firmware.sh
#   ./scripts/export_firmware.sh
# ─────────────────────────────────────────────────────────────────────────────

set -e

SKETCH_DIR="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$SKETCH_DIR/docs/firmware/PlaneRadar.bin"

echo "Looking for compiled binary..."

# Arduino IDE 2 puts build output here (adjust if your board/fqbn differs)
FQBN_DIR="esp32s3.esp32.esp32s3"
BUILD_DIR="$SKETCH_DIR/build/$FQBN_DIR"
MERGED="$BUILD_DIR/PlaneRadar.ino.merged.bin"

if [ -f "$MERGED" ]; then
    echo "Found merged binary: $MERGED"
    cp "$MERGED" "$DEST"
    SIZE=$(du -sh "$DEST" | cut -f1)
    echo "✓ Copied to docs/firmware/PlaneRadar.bin ($SIZE)"
    echo ""
    echo "Next steps:"
    echo "  1. git add docs/firmware/PlaneRadar.bin"
    echo "  2. git commit -m 'firmware: update to vX.Y.Z'"
    echo "  3. git push"
    echo "  4. Your web installer at https://brokosz.github.io/PlaneRadar will serve the new firmware"
    exit 0
fi

# Fallback: try to find the merged binary anywhere under build/
echo "Searching build/ for merged binary..."
FOUND=$(find "$SKETCH_DIR/build" -name "*.merged.bin" 2>/dev/null | head -1)
if [ -n "$FOUND" ]; then
    echo "Found: $FOUND"
    cp "$FOUND" "$DEST"
    echo "✓ Copied to docs/firmware/PlaneRadar.bin"
    exit 0
fi

# If no merged binary exists, offer to create one with esptool
echo ""
echo "No merged binary found. Make sure you ran Sketch → Export Compiled Binary first."
echo ""
echo "If the merged binary is missing, you can create it manually:"
echo ""
echo "  # Find esptool (bundled with Arduino IDE):"
echo "  ESPTOOL=\$(find ~/Library/Arduino15/packages/esp32 -name 'esptool.py' 2>/dev/null | head -1)"
echo ""
echo "  # Merge (adjust paths to match your build output):"
echo "  python3 \"\$ESPTOOL\" --chip esp32s3 merge_bin \\"
echo "    --output docs/firmware/PlaneRadar.bin \\"
echo "    0x0     build/\$FQBN_DIR/PlaneRadar.ino.bootloader.bin \\"
echo "    0x8000  build/\$FQBN_DIR/PlaneRadar.ino.partitions.bin \\"
echo "    0xe000  \$(find ~/Library/Arduino15 -name 'boot_app0.bin' | head -1) \\"
echo "    0x10000 build/\$FQBN_DIR/PlaneRadar.ino.bin"
echo ""
exit 1
