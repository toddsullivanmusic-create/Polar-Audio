#!/bin/bash
# Polar Audio — installs AURA, GLOW, McGEE and SANCTUARY into Logic Pro (Audio Units) and the VST3 folder.
# Double-click this file. If macOS says it "cannot be opened", right-click → Open once.
cd "$(dirname "$0")"
AU=~/Library/Audio/Plug-Ins/Components
VST=~/Library/Audio/Plug-Ins/VST3
mkdir -p "$AU" "$VST"

echo "Installing Polar Audio plugins..."
for c in Components/*.component; do
    n=$(basename "$c")
    rm -rf "$AU/$n"
    cp -R "$c" "$AU/"
    xattr -cr "$AU/$n" 2>/dev/null
    codesign --force --deep -s - "$AU/$n" 2>/dev/null
    echo "  AU   $n"
done
for v in VST3/*.vst3; do
    n=$(basename "$v")
    rm -rf "$VST/$n"
    cp -R "$v" "$VST/"
    xattr -cr "$VST/$n" 2>/dev/null
    codesign --force --deep -s - "$VST/$n" 2>/dev/null
    echo "  VST3 $n"
done

# Make Logic rescan
killall -9 AudioComponentRegistrar 2>/dev/null
echo
echo "Validating with Apple's auval (this is exactly what Logic runs)..."
for spec in "aufx Aur1" "aufx Glw1" "aumu McG1" "aumu Snc1"; do
    if auval -v $spec Polr 2>&1 | grep -q "PASS"; then echo "  PASS  $spec"; else echo "  FAIL  $spec  (open Logic → Settings → Plug-in Manager → Reset & Rescan)"; fi
done
echo
echo "Done. Open Logic Pro:"
echo "  Effects:     Audio Units → Polar Audio → AURA / GLOW"
echo "  Instruments: AU Instruments → Polar Audio → McGEE / SANCTUARY"
echo "If they don't show up, Logic Pro → Settings → Plug-in Manager → select them → Reset & Rescan Selection."
read -n 1 -s -r -p "Press any key to close."
